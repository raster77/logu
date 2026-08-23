#include "Testing.hpp"

#include <string>
#include <vector>

#include "model/FilterExpression.hpp"
#include "model/LogDocument.hpp"
#include "model/LogEntry.hpp"

using namespace logutils;

namespace {

  // Flattens a parsed expression to "a,!b | c" so a whole parse is one
  // readable expectation.
  std::string describe(const std::vector<FilterAndGroup> &groups) {
    std::string out;
    for (const FilterAndGroup &group : groups) {
      if (!out.empty()) {
        out += " | ";
      }
      bool first = true;
      for (const FilterTerm &term : group) {
        if (!first) {
          out += ",";
        }
        first = false;
        out += (term.negate ? "!" : "") + term.text;
      }
    }
    return out;
  }

  std::vector<LogEntry> sampleEntries() {
    return {
        LogEntry{"1", {"2026-08-06 14:23:01.000 INFO first", "  at Thing.java:1"}},
        LogEntry{"2", {"2026-08-06 14:23:02.000 ERROR second failed"}},
        LogEntry{"3", {"2026-08-06 14:23:03.000 INFO third"}},
    };
  }

} // namespace

TEST(filterExpressionGrammar) {
  CHECK_EQ(describe(parseFilterExpression("error")), "error");
  CHECK_EQ(describe(parseFilterExpression("error timeout")), "error,timeout");
  CHECK_EQ(describe(parseFilterExpression("error && timeout")), "error,timeout");
  CHECK_EQ(describe(parseFilterExpression("error || timeout")), "error | timeout");
  CHECK_EQ(describe(parseFilterExpression("error && !timeout")), "error,!timeout");
  CHECK_EQ(describe(parseFilterExpression("! timeout")), "!timeout");
  CHECK_EQ(describe(parseFilterExpression("!!timeout")), "timeout");
  CHECK_EQ(describe(parseFilterExpression("\"connection refused\"")), "connection refused");
  CHECK_EQ(describe(parseFilterExpression("\"connection refused\" && !timeout")),
           "connection refused,!timeout");
  // Trailing whitespace can't produce a term (which is what lets the command
  // layer trim arguments without changing what a filter means).
  CHECK_EQ(describe(parseFilterExpression("error   ")), "error");
  // An unterminated quote runs to the end of the input rather than failing.
  CHECK_EQ(describe(parseFilterExpression("\"still open")), "still open");
  // Operators with nothing to operate on produce no groups at all, which is
  // what LogDocument rejects instead of applying a match-nothing filter.
  CHECK(parseFilterExpression("!").empty());
  CHECK(parseFilterExpression("&&").empty());
  CHECK(parseFilterExpression("   ").empty());
}

TEST(documentExposesLinesAndEntries) {
  LogDocument document(sampleEntries());

  CHECK_EQ(document.totalLineCount(), static_cast<std::size_t>(4));
  CHECK_EQ(document.visibleLineCount(), static_cast<std::size_t>(4));
  CHECK_EQ(document.visibleLineAt(1), "  at Thing.java:1");
  // The focused line resolves to its whole entry, continuation lines included.
  CHECK_EQ(document.currentEntryContent(),
           "2026-08-06 14:23:01.000 INFO first\n  at Thing.java:1\n");
}

TEST(substringFilterMatchesAndIsRejectedWhenEmpty) {
  LogDocument document(sampleEntries());

  CHECK(document.setFilter("info"));  // case-insensitive
  CHECK_EQ(document.visibleLineCount(), static_cast<std::size_t>(2));
  CHECK_EQ(document.visibleLineAt(0), "2026-08-06 14:23:01.000 INFO first");

  CHECK(document.setFilter("info || error"));
  CHECK_EQ(document.visibleLineCount(), static_cast<std::size_t>(3));

  CHECK(document.setFilter("info && !third"));
  CHECK_EQ(document.visibleLineCount(), static_cast<std::size_t>(1));

  // A filter with no usable terms is refused, and leaves the previous one in
  // place rather than blanking the screen.
  CHECK(!document.setFilter("!"));
  CHECK_EQ(document.filter(), "info && !third");
  CHECK_EQ(document.visibleLineCount(), static_cast<std::size_t>(1));

  CHECK(document.setFilter(""));
  CHECK_EQ(document.visibleLineCount(), static_cast<std::size_t>(4));
}

