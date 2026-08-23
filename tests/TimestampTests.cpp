#include "Testing.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "model/LogEntry.hpp"
#include "service/LogEntryDeduplicator.hpp"
#include "service/LogMerger.hpp"
#include "service/TimestampFormat.hpp"

using namespace logutils;

namespace {

  const TimestampFormat &logback() {
    static const TimestampFormat format("logback", "yyyy-MM-dd HH:mm:ss.SSS");
    return format;
  }

  std::string keyOf(const TimestampFormat &format, const std::string &line) {
    const auto key = format.match(line);
    return key ? *key : std::string("(no match)");
  }

  LogEntry entry(std::string sortKey, std::vector<std::string> lines) {
    return LogEntry{std::move(sortKey), std::move(lines)};
  }

} // namespace

TEST(timestampMatchesAndNormalizes) {
  CHECK_EQ(keyOf(logback(), "2026-08-06 14:23:01.123 INFO hello"), "2026-08-06 14:23:01.123000000");
  CHECK_EQ(keyOf(logback(), "no timestamp here"), "(no match)");
  // A line that stops short of the full pattern must not match on a prefix.
  CHECK_EQ(keyOf(logback(), "2026-08-06 14:23"), "(no match)");
  // The fraction needs at least one digit.
  CHECK_EQ(keyOf(logback(), "2026-08-06 14:23:01. INFO"), "(no match)");
}

TEST(fractionIsPaddedToAFixedWidth) {
  // The point of the padding: same instant, three different precisions, one
  // key -- so ordering falls through to LogMerger's stable sort instead of
  // being decided by digit count.
  const TimestampFormat iso("iso8601", "yyyy-MM-ddTHH:mm:ss.SSS");
  const std::string milli = keyOf(logback(), "2026-08-06 14:23:01.500 x");
  const std::string micro = keyOf(iso, "2026-08-06T14:23:01.500000 x");
  const std::string nano = keyOf(iso, "2026-08-06T14:23:01.500000000Z x");

  CHECK_EQ(milli, micro);
  CHECK_EQ(micro, nano);
  // Still ordered correctly against a different instant in the same second.
  CHECK(keyOf(logback(), "2026-08-06 14:23:01.499 x") < milli);
  CHECK(milli < keyOf(logback(), "2026-08-06 14:23:01.501 x"));

  // At most nine digits reach the key; any further ones are consumed and
  // dropped rather than left in the line (see the regression test below).
  CHECK_EQ(keyOf(logback(), "2026-08-06 14:23:01.1234567890123 x"), "2026-08-06 14:23:01.123456789");

  // A format with no fraction keeps a bare key, and so sorts ahead of every
  // fractional entry in the same second.
  const TimestampFormat plain("plain", "yyyy-MM-dd HH:mm:ss");
  CHECK(keyOf(plain, "2026-08-06 14:23:01 x") < keyOf(logback(), "2026-08-06 14:23:01.000 x"));
}

TEST(monthNamesAreMatchedCaseInsensitively) {
  const TimestampFormat apache("apache", "dd/MMM/yyyy:HH:mm:ss");
  CHECK_EQ(keyOf(apache, "20/May/2026:08:17:43 GET /"), "2026-05-20 08:17:43");
  CHECK_EQ(keyOf(apache, "20/MAY/2026:08:17:43 GET /"), "2026-05-20 08:17:43");
  CHECK_EQ(keyOf(apache, "01/dec/2026:08:17:43 GET /"), "2026-12-01 08:17:43");
  // "mai" is French for May, also recognized (see frenchMonthNamesAreMatched
  // below), so this now matches too instead of failing.
  CHECK_EQ(keyOf(apache, "20/Mai/2026:08:17:43 GET /"), "2026-05-20 08:17:43");
}

TEST(frenchMonthNamesAreMatched) {
  const TimestampFormat apache("apache", "dd/MMM/yyyy:HH:mm:ss");
  CHECK_EQ(keyOf(apache, "26/juin/2026:21:07:22 GET /"), "2026-06-26 21:07:22");
  // A dotted abbreviation (longer than its 3-letter English lookalike, if
  // any) is matched whole, not cut short.
  CHECK_EQ(keyOf(apache, "05/sept./2026:21:07:22 GET /"), "2026-09-05 21:07:22");
  CHECK_EQ(keyOf(apache, "01/déc./2026:21:07:22 GET /"), "2026-12-01 21:07:22");
  // "mars" (French, 4 chars) must win over "mar" (English, 3 chars): the
  // literal "/" right after the month name only matches at the 4-char
  // boundary here.
  CHECK_EQ(keyOf(apache, "01/mars/2026:21:07:22 GET /"), "2026-03-01 21:07:22");
}

