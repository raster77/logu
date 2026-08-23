#include "Testing.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include <zlib.h>

#include "service/LogFileParser.hpp"
#include "service/TimestampFormat.hpp"
#include "util/DirectoryScan.hpp"
#include "util/TextUtil.hpp"

using namespace logutils;

namespace {

  namespace fs = std::filesystem;

  // A directory under the system temp dir, removed when the test ends
  // (including when a CHECK fails and the test keeps running).
  class TempDir {
    public:
      TempDir() : mPath(fs::temp_directory_path() / uniqueName()) {
        fs::create_directories(mPath);
      }

      ~TempDir() {
        std::error_code ignored;
        fs::permissions(mPath, fs::perms::owner_all, fs::perm_options::add, ignored);
        fs::remove_all(mPath, ignored);
      }

      TempDir(const TempDir &) = delete;
      TempDir &operator=(const TempDir &) = delete;

      const fs::path &path() const {
        return mPath;
      }

      fs::path write(const std::string &name, const std::string &content) const {
        const fs::path file = mPath / name;
        fs::create_directories(file.parent_path());
        std::ofstream out(file, std::ios::binary);
        out << content;
        return file;
      }

    private:
      static std::string uniqueName() {
        static int counter = 0;
        return "logu-tests-" + std::to_string(::getpid()) + "-" + std::to_string(counter++);
      }

      fs::path mPath;
  };

  const TimestampFormatCatalog &catalog() {
    static const TimestampFormatCatalog formats({
        TimestampFormat("logback", "yyyy-MM-dd HH:mm:ss.SSS"),
        TimestampFormat("plain", "yyyy-MM-dd HH:mm:ss"),
    });
    return formats;
  }

} // namespace

TEST(textUtilHelpers) {
  CHECK_EQ(text_util::toLower("MiXed 123 ÉÀ"), "mixed 123 ÉÀ"); // ASCII only, length-preserving
  CHECK(text_util::iequals("Quit", "qUIT"));
  CHECK(!text_util::iequals("quit", "quits"));
  CHECK(text_util::icontains("a Connection Refused b", "connection refused"));
  CHECK(text_util::icontains("anything", "")); // empty needle matches
  CHECK(!text_util::icontains("short", "much longer needle"));
  CHECK(text_util::icontains("aaab", "aab")); // restart after a partial match
}

TEST(parserGroupsContinuationLines) {
  const TempDir dir;
  const auto file = dir.write("app.log",
                              "2026-08-06 14:23:01.100 INFO first\n"
                              "\tat com.example.Thing.run(Thing.java:42)\n"
                              "\tat com.example.Other.call(Other.java:7)\n"
                              "2026-08-06 14:23:02.200 INFO second\n");

  const MmapLogParser parser(catalog());
  const ParseResult result = parser.parse(file.string());

  CHECK(result.detectedFormat.has_value());
  CHECK_EQ(*result.detectedFormat, "logback");
  CHECK_EQ(result.entries.size(), static_cast<std::size_t>(2));
  CHECK_EQ(result.entries[0].lines.size(), static_cast<std::size_t>(3));
  CHECK_EQ(result.entries[0].sortKey, "2026-08-06 14:23:01.100000000");
  CHECK_EQ(result.entries[1].lines.size(), static_cast<std::size_t>(1));
}

TEST(parserHandlesEdgeCasesOfLineEndings) {
  const TempDir dir;
  // CRLF, no trailing newline on the last line, and leading text with no
  // timestamp at all.
  const auto file = dir.write("crlf.log",
                              "banner line\r\n"
                              "2026-08-06 14:23:01.100 INFO first\r\n"
                              "2026-08-06 14:23:02.200 INFO last");

  const MmapLogParser parser(catalog());
  const ParseResult result = parser.parse(file.string());

  CHECK_EQ(result.entries.size(), static_cast<std::size_t>(3));
  // The banner keeps an empty key, so it sorts first rather than being lost.
  CHECK_EQ(result.entries[0].sortKey, "");
  CHECK_EQ(result.entries[0].lines[0], "banner line");
  CHECK_EQ(result.entries[1].lines[0], "2026-08-06 14:23:01.100 INFO first");
  CHECK_EQ(result.entries[2].lines[0], "2026-08-06 14:23:02.200 INFO last");
}

