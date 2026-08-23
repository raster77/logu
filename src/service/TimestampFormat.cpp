#include "TimestampFormat.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include <nlohmann/json.hpp>

#include "../util/TextUtil.hpp"

namespace logutils {

  namespace {

    struct MonthNameEntry {
        std::string_view text;
        int number;
    };

    // Literal, lowercase month-name spellings MMM matches, tried longest-first
    // (see monthNameEntries() below) so e.g. French "mars" isn't cut short by
    // English "mar". Two families: the English 3-letter abbreviations, and the
    // French ones glibc's %b produces under fr_FR (some already 3-4 letters
    // with no trailing dot -- mars, mai, juin, août -- others dotted). Matching
    // is ASCII-only case-insensitive (see text_util::iequals), so an accented
    // character only matches when it's already lower-case in the line, same as
    // every log this tool has seen it in.
    constexpr std::array<MonthNameEntry, 24> MONTH_NAME_ENTRIES = {{
        {"jan", 1}, {"feb", 2}, {"mar", 3}, {"apr", 4}, {"may", 5}, {"jun", 6},
        {"jul", 7}, {"aug", 8}, {"sep", 9}, {"oct", 10}, {"nov", 11}, {"dec", 12},
        {"janv.", 1}, {"févr.", 2}, {"mars", 3}, {"avr.", 4}, {"mai", 5}, {"juin", 6},
        {"juil.", 7}, {"août", 8}, {"sept.", 9}, {"oct.", 10}, {"nov.", 11}, {"déc.", 12},
    }};

    // MONTH_NAME_ENTRIES ordered longest-name-first, computed once: matching
    // greedily against the longest candidate that fits is what lets "mars"
    // (4 chars) win over the 3-char prefix "mar" would otherwise match, with
    // no backtracking needed since every candidate is a fixed literal.
    const std::vector<MonthNameEntry> &monthNameEntriesByLength() {
      static const std::vector<MonthNameEntry> sorted = [] {
        std::vector<MonthNameEntry> entries(MONTH_NAME_ENTRIES.begin(), MONTH_NAME_ENTRIES.end());
        std::stable_sort(entries.begin(), entries.end(), [](const MonthNameEntry &a, const MonthNameEntry &b) {
          return a.text.size() > b.text.size();
        });
        return entries;
      }();
      return sorted;
    }

    // Weekday-name spellings EEE matches -- same two families and same
    // longest-first matching reasoning as MONTH_NAME_ENTRIES above. The
    // weekday carries no information the sortKey needs (the date fields
    // already order entries correctly, and cross-checking a wrong weekday
    // against the date isn't this tool's job), so it's matched only to be
    // consumed, never stored.
    constexpr std::array<std::string_view, 14> WEEKDAY_NAME_ENTRIES = {{
        "mon", "tue", "wed", "thu", "fri", "sat", "sun",
        "lun.", "mar.", "mer.", "jeu.", "ven.", "sam.", "dim.",
    }};

    const std::vector<std::string_view> &weekdayNameEntriesByLength() {
      static const std::vector<std::string_view> sorted = [] {
        std::vector<std::string_view> entries(WEEKDAY_NAME_ENTRIES.begin(), WEEKDAY_NAME_ENTRIES.end());
        std::stable_sort(entries.begin(), entries.end(),
            [](std::string_view a, std::string_view b) { return a.size() > b.size(); });
        return entries;
      }();
      return sorted;
    }

  } // namespace