TEST(yearIsOptionalAndSortsBeforeAnyDatedEntry) {
  // No "yyyy" token at all -- year defaults to "0000".
  const TimestampFormat vmtoolsd("vmtoolsd", "[MMM dd HH:mm:ss.SSS]");
  CHECK_EQ(keyOf(vmtoolsd, "[juin 26 21:07:22.467] [ warning] [guestinfo] Failed to get disk info."),
      "0000-06-26 21:07:22.467000000");
  CHECK(keyOf(vmtoolsd, "[juin 26 21:07:22.467] x") <
      keyOf(logback(), "2026-08-06 14:23:01.123 INFO hello"));
}

TEST(weekdayIsMatchedAndDiscarded) {
  const TimestampFormat ctime("ctime", "EEE MMM dd HH:mm:ss yyyy");
  CHECK_EQ(keyOf(ctime, "Wed Aug 21 12:34:56 2026 x"), "2026-08-21 12:34:56");
  // Case-insensitive, and the French glibc abbreviation is recognized too.
  CHECK_EQ(keyOf(ctime, "wed Aug 21 12:34:56 2026 x"), "2026-08-21 12:34:56");
  const TimestampFormat ctimeFr("ctime-fr", "EEE MMM dd HH:mm:ss yyyy");
  CHECK_EQ(keyOf(ctimeFr, "mer. août 21 12:34:56 2026 x"), "2026-08-21 12:34:56");
  // Whatever weekday text appears is accepted -- there's no cross-check
  // against the actual date -- but something has to be there.
  CHECK_EQ(keyOf(ctime, "21 Aug 21 12:34:56 2026 x"), "(no match)");
}

TEST(twelveHourClockWithAmPmConvertsTo24Hour) {
  const TimestampFormat us12h("us-12h", "MM/dd/yyyy hh:mm:ss a");
  CHECK_EQ(keyOf(us12h, "08/21/2026 12:34:56 AM x"), "2026-08-21 00:34:56");
  CHECK_EQ(keyOf(us12h, "08/21/2026 12:34:56 PM x"), "2026-08-21 12:34:56");
  CHECK_EQ(keyOf(us12h, "08/21/2026 01:34:56 PM x"), "2026-08-21 13:34:56");
  CHECK_EQ(keyOf(us12h, "08/21/2026 11:34:56 AM x"), "2026-08-21 11:34:56");
  // Case-insensitive designator.
  CHECK_EQ(keyOf(us12h, "08/21/2026 11:34:56 pm x"), "2026-08-21 23:34:56");
  CHECK_EQ(keyOf(us12h, "08/21/2026 11:34:56 xx x"), "(no match)");
}

TEST(hourTwelveWithoutAmPmIsStillReducedModTwelve) {
  // No "a" token in the pattern at all: hh still means a 12-hour clock, so it
  // is reduced mod 12 -- there is simply no designator to add the 12 back for
  // an afternoon time. Use HH for a 24-hour clock.
  const TimestampFormat bareHh("bare-hh", "yyyy-MM-dd hh:mm:ss");
  CHECK_EQ(keyOf(bareHh, "2026-08-21 09:34:56 x"), "2026-08-21 09:34:56");
  CHECK_EQ(keyOf(bareHh, "2026-08-21 12:34:56 x"), "2026-08-21 00:34:56");
}

// Regression: the fraction scan used to stop after nine digits and leave the
// rest of them in the line for the next token to match. A pattern with a
// literal after SSS therefore failed outright on a timestamp written with more
// precision than that -- the line was not recognized as an entry at all, and
// got folded into the previous one as a continuation.
TEST(fractionLongerThanNineDigitsIsFullyConsumed) {
  const TimestampFormat vmtoolsd("vmtoolsd", "[MMM dd HH:mm:ss.SSS]");

  CHECK_EQ(keyOf(vmtoolsd, "[Aug 21 10:00:00.123] msg"), "0000-08-21 10:00:00.123000000");
  // Twelve digits: the trailing "]" still has to line up.
  CHECK_EQ(keyOf(vmtoolsd, "[Aug 21 10:00:00.123456789012] msg"), "0000-08-21 10:00:00.123456789");

  // matchedLength covers the whole timestamp, extra digits included, so the
  // viewer colours the entire span rather than stopping mid-number.
  std::size_t matchedLength = 0;
  const auto key = vmtoolsd.match("[Aug 21 10:00:00.123456789012] msg", &matchedLength);
  CHECK(key.has_value());
  CHECK_EQ(matchedLength, std::string("[Aug 21 10:00:00.123456789012]").size());
}

