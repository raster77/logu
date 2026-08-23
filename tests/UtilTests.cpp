#include "Testing.hpp"

#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <system_error>

#include "util/DirectoryScan.hpp"
#include "util/PathUtil.hpp"
#include "util/TempFile.hpp"
#include "util/TextUtil.hpp"

using namespace logutils;

namespace {

  namespace fs = std::filesystem;

  // setenv/unsetenv are POSIX and unavailable on Windows; _putenv_s covers
  // both (an empty value removes the variable).
  void setEnvVar(const char *name, const char *value) {
#ifdef _WIN32
    _putenv_s(name, value != nullptr ? value : "");
#else
    if (value != nullptr) {
      ::setenv(name, value, 1);
    } else {
      ::unsetenv(name);
    }
#endif
  }

  // Restores an environment variable to whatever it was, so a test that has to
  // move HOME doesn't leak that into the tests that run after it.
  class ScopedEnv {
    public:
      ScopedEnv(const char *name, const char *value) : mName(name) {
        if (const char *previous = std::getenv(name)) {
          mHad = true;
          mPrevious = previous;
        }
        setEnvVar(name, value);
      }

      ~ScopedEnv() {
        setEnvVar(mName.c_str(), mHad ? mPrevious.c_str() : nullptr);
      }

      ScopedEnv(const ScopedEnv &) = delete;
      ScopedEnv &operator=(const ScopedEnv &) = delete;

    private:
      std::string mName;
      bool mHad = false;
      std::string mPrevious;
  };

} // namespace

// --- text_util::icontainsLower ---------------------------------------------

TEST(icontainsLowerMatchesRegardlessOfHaystackCase) {
  CHECK(text_util::icontainsLower("Connection REFUSED by peer", "refused"));
  CHECK(text_util::icontainsLower("ERROR", "error"));
  CHECK(!text_util::icontainsLower("connection accepted", "refused"));
}

// The whole point of the separate entry point is that it skips folding the
// needle, so it must agree with icontains() on every already-lowered needle --
// otherwise LogDocument's filters would quietly start missing lines.
TEST(icontainsLowerAgreesWithIcontainsOnLoweredNeedles) {
  const char *haystacks[] = {"", "a", "ERROR at 12:00", "no match here", "MiXeD CaSe NeEdLe"};
  const char *needles[] = {"", "a", "error", "mixed case", "12:00", "zzz"};

  for (const char *hay : haystacks) {
    for (const char *needle : needles) {
      const std::string lowered = text_util::toLower(needle);
      CHECK_EQ(text_util::icontainsLower(hay, lowered), text_util::icontains(hay, lowered));
    }
  }
}

TEST(icontainsLowerHandlesEmptyAndOversizedNeedles) {
  CHECK(text_util::icontainsLower("anything", ""));
  CHECK(!text_util::icontainsLower("short", "much longer needle"));
}

// --- path_util::expandUser -------------------------------------------------

TEST(expandUserExpandsLeadingTilde) {
  ScopedEnv home("HOME", "/home/tester");
  CHECK_EQ(path_util::expandUser("~/out.log").string(), std::string("/home/tester/out.log"));
  CHECK_EQ(path_util::expandUser("~").string(), std::string("/home/tester"));
  CHECK_EQ(path_util::expandUser("~/a/b/c.log").string(), std::string("/home/tester/a/b/c.log"));
}

TEST(expandUserLeavesEverythingElseAlone) {
  ScopedEnv home("HOME", "/home/tester");
  // Absolute and relative paths pass through untouched.
  CHECK_EQ(path_util::expandUser("/tmp/out.log").string(), std::string("/tmp/out.log"));
  CHECK_EQ(path_util::expandUser("out.log").string(), std::string("out.log"));
  CHECK_EQ(path_util::expandUser("").string(), std::string(""));
  // A tilde that isn't leading is an ordinary character.
  CHECK_EQ(path_util::expandUser("logs/~backup.log").string(), std::string("logs/~backup.log"));
  // "~user" needs a passwd lookup this deliberately doesn't do.
  CHECK_EQ(path_util::expandUser("~other/out.log").string(), std::string("~other/out.log"));
}