  TimestampFormat::TimestampFormat(std::string name, const std::string &pattern)
  : mName(std::move(name)), mTokens(compile(pattern)) {
    bool hasMonth = false, hasDay = false;
    bool hasHour = false, hasMinute = false, hasSecond = false;

    for (const Token &token : mTokens) {
      if (token.isLiteral) {
        continue;
      }

      switch (token.field) {
        case Field::Year: break;
        case Field::Month: hasMonth = true; break;
        case Field::Day: hasDay = true; break;
        case Field::Hour: hasHour = true; break;
        case Field::Hour12: hasHour = true; break;
        case Field::Minute: hasMinute = true; break;
        case Field::Second: hasSecond = true; break;
        case Field::Fraction: break;
        case Field::Weekday: break;
        case Field::AmPm: break;
      }
    }

    if (!(hasMonth && hasDay && hasHour && hasMinute && hasSecond)) {
      throw std::invalid_argument("timestamp pattern \"" + pattern +
                                  "\" must specify month, day, hour, minute and second");
    }
  }

  std::vector<TimestampFormat::Token> TimestampFormat::compile(const std::string &pattern) {
    struct Keyword {
        std::string_view text;
        Field field;
        bool isMonthName;
    };

    // Longest/most specific keywords first so "MMM" is tried before "MM".
    static constexpr Keyword KEYWORDS[] = {
        {"yyyy", Field::Year, false},
        {"EEE", Field::Weekday, false},
        {"MMM", Field::Month, true},
        {"MM", Field::Month, false},
        {"dd", Field::Day, false},
        {"HH", Field::Hour, false},
        {"hh", Field::Hour12, false},
        {"mm", Field::Minute, false},
        {"ss", Field::Second, false},
        {"SSS", Field::Fraction, false},
        {"a", Field::AmPm, false},
    };

    std::vector<Token> tokens;
    std::size_t i = 0;

    while (i < pattern.size()) {
      bool matched = false;

      for (const Keyword &keyword : KEYWORDS) {
        if (pattern.compare(i, keyword.text.size(), keyword.text) == 0) {
          tokens.push_back({false, '\0', keyword.field, keyword.isMonthName});
          i += keyword.text.size();
          matched = true;
          break;
        }
      }

      if (!matched) {
        tokens.push_back({true, pattern[i], Field::Year, false});
        ++i;
      }
    }

    return tokens;
  }