TEST(patternMustCoverEveryDateTimeField) {
  bool threw = false;
  try {
    const TimestampFormat incomplete("bad", "yyyy-MM-dd HH:mm");
  } catch (const std::invalid_argument &) {
    threw = true;
  }
  CHECK(threw);

  // Year, unlike every other field, is optional -- this must not throw.
  bool yearlessThrew = false;
  try {
    const TimestampFormat yearless("bad", "MM-dd HH:mm:ss");
  } catch (const std::invalid_argument &) {
    yearlessThrew = true;
  }
  CHECK(!yearlessThrew);
}

TEST(detectVotesOnTheSample) {
  const TimestampFormatCatalog catalog({
      TimestampFormat("logback", "yyyy-MM-dd HH:mm:ss.SSS"),
      TimestampFormat("plain", "yyyy-MM-dd HH:mm:ss"),
  });

  // A header without milliseconds matches "plain" only; the entries that
  // follow match both. The majority has to win, otherwise the header would
  // lock the whole file onto "plain".
  const std::vector<std::string> sample = {
      "2026-08-06 14:23:00 starting up",
      "2026-08-06 14:23:01.100 INFO a",
      "2026-08-06 14:23:02.200 INFO b",
  };
  const auto detected = catalog.detect(sample);
  CHECK(detected.has_value());
  CHECK_EQ(catalog.name(*detected), "logback");

  // Ties go to the earlier format, keeping "listed first wins".
  const auto tie = catalog.detect({"2026-08-06 14:23:01.100 INFO a"});
  CHECK(tie.has_value());
  CHECK_EQ(catalog.name(*tie), "logback");

  // Nothing recognizable at all.
  CHECK(!catalog.detect({"a line", "another"}).has_value());
}

TEST(matchReusesTheKnownFormatIndex) {
  const TimestampFormatCatalog catalog({
      TimestampFormat("logback", "yyyy-MM-dd HH:mm:ss.SSS"),
      TimestampFormat("plain", "yyyy-MM-dd HH:mm:ss"),
  });

  std::optional<std::size_t> index;
  CHECK(catalog.match("2026-08-06 14:23:01.100 INFO a", index).has_value());
  CHECK(index.has_value());
  CHECK_EQ(*index, static_cast<std::size_t>(0));

  // Pinned to "logback": a line that only "plain" would match no longer does.
  CHECK(!catalog.match("2026-08-06 14:23:02 INFO b", index).has_value());
  CHECK_EQ(*index, static_cast<std::size_t>(0));
}

TEST(mergeIsStableOnEqualKeys) {
  std::vector<LogEntry> entries = {
      entry("2026-08-06 14:23:01.500000000", {"first file"}),
      entry("2026-08-06 14:23:01.500000000", {"second file"}),
      entry("2026-08-06 14:23:00.000000000", {"earlier"}),
      entry("", {"no timestamp"}),
  };

  LogMerger::sortChronologically(entries);

  CHECK_EQ(entries[0].lines[0], "no timestamp");
  CHECK_EQ(entries[1].lines[0], "earlier");
  // Equal keys keep the order they were concatenated in.
  CHECK_EQ(entries[2].lines[0], "first file");
  CHECK_EQ(entries[3].lines[0], "second file");
}

TEST(dedupeKeepsTheFirstOccurrenceAndOrder) {
  const LogEntryDeduplicator deduplicator;
  std::vector<LogEntry> entries = {
      entry("1", {"alpha", "  at Thing.java:1"}),
      entry("2", {"beta"}),
      entry("3", {"alpha", "  at Thing.java:1"}), // identical content to the first
      entry("4", {"alpha"}),                      // same first line, different entry
  };

  const std::vector<LogEntry> result = deduplicator.dedupe(std::move(entries));

  CHECK_EQ(result.size(), static_cast<std::size_t>(3));
  CHECK_EQ(result[0].sortKey, "1");
  CHECK_EQ(result[1].sortKey, "2");
  CHECK_EQ(result[2].sortKey, "4");
}