TEST(regexpFilterCompilesOrIsRefused) {
  LogDocument document(sampleEntries());

  CHECK(document.setRegexpFilter("^2026.*ERROR"));
  CHECK_EQ(document.visibleLineCount(), static_cast<std::size_t>(1));
  CHECK(document.filterIsRegex());

  // An uncompilable pattern leaves the active filter untouched.
  CHECK(!document.setRegexpFilter("("));
  CHECK_EQ(document.filter(), "^2026.*ERROR");
  CHECK_EQ(document.visibleLineCount(), static_cast<std::size_t>(1));

  document.clearFilter();
  CHECK(!document.filterIsRegex());
  CHECK_EQ(document.visibleLineCount(), static_cast<std::size_t>(4));
}

TEST(findWrapsAroundInBothDirections) {
  LogDocument document(sampleEntries());

  CHECK(document.setFindTerm("third"));
  CHECK_EQ(document.scrollOffset(), 3);

  // Forward from the last match wraps back to it (only one match).
  CHECK(document.findNext());
  CHECK_EQ(document.scrollOffset(), 3);

  // setFindTerm searches from the current position, so start from the top to
  // make the wrap-around sequence below unambiguous.
  document.scrollToStart();
  CHECK(document.setFindTerm("INFO"));
  CHECK_EQ(document.scrollOffset(), 0);
  CHECK(document.findNext());
  CHECK_EQ(document.scrollOffset(), 3);
  CHECK(document.findNext()); // wraps
  CHECK_EQ(document.scrollOffset(), 0);
  CHECK(document.findPrevious());
  CHECK_EQ(document.scrollOffset(), 3);

  CHECK(!document.setFindTerm("nothing here"));
  // A failed search leaves the position alone.
  CHECK_EQ(document.scrollOffset(), 3);
}

TEST(visibleEntryHeadersCollapseContinuationLines) {
  LogDocument document(sampleEntries());

  auto headers = document.visibleEntryHeaders();
  CHECK_EQ(headers.size(), static_cast<std::size_t>(3));
  CHECK_EQ(headers[0], "2026-08-06 14:23:01.000 INFO first");

  // With a filter matching only a continuation line, the entry is still
  // reported once, by its header.
  CHECK(document.setFilter("Thing.java"));
  headers = document.visibleEntryHeaders();
  CHECK_EQ(headers.size(), static_cast<std::size_t>(1));
  CHECK_EQ(headers[0], "2026-08-06 14:23:01.000 INFO first");
}

TEST(scrollingClampsToTheVisibleRange) {
  LogDocument document(sampleEntries());

  document.scrollUp(10);
  CHECK_EQ(document.scrollOffset(), 0);
  document.scrollDown(100);
  CHECK_EQ(document.scrollOffset(), 3);
  document.scrollToStart();
  CHECK_EQ(document.scrollOffset(), 0);
  document.scrollToEnd();
  CHECK_EQ(document.scrollOffset(), 3);

  // Filtering to fewer lines pulls the position back into range.
  CHECK(document.setFilter("third"));
  CHECK_EQ(document.scrollOffset(), 0);
}

TEST(reloadRebuildsAndReappliesTheFilter) {
  LogDocument document(sampleEntries());
  CHECK(document.setFilter("INFO"));
  document.scrollToEnd();
  CHECK_EQ(document.scrollOffset(), 1);

  document.reload({LogEntry{"1", {"2026-08-06 14:23:09.000 INFO only one"}}});

  CHECK_EQ(document.totalLineCount(), static_cast<std::size_t>(1));
  // The filter survives the reload and is applied to the new content...
  CHECK_EQ(document.filter(), "INFO");
  CHECK_EQ(document.visibleLineCount(), static_cast<std::size_t>(1));
  // ...and the scroll position is clamped to what's left.
  CHECK_EQ(document.scrollOffset(), 0);

  // Reloading an empty document must not leave dangling indices behind.
  document.reload({});
  CHECK_EQ(document.totalLineCount(), static_cast<std::size_t>(0));
  CHECK_EQ(document.visibleLineCount(), static_cast<std::size_t>(0));
  CHECK_EQ(document.currentEntryContent(), "");
  CHECK(document.visibleEntryHeaders().empty());
}

TEST(emptyDocumentIsSafeToQuery) {
  LogDocument document({});

  CHECK_EQ(document.totalLineCount(), static_cast<std::size_t>(0));
  CHECK_EQ(document.currentEntryContent(), "");
  CHECK(!document.findNext());
  CHECK(!document.setFindTerm("anything"));
  document.scrollDown(5);
  CHECK_EQ(document.scrollOffset(), 0);
}