  std::optional<std::string> TimestampFormat::match(const std::string &line, std::size_t *matchedLength) const {
    std::size_t pos = 0;
    const std::size_t size = line.size();

    // Fixed offsets of each field within the "YYYY-MM-DD HH:MM:SS" layout
    // sortKey always has, regardless of the order fields appear in the
    // pattern -- written into directly instead of building/concatenating a
    // std::string per field, since that's a guaranteed heap allocation per
    // line once the result crosses libstdc++'s 15-char SSO threshold.
    constexpr int YEAR_OFFSET = 0;
    constexpr int MONTH_OFFSET = 5;
    constexpr int DAY_OFFSET = 8;
    constexpr int HOUR_OFFSET = 11;
    constexpr int MINUTE_OFFSET = 14;
    constexpr int SECOND_OFFSET = 17;
    constexpr int DATE_TIME_LEN = 19;
    // Fractions are captured up to this many digits and, in the sortKey,
    // padded with trailing zeros to exactly this width. Both halves have to
    // agree, hence the shared constant.
    //
    // The padding is what makes two timestamps at the same instant written
    // with different precisions compare equal -- ".500" from a Logback file
    // and ".500000000" from an RFC3339Nano container log. Comparing raw
    // fractions is otherwise already correct (the digits are positional, so
    // ".123" < ".5" lexicographically as well as numerically), but a shorter
    // fraction is a *prefix* of an equal longer one, and a prefix always
    // sorts first -- which silently decided the order by digit count instead
    // of leaving it to LogMerger's stable sort (i.e. input file order).
    constexpr std::size_t FRACTION_DIGITS = 9;

    char buf[DATE_TIME_LEN];

    buf[4] = '-';
    buf[7] = '-';
    buf[10] = ' ';
    buf[13] = ':';
    buf[16] = ':';
    // A pattern with no year token (see the constructor -- year is optional)
    // never writes YEAR_OFFSET..+3 below, so default it here rather than
    // leaving those bytes uninitialized. "0000" sorts ahead of every dated
    // entry, which is the least surprising placement for a log that never
    // says what year it's from.
    buf[YEAR_OFFSET] = '0';
    buf[YEAR_OFFSET + 1] = '0';
    buf[YEAR_OFFSET + 2] = '0';
    buf[YEAR_OFFSET + 3] = '0';

    bool hasFraction = false;
    std::size_t fractionStart = 0;
    std::size_t fractionLen = 0;

    // hh is written to buf[HOUR_OFFSET] only after the whole pattern has been
    // read, since a trailing "a" token (AM/PM) can still change it -- an
    // hh/a pair may appear in either order in a pattern.
    bool hasHour12 = false;
    int hour12Value = 0;
    bool hasAmPm = false;
    bool isPm = false;

    for (const Token &token : mTokens) {
      if (token.isLiteral) {
        if (pos >= size || line[pos] != token.literal) {
          return std::nullopt;
        }

        ++pos;

        continue;
      }

      if (token.field == Field::Weekday) {
        std::size_t matchedLen = 0;

        for (std::string_view candidate : weekdayNameEntriesByLength()) {
          if (pos + candidate.size() > size) {
            continue;
          }

          if (text_util::iequals(std::string_view(line).substr(pos, candidate.size()), candidate)) {
            matchedLen = candidate.size();
            break;
          }
        }

        if (matchedLen == 0) {
          return std::nullopt;
        }

        pos += matchedLen;
        continue;
      }

      if (token.field == Field::AmPm) {
        if (pos + 2 > size) {
          return std::nullopt;
        }

        const std::string_view candidate(line.data() + pos, 2);
        if (text_util::iequals(candidate, "AM")) {
          isPm = false;
        } else if (text_util::iequals(candidate, "PM")) {
          isPm = true;
        } else {
          return std::nullopt;
        }

        hasAmPm = true;
        pos += 2;
        continue;
      }

      if (token.field == Field::Hour12) {
        if (pos + 2 > size || !std::isdigit(static_cast<unsigned char>(line[pos])) ||
            !std::isdigit(static_cast<unsigned char>(line[pos + 1]))) {
          return std::nullopt;
        }

        hour12Value = (line[pos] - '0') * 10 + (line[pos + 1] - '0');
        hasHour12 = true;
        pos += 2;
        continue;
      }

      if (token.field == Field::Fraction) {
        const std::size_t start = pos;

        // Consumes every fractional digit, not just the first FRACTION_DIGITS
        // of them. Stopping at nine used to leave any further digits in the
        // line for the *next* token to match, so a pattern with a literal
        // after SSS -- "[MMM dd HH:mm:ss.SSS]", say -- simply failed to match
        // a timestamp written with more precision than that, and the line was
        // folded into the previous entry. Only the first nine reach the
        // sortKey; the rest are consumed and dropped, which also keeps
        // matchedLength covering the whole timestamp for the viewer's
        // colouring.
        while (pos < size && std::isdigit(static_cast<unsigned char>(line[pos]))) {
          ++pos;
        }

        if (pos == start) {
          return std::nullopt;
        }

        hasFraction = true;
        fractionStart = start;
        fractionLen = std::min(pos - start, FRACTION_DIGITS);
        continue;
      }

      if (token.isMonthName) {
        // Compared in place, longest candidate first: substr + toLower would
        // allocate two strings per line, in a function whose whole point is
        // to allocate nothing until the sortKey at the end.
        int monthNum = 0;
        std::size_t matchedLen = 0;
        for (const MonthNameEntry &candidate : monthNameEntriesByLength()) {
          if (pos + candidate.text.size() > size) {
            continue;
          }
          if (text_util::iequals(std::string_view(line).substr(pos, candidate.text.size()), candidate.text)) {
            monthNum = candidate.number;
            matchedLen = candidate.text.size();
            break;
          }
        }

        if (monthNum == 0) {
          return std::nullopt;
        }

        buf[MONTH_OFFSET] = static_cast<char>('0' + monthNum / 10);
        buf[MONTH_OFFSET + 1] = static_cast<char>('0' + monthNum % 10);

        pos += matchedLen;
        continue;
      }

      const int width = (token.field == Field::Year) ? 4 : 2;

      if (pos + static_cast<std::size_t>(width) > size) {
        return std::nullopt;
      }

      for (int k = 0; k < width; ++k) {
        if (!std::isdigit(static_cast<unsigned char>(line[pos + k]))) {
          return std::nullopt;
        }
      }

      int offset = 0;

      switch (token.field) {
        case Field::Year: offset = YEAR_OFFSET; break;
        case Field::Month: offset = MONTH_OFFSET; break;
        case Field::Day: offset = DAY_OFFSET; break;
        case Field::Hour: offset = HOUR_OFFSET; break;
        case Field::Minute: offset = MINUTE_OFFSET; break;
        case Field::Second: offset = SECOND_OFFSET; break;
        case Field::Fraction: break;
        case Field::Weekday: break;
        case Field::Hour12: break;
        case Field::AmPm: break;
      }

      std::copy(line.data() + pos, line.data() + pos + width, buf + offset);

      pos += width;
    }

    if (matchedLength) {
      *matchedLength = pos;
    }

    if (hasHour12) {
      // A bare "hh" with no "a" in the same pattern is written through
      // unadjusted (mod 12, so "12" reads as hour 0) rather than treated as
      // an error -- ambiguous, but no less usable than any other yearless or
      // fraction-less format this tool already accepts.
      int hour24 = hour12Value % 12;

      if (hasAmPm && isPm) {
        hour24 += 12;
      }

      buf[HOUR_OFFSET] = static_cast<char>('0' + hour24 / 10);
      buf[HOUR_OFFSET + 1] = static_cast<char>('0' + hour24 % 10);
    }

    std::string sortKey;
    sortKey.reserve(hasFraction ? DATE_TIME_LEN + 1 + FRACTION_DIGITS : DATE_TIME_LEN);
    sortKey.append(buf, DATE_TIME_LEN);

    if (hasFraction) {
      sortKey += '.';
      sortKey.append(line, fractionStart, fractionLen);
      sortKey.append(FRACTION_DIGITS - fractionLen, '0');
    }
    // A line whose format carries no fraction at all keeps a bare
    // "…:01" key, so it still sorts ahead of every fractional entry within
    // that second. Deliberate: there's no sub-second information to place it
    // any better, and padding it to ".000000000" would instead interleave it
    // with entries actually logged at .000.

    return sortKey;
  }