TEST(parserAcceptsAnEmptyFileAndRejectsAMissingOne) {
  const TempDir dir;
  const auto empty = dir.write("empty.log", "");

  const MmapLogParser parser(catalog());
  const ParseResult result = parser.parse(empty.string());
  CHECK(result.entries.empty());
  CHECK(!result.detectedFormat.has_value());

  bool threw = false;
  try {
    parser.parse((dir.path() / "does-not-exist.log").string());
  } catch (const std::runtime_error &) {
    threw = true;
  }
  CHECK(threw);
}

TEST(parserDecompressesGzipTransparently) {
  const TempDir dir;
  const fs::path file = dir.path() / "app.log.gz";

  gzFile out = gzopen(file.string().c_str(), "wb");
  CHECK(out != nullptr);
  if (out != nullptr) {
    const std::string content =
        "2026-08-06 14:23:01.100 INFO compressed\n"
        "\tat Thing.java:1\n";
    gzwrite(out, content.data(), static_cast<unsigned>(content.size()));
    gzclose(out);
  }

  const MmapLogParser parser(catalog());
  const ParseResult result = parser.parse(file.string());

  CHECK_EQ(result.entries.size(), static_cast<std::size_t>(1));
  CHECK_EQ(result.entries[0].lines.size(), static_cast<std::size_t>(2));
  CHECK_EQ(result.entries[0].lines[0], "2026-08-06 14:23:01.100 INFO compressed");
}

TEST(directoryScanWalksEveryDepth) {
  const TempDir dir;
  dir.write("a.log", "x");
  dir.write("nested/b.log", "x");
  dir.write("nested/deeper/c.log", "x");

  std::vector<std::string> found;
  const std::size_t skipped = dir_scan::forEachRegularFile(
      dir.path(), [&found](const fs::directory_entry &entry) {
        found.push_back(entry.path().filename().string());
      });

  CHECK_EQ(skipped, static_cast<std::size_t>(0));
  CHECK_EQ(found.size(), static_cast<std::size_t>(3));
}

TEST(directoryScanReportsWhatItCouldNotRead) {
  const TempDir dir;
  dir.write("readable.log", "x");
  const fs::path locked = dir.path() / "locked";
  fs::create_directories(locked);
  dir.write("locked/hidden.log", "x");

  std::error_code ec;
  fs::permissions(locked, fs::perms::none, ec);
  if (ec) {
    return; // running as a user that can't restrict permissions (e.g. root)
  }

  std::vector<std::string> found;
  const std::size_t skipped = dir_scan::forEachRegularFile(
      dir.path(), [&found](const fs::directory_entry &entry) {
        found.push_back(entry.path().filename().string());
      });

  // The unreadable directory is reported, and the rest of the tree is still
  // walked -- the whole reason this isn't recursive_directory_iterator.
  CHECK_EQ(skipped, static_cast<std::size_t>(1));
  CHECK_EQ(found.size(), static_cast<std::size_t>(1));
  CHECK_EQ(found[0], "readable.log");

  fs::permissions(locked, fs::perms::owner_all, ec);
}

TEST(directoryScanDoesNotFollowDirectorySymlinks) {
  const TempDir dir;
  dir.write("real/a.log", "x");

  std::error_code ec;
  fs::create_directory_symlink(dir.path() / "real", dir.path() / "link", ec);
  if (ec) {
    return; // no symlink support here
  }

  std::size_t count = 0;
  const std::size_t skipped = dir_scan::forEachRegularFile(
      dir.path(), [&count](const fs::directory_entry &) { ++count; });

  CHECK_EQ(skipped, static_cast<std::size_t>(0));
  // Once through "real", not a second time through the symlink.
  CHECK_EQ(count, static_cast<std::size_t>(1));
}
