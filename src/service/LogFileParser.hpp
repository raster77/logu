#ifndef SERVICE_LOGFILEPARSER_HPP_
#define SERVICE_LOGFILEPARSER_HPP_

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "../model/LogEntry.hpp"
#include "TimestampFormat.hpp"

namespace logutils {

  /**
   * @brief Result of parsing one file.
   */
  struct ParseResult {
      /** The parsed entries, in file order. */
      std::vector<LogEntry> entries;
      /**
       * Name of the timestamp format detected for this file, or
       * {@code std::nullopt} if no line matched any format (e.g. an empty
       * file or one with no recognized timestamps).
       */
      std::optional<std::string> detectedFormat;
  };

  /**
   * @brief Abstraction over "turn a file on disk into log entries".
   *
   * Lets the rest of the app depend on this interface rather than a concrete
   * file-reading strategy (e.g. to substitute a test double, or a non-mmap
   * implementation for platforms where mmap is unavailable).
   */
  class ILogParser {
    public:
      virtual ~ILogParser() = default;
      /**
       * @brief Parses the file at {@code path} into log entries.
       * @param path path to the log file (transparently decompressed if it ends in {@code .gz}).
       * @return the parse result.
       */
      virtual ParseResult parse(const std::string &path) const = 0;
  };

  /**
   * @brief {@link ILogParser} backed by a memory-mapped file.
   *
   * Logs can be large, so a file is memory-mapped (via mio) rather than read
   * through an {@code ifstream}: the OS pages in only the ranges we actually
   * touch, and there's no intermediate stream buffer copy. {@code .gz} files
   * are the exception: they're read fully into memory via zlib, since a
   * compressed file can't be interpreted as log lines without inflating it first.
   *
   * The caveat of mapping a file that's still being written: appending to it
   * is harmless (the mapping just doesn't see the new bytes), but *truncating*
   * it -- log rotation with copytruncate, say -- while it's mapped makes
   * touching the vanished pages raise SIGBUS, which no try/catch can turn
   * into a skipped file. It's a narrow window (only while a file is actually
   * being parsed) and the alternative costs a full copy of every log on every
   * read, so this trades that risk for the speed; worth knowing about if
   * {@code --working-dir} is ever pointed at a directory under aggressive rotation.
   */
  class MmapLogParser : public ILogParser {
    public:
      /**
       * @brief Constructs a parser that detects timestamps against {@code formats}.
       * @param formats the catalog to detect and match timestamps with; must
       * outlive this parser.
       */
      explicit MmapLogParser(const TimestampFormatCatalog &formats) : mFormats(formats) {
      }

      ParseResult parse(const std::string &path) const override;

    private:
      ParseResult parseBuffer(const char *data, std::size_t size) const;

      const TimestampFormatCatalog &mFormats;
  };

} // namespace logutils

#endif /* SERVICE_LOGFILEPARSER_HPP_ */