  TimestampFormatCatalog::TimestampFormatCatalog(std::vector<TimestampFormat> formats)
    : mFormats(std::move(formats)) {
  }

  namespace {

    // Kept in sync with the repo's formats.json, so behavior is identical
    // whether that file is present or not.
    std::vector<TimestampFormat> defaultFormats() {
      std::vector<TimestampFormat> formats;
      formats.emplace_back("logback", "yyyy-MM-dd HH:mm:ss.SSS");
      formats.emplace_back("logback-iso", "yyyy-MM-ddTHH:mm:ss,SSS");
      formats.emplace_back("logback-comma", "yyyy-MM-dd HH:mm:ss,SSS");
      formats.emplace_back("iso8601", "yyyy-MM-ddTHH:mm:ss.SSS");
      formats.emplace_back("iso8601-nofrac", "yyyy-MM-ddTHH:mm:ss");
      formats.emplace_back("apache", "dd/MMM/yyyy:HH:mm:ss.SSS");
      formats.emplace_back("apache-common", "dd/MMM/yyyy:HH:mm:ss");
      formats.emplace_back("nginx", "yyyy/MM/dd HH:mm:ss");
      formats.emplace_back("plain", "yyyy-MM-dd HH:mm:ss");
      formats.emplace_back("vmtoolsd", "[MMM dd HH:mm:ss.SSS]");
      formats.emplace_back("syslog", "MMM dd HH:mm:ss");
      formats.emplace_back("redis", "dd MMM yyyy HH:mm:ss.SSS");
      formats.emplace_back("oracle", "dd-MMM-yyyy HH:mm:ss");
      formats.emplace_back("ctime", "EEE MMM dd HH:mm:ss yyyy");
      formats.emplace_back("http-date", "EEE, dd MMM yyyy HH:mm:ss");
      formats.emplace_back("us-12h", "MM/dd/yyyy hh:mm:ss a");
      return formats;
    }

  } // namespace