TEST(expandUserLeavesTildeAloneWithNoResolvableHome) {
  ScopedEnv home("HOME", nullptr);
  ScopedEnv userProfile("USERPROFILE", nullptr);
  ScopedEnv homeDrive("HOMEDRIVE", nullptr);
  ScopedEnv homePath("HOMEPATH", nullptr);
  CHECK_EQ(path_util::expandUser("~/out.log").string(), std::string("~/out.log"));
}

// --- temp_file::createPrivate ----------------------------------------------

TEST(createPrivateCreatesTheFileItNames) {
  const fs::path path = temp_file::createPrivate("logu-test-", ".log");
  std::error_code ignored;

  CHECK(fs::exists(path));
  CHECK(fs::is_regular_file(path, ignored));
  CHECK_EQ(fs::file_size(path, ignored), static_cast<std::uintmax_t>(0));
  CHECK_EQ(path.extension().string(), std::string(".log"));
  CHECK(path.filename().string().rfind("logu-test-", 0) == 0);

  fs::remove(path, ignored);
}

#ifndef _WIN32
// The reason this helper exists rather than a bare std::ofstream: the temp
// directory is world-writable, so the merged log must not land there readable
// by every other user on the machine.
TEST(createPrivateCreatesAnOwnerOnlyFile) {
  const fs::path path = temp_file::createPrivate("logu-test-", ".log");
  std::error_code ignored;

  const fs::perms mode = fs::status(path, ignored).permissions();
  CHECK((mode & fs::perms::owner_read) != fs::perms::none);
  CHECK((mode & fs::perms::owner_write) != fs::perms::none);
  CHECK((mode & fs::perms::group_all) == fs::perms::none);
  CHECK((mode & fs::perms::others_all) == fs::perms::none);

  fs::remove(path, ignored);
}
#endif

// The old clock-derived name collided whenever two runs started in the same
// tick; unique names are the property that replaced it.
TEST(createPrivateReturnsDistinctPathsAcrossCalls) {
  std::set<std::string> seen;
  std::vector<fs::path> created;

  for (int i = 0; i < 16; ++i) {
    const fs::path path = temp_file::createPrivate("logu-test-", ".log");
    CHECK(seen.insert(path.string()).second);
    created.push_back(path);
  }

  std::error_code ignored;
  for (const fs::path &path : created) {
    fs::remove(path, ignored);
  }
}

// --- dir_scan::FileExclusion -----------------------------------------------

TEST(fileExclusionMatchesTheOutputFileOnly) {
  const fs::path dir = temp_file::createPrivate("logu-test-dir-", "");
  std::error_code ignored;
  fs::remove(dir, ignored);
  fs::create_directories(dir);

  const fs::path output = dir / "merged.log";
  const fs::path other = dir / "app.log";
  std::ofstream(output) << "out\n";
  std::ofstream(other) << "in\n";

  const dir_scan::FileExclusion exclusion(output.string());
  CHECK(exclusion.matches(fs::directory_entry(output)));
  CHECK(!exclusion.matches(fs::directory_entry(other)));

  fs::remove_all(dir, ignored);
}

// A spelling of the same file that isn't the same string still has to be
// recognized -- that is why this compares by identity, not by path text.
TEST(fileExclusionMatchesADifferentSpellingOfTheSameFile) {
  const fs::path dir = temp_file::createPrivate("logu-test-dir-", "");
  std::error_code ignored;
  fs::remove(dir, ignored);
  fs::create_directories(dir);

  const fs::path output = dir / "merged.log";
  std::ofstream(output) << "out\n";

  const dir_scan::FileExclusion exclusion((dir / "." / "merged.log").string());
  CHECK(exclusion.matches(fs::directory_entry(output)));

  fs::remove_all(dir, ignored);
}

TEST(fileExclusionWithNoOutputPathExcludesNothing) {
  const fs::path dir = temp_file::createPrivate("logu-test-dir-", "");
  std::error_code ignored;
  fs::remove(dir, ignored);
  fs::create_directories(dir);

  const fs::path file = dir / "app.log";
  std::ofstream(file) << "in\n";

  const dir_scan::FileExclusion exclusion("");
  CHECK(!exclusion.matches(fs::directory_entry(file)));

  fs::remove_all(dir, ignored);
}
