#include "LogFileParser.hpp"

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <vector>

#include <mio.hpp>
#include <zlib.h>

#include "../util/TextUtil.hpp"

namespace logutils {

  namespace {

    bool hasGzipExtension(const std::string &path) {
      return path.size() >= 3 && text_util::toLower(path.substr(path.size() - 3)) == ".gz";
    }

    // Reads a whole ".gz" file into memory, decompressed. zlib's gz* API
    // transparently handles the gzip header/trailer (and multi-member
    // streams), so this is just a read loop.
    std::string readGzipFile(const std::string &path) {
      gzFile file = gzopen(path.c_str(), "rb");

      if (!file) {
        throw std::runtime_error("could not open file: " + path);
      }

      std::string buffer;
      // On the heap rather than a 64 KiB automatic array: harmless on a
      // default 8 MiB stack, but this also runs on the --working-dir reload
      // worker, and a buffer that large is not something to have to reason
      // about per thread.
      constexpr unsigned chunkSize = 1 << 16;
      std::vector<char> chunk(chunkSize);
      int bytesRead = 0;

      while ((bytesRead = gzread(file, chunk.data(), chunkSize)) > 0) {
        buffer.append(chunk.data(), static_cast<std::size_t>(bytesRead));
      }

      int errorCode = 0;
      const std::string errorMessage = (bytesRead < 0) ? gzerror(file, &errorCode) : std::string();

      gzclose(file);

      if (bytesRead < 0) {
        throw std::runtime_error("could not read file: " + path + " (" + errorMessage + ")");
      }

      return buffer;
    }

  } // namespace

  ParseResult MmapLogParser::parse(const std::string &path) const {
    if (hasGzipExtension(path)) {
      const std::string buffer = readGzipFile(path);
      return parseBuffer(buffer.data(), buffer.size());
    }

    std::error_code sizeError;
    const auto fileSize = std::filesystem::file_size(path, sizeError);

    if (sizeError) {
      throw std::runtime_error("could not open file: " + path + " (" + sizeError.message() + ")");
    }

    // mio requires a non-empty file to map; an empty log simply has no entries.
    if (fileSize == 0) {
      return {};
    }

    std::error_code mapError;
    mio::mmap_source mmap = mio::make_mmap_source(path, mapError);

    if (mapError) {
      throw std::runtime_error("could not open file: " + path + " (" + mapError.message() + ")");
    }

    return parseBuffer(mmap.data(), mmap.size());
  }

  namespace {

    // Extracts the line starting at `pos` and advances `pos` past its
    // newline. A trailing '\r' is stripped so CRLF-terminated files aren't
    // left with it.
    std::string nextLine(const char *data, std::size_t size, std::size_t &pos) {
      std::size_t eol = pos;

      while (eol < size && data[eol] != '\n') {
        ++eol;
      }

      std::size_t lineLen = eol - pos;

      if (lineLen > 0 && data[pos + lineLen - 1] == '\r') {
        --lineLen;
      }

      std::string line(data + pos, lineLen);

      pos = eol + 1;
      return line;
    }

    // How many lines from the start of a file to vote on when deciding its
    // timestamp format. Comfortably larger than a plausible leading stack
    // trace or thread dump, so a file whose first entry happens to carry a
    // long one still gets to vote on real entries rather than on trace
    // lines. Costs one extra pass over a bounded prefix (formats x lines
    // match attempts), which is negligible next to parsing the file itself.
    constexpr std::size_t kDetectionSampleLines = 2000;

    // Votes on the file's timestamp format across its first
    // kDetectionSampleLines lines instead of locking onto whichever format
    // the first matching line happens to fit. std::nullopt if no format
    // matched any of them -- in which case parseBuffer falls back to
    // detecting from the first line that matches anywhere in the file, so
    // logs whose timestamps only start later still work.
    std::optional<std::size_t> detectFormat(
        const TimestampFormatCatalog &formats, const char *data, std::size_t size) {
      std::vector<std::string> sample;
      std::size_t pos = 0;

      while (pos < size && sample.size() < kDetectionSampleLines) {
        sample.push_back(nextLine(data, size, pos));
      }

      return formats.detect(sample);
    }

  } // namespace

  ParseResult MmapLogParser::parseBuffer(const char *data, std::size_t size) const {
    std::vector<LogEntry> entries;
    std::optional<std::size_t> formatIndex = detectFormat(mFormats, data, size);
    std::size_t pos = 0;

    while (pos < size) {
      std::string line = nextLine(data, size, pos);
      std::size_t timestampLength = 0;
      std::optional<std::string> sortKey = mFormats.match(line, formatIndex, &timestampLength);

      if (sortKey) {
        entries.push_back({std::move(*sortKey), {std::move(line)}, timestampLength});
      } else if (!entries.empty()) {
        entries.back().lines.push_back(std::move(line));
      } else {
        // Leading lines with no recognized timestamp: kept as-is, sorted
        // first.
        entries.push_back({"", {std::move(line)}});
      }
    }

    std::optional<std::string> detectedFormat;
    if (formatIndex) {
      detectedFormat = mFormats.name(*formatIndex);
    }

    return {std::move(entries), std::move(detectedFormat)};
  }

} // namespace logutils