  TimestampFormatCatalog TimestampFormatCatalog::loadOrDefault(const std::string &path, bool verbose) {
    std::ifstream in(path);
    if (!in && verbose) {
      // Not an error: the built-in defaults are kept identical to the repo's
      // formats.json, so a missing file changes nothing. Still worth saying
      // under --verbose, because the default path is relative to the working
      // directory -- run from elsewhere, a catalog the user did edit is
      // quietly not the one in use.
      std::cerr << "note: " << path << " not found; using the built-in timestamp formats\n";
    }

    if (in) {
      try {
        nlohmann::json doc;
        in >> doc;
        std::vector<TimestampFormat> formats;

        for (const auto &entry : doc) {
          formats.emplace_back(
              entry.at("name").get<std::string>(), entry.at("pattern").get<std::string>());
        }

        if (formats.empty()) {
          std::cerr << "warning: " << path << " defines no timestamp formats; using built-in defaults\n";
        } else {
          return TimestampFormatCatalog(std::move(formats));
        }
      } catch (const std::exception &e) {
        std::cerr << "warning: could not read timestamp formats from " << path << " (" << e.what()
                           << "); using built-in defaults\n";
      }
    }

    return TimestampFormatCatalog(defaultFormats());
  }

  std::optional<std::size_t> TimestampFormatCatalog::detect(
      const std::vector<std::string> &sample) const {
    // One vote per line, cast for the *first* format that matches it, rather
    // than a tally of every format that could.
    //
    // The difference matters because the catalog contains formats that are
    // prefixes of each other: "plain" (yyyy-MM-dd HH:mm:ss) matches every
    // line "logback" (the same plus .SSS) matches, since a pattern only has
    // to match the start of the line. Tallying every format therefore gave
    // "plain" a score at least equal to "logback"'s on any Logback file, and
    // strictly higher as soon as one line lacked milliseconds -- a startup
    // banner was enough to switch a whole file to "plain" and drop the
    // fraction from every sortKey it produced. Letting each line vote once,
    // for the most specific format that fits it, makes the majority of
    // ordinary lines decide.
    std::vector<std::size_t> votes(mFormats.size(), 0);

    for (const std::string &line : sample) {
      for (std::size_t i = 0; i < mFormats.size(); ++i) {
        if (mFormats[i].match(line)) {
          ++votes[i];
          break;
        }
      }
    }

    std::optional<std::size_t> best;
    std::size_t bestCount = 0;

    for (std::size_t i = 0; i < votes.size(); ++i) {
      // Strictly greater, so an earlier format wins a tie -- including the
      // one-matching-line case, which keeps "tried in order, first match
      // wins" true for files too short or too uniform to vote.
      if (votes[i] > bestCount) {
        bestCount = votes[i];
        best = i;
      }
    }

    return best;
  }

  std::optional<std::string> TimestampFormatCatalog::match(const std::string &line,
      std::optional<std::size_t> &knownIndex, std::size_t *matchedLength) const {
    if (knownIndex) {
      return mFormats[*knownIndex].match(line, matchedLength);
    }

    for (std::size_t i = 0; i < mFormats.size(); ++i) {
      if (auto result = mFormats[i].match(line, matchedLength)) {
        knownIndex = i;
        return result;
      }
    }

    return std::nullopt;
  }

} // namespace logutils
