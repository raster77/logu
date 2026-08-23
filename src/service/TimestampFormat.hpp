#ifndef SERVICE_TIMESTAMPFORMAT_HPP_
#define SERVICE_TIMESTAMPFORMAT_HPP_

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace logutils {

  /**
   * @brief A single timestamp pattern compiled from a
   * Logback/SimpleDateFormat-style pattern string, e.g.
   * {@code "yyyy-MM-dd HH:mm:ss.SSS"} or {@code "dd/MMM/yyyy:HH:mm:ss.SSS"}.
   *
   * Recognized tokens: {@code yyyy} (4-digit year), {@code MM} (2-digit
   * month), {@code MMM} (3-letter month name, case-insensitive), {@code dd},
   * {@code HH}, {@code mm}, {@code ss} (2 digits each), {@code SSS} (1-9
   * variable-length fractional-second digits, normalized to 9 in the
   * sortKey), {@code EEE} (weekday name, case-insensitive, matched and
   * discarded -- it carries no information a sortKey needs), {@code hh}
   * (2-digit 12-hour clock hour) and {@code a} (AM/PM designator; combined
   * with {@code hh} to produce the 24-hour value written to the sortKey). An
   * {@code hh} with no {@code "a"} token in the same pattern is still reduced
   * mod 12 -- {@code "12"} reads as hour 0, {@code "13"} as hour 1 -- since
   * the token means a 12-hour clock whether or not the designator that would
   * disambiguate it is present. Ambiguous, but no less usable than any other
   * yearless or fraction-less format this tool already accepts; use
   * {@code HH} for a 24-hour clock.
   *
   * Any other character is a literal that must match exactly. Hand-rolled
   * instead of {@code std::regex}, same reasoning as the original
   * Logback-only matcher: regex costs several microseconds per line even on
   * a match, which dominates parse time at typical log volumes.
   */
  class TimestampFormat {
    public:
      /**
       * @brief Compiles a named format from a Logback/SimpleDateFormat-style pattern.
       * @param name the format's display name (returned by {@link #name}).
       * @param pattern the pattern string, per the class documentation above.
       * @throws std::invalid_argument if {@code pattern} doesn't specify all
       * of year/month/day/hour/minute/second (a fraction, weekday, and AM/PM
       * designator are optional). Either {@code HH} or {@code hh} satisfies
       * the hour requirement.
       */
      TimestampFormat(std::string name, const std::string &pattern);

      /** @return this format's display name. */
      const std::string &name() const {
        return mName;
      }

      /**
       * @brief Matches {@code line} against this format's pattern.
       *
       * The fraction is zero-padded to a fixed width so precisions can be
       * mixed across files without the digit count deciding the order -- see
       * {@code match()}'s {@code FRACTION_DIGITS}.
       *
       * @param line the line to match, tested from its start.
       * @param matchedLength if non-null and the match succeeds, receives the
       * length of the raw timestamp text at the start of {@code line} (used
       * by the viewer to color just that span rather than re-detecting it
       * per frame).
       * @return the normalized, directly-comparable sortKey (e.g.
       * {@code "2026-08-06 14:23:01.123000000"}) if {@code line} starts with
       * this format's pattern, or {@code std::nullopt} otherwise.
       */
      std::optional<std::string> match(const std::string &line, std::size_t *matchedLength = nullptr) const;

    private:
      enum class Field { Year, Month, Day, Hour, Minute, Second, Fraction, Weekday, Hour12, AmPm };

      struct Token {
          bool isLiteral;
          char literal;
          Field field;
          bool isMonthName;
      };

      static std::vector<Token> compile(const std::string &pattern);

      std::string mName;
      std::vector<Token> mTokens;
  };

  /**
   * @brief An ordered set of formats: a line is tried against each in turn
   * and the first match wins, so more specific or more likely formats should
   * be listed first.
   */
  class TimestampFormatCatalog {
    public:
      /**
       * @brief Constructs a catalog from an already-built format list, in match order.
       * @param formats the formats to try, most specific/likely first.
       */
      explicit TimestampFormatCatalog(std::vector<TimestampFormat> formats);

      /**
       * @brief Loads formats from a JSON file, falling back to defaults on any problem.
       *
       * The file is an array of {@code {"name": ..., "pattern": ...}} objects.
       * Falls back to the built-in defaults (Logback/Spring Boot, its ISO
       * variant, and the Apache common/combined log format) if the file
       * doesn't exist, isn't valid JSON, defines no formats, or defines a
       * format missing a required field -- printing a warning to stderr in
       * the latter two cases, and, when {@code verbose}, a note in the first
       * (the default path is relative to the working directory, so "not
       * found" can simply mean "run from somewhere else").
       *
       * @param path path to the format catalog JSON file.
       * @param verbose whether to print a note to stderr when falling back
       * because the file wasn't found.
       * @return the loaded catalog, or the built-in defaults on failure.
       */
      static TimestampFormatCatalog loadOrDefault(const std::string &path, bool verbose = false);

      /**
       * @brief Matches {@code line} against a known or yet-to-be-determined format.
       *
       * Matches {@code line} against the format at {@code knownIndex} if set;
       * otherwise tries every format in order and, on the first match, writes
       * the winning index into {@code knownIndex}, so later calls for the
       * same file can skip straight to it instead of re-trying every format.
       * Pass a fresh ({@code std::nullopt}) {@code knownIndex} per file: a
       * file is assumed to use one consistent timestamp format throughout,
       * matching this tool's assumption that a file's entries all come from
       * the same application/logger.
       *
       * @param line the line to match.
       * @param knownIndex in/out: the format index already determined for
       * this file, or {@code std::nullopt} to try every format.
       * @param matchedLength see {@link TimestampFormat#match}.
       * @return the matched sortKey, or {@code std::nullopt} if no format matches.
       */
      std::optional<std::string> match(const std::string &line, std::optional<std::size_t> &knownIndex,
          std::size_t *matchedLength = nullptr) const;

      /**
       * @brief Picks the format that best fits a sample of lines.
       *
       * Deciding on a sample rather than on the single first matching line is
       * what stops one atypical line from locking in the wrong format for a
       * whole file: a header without milliseconds matches the less specific
       * "plain" in an otherwise-"logback" file, and locking onto it would
       * leave every later line unrecognized and folded into that first entry.
       *
       * @param sample lines to test each format against.
       * @return the index of the format matching the most lines of
       * {@code sample}, or {@code std::nullopt} if none matches any of them.
       * Ties go to the earlier format, so listing order still decides
       * between formats that fit a file equally well.
       */
      std::optional<std::size_t> detect(const std::vector<std::string> &sample) const;

      /**
       * @brief The name of the format at {@code index}, as returned via
       * {@code knownIndex} by {@link #match}.
       * @param index index into this catalog's formats.
       * @return the format's display name.
       */
      const std::string &name(std::size_t index) const {
        return mFormats[index].name();
      }

    private:
      std::vector<TimestampFormat> mFormats;
  };

} // namespace logutils

#endif /* SERVICE_TIMESTAMPFORMAT_HPP_ */
