#ifndef MODEL_LOGDOCUMENT_HPP_
#define MODEL_LOGDOCUMENT_HPP_

#include <regex>
#include <string>
#include <vector>

#include "FilterExpression.hpp"
#include "LogEntry.hpp"

namespace logutils {

  /**
   * @brief The viewer's Model: the full set of merged log lines, the
   * currently active filter (substring or regexp), the lines it produces,
   * and the viewport (scroll) position within them.
   *
   * Knows nothing about terminals or ftxui; all of that lives in the
   * view/controller layers.
   */
  class LogDocument {
    public:
      /**
       * @brief Builds a document from a chronologically merged set of entries.
       * @param entries the merged log entries, in display order.
       */
      explicit LogDocument(std::vector<LogEntry> entries);

      /**
       * @brief Replaces the entire document with a freshly merged set of entries.
       *
       * Used e.g. after {@code --working-dir} picks up an added/changed file.
       * Reapplies the current filter and clamps the scroll offset to the new
       * size. The active find term is left as-is; a subsequent {@code /find}
       * or Ctrl+N/P re-searches the new content.
       *
       * @param entries the newly merged log entries, in display order.
       */
      void reload(std::vector<LogEntry> entries);

      /**
       * @brief Applies a case-insensitive substring filter.
       *
       * Supports multiple terms combined with {@code &&}, {@code ||}, and a
       * leading {@code !} per term (see {@link FilterExpression.hpp} for the
       * exact grammar). An empty needle clears it and returns {@code true}.
       *
       * @param needle the filter expression text.
       * @return {@code true} on success; {@code false} if a non-empty needle
       * produces no usable terms (e.g. {@code "!"} or {@code "&&"} alone --
       * operators with nothing to operate on), in which case the current
       * filter is left untouched rather than silently matching nothing.
       */
      bool setFilter(std::string needle);
      /**
       * @brief Applies a case-insensitive regular-expression filter.
       *
       * Same term/operator grammar as {@link #setFilter}, except each term is
       * an ECMAScript regex rather than a substring. An empty pattern clears it.
       *
       * @param pattern the filter expression text, each term a regex pattern.
       * @return {@code true} on success; {@code false}, leaving the current
       * filter untouched, if any term's pattern doesn't compile, or if the
       * pattern produces no usable terms (same {@code "!"}/{@code "&&"} case
       * {@link #setFilter} rejects -- such a pattern compiles fine, since
       * there is nothing to compile).
       */
      bool setRegexpFilter(std::string pattern);
      /** @brief Clears whichever filter (substring or regexp) is active. */
      void clearFilter();
      /** @return the raw text of the active filter, or empty if none. */
      const std::string &filter() const;
      /** @return {@code true} if the active filter is a regexp filter. */
      bool filterIsRegex() const;

      /**
       * @brief Sets the active search term (case-insensitive) and jumps to
       * the first match at or after the current scroll position, wrapping
       * around if needed.
       *
       * An empty term just clears the highlight without moving the scroll
       * position.
       *
       * @param term the search term.
       * @return whether a match was found (always {@code true} for an empty term).
       */
      bool setFindTerm(std::string term);
      /**
       * @brief Moves to the next line (wrapping) containing the active find term.
       * @return {@code false} without moving if there's no active term or no match.
       */
      bool findNext();
      /**
       * @brief Moves to the previous line (wrapping) containing the active find term.
       * @return {@code false} without moving if there's no active term or no match.
       */
      bool findPrevious();
      /** @return the active find term, or empty if none. */
      const std::string &findTerm() const;

      /** @return the total number of lines in the document, ignoring any filter. */
      std::size_t totalLineCount() const;
      /** @return the number of lines currently visible, honoring the active filter if any. */
      std::size_t visibleLineCount() const;
      /**
       * @brief The line at visible position {@code i}.
       * @param i index in {@code [0, visibleLineCount())}.
       * @return the i-th visible line, honoring the active filter if any.
       */
      const std::string &visibleLineAt(std::size_t i) const;
      /**
       * @brief Length of the timestamp span at the start of {@link #visibleLineAt}({@code i}).
       * @param i index in {@code [0, visibleLineCount())}.
       * @return the timestamp length, or 0 if that line isn't an entry header
       * (a continuation line) or its entry had no recognized timestamp. See
       * {@link LogEntry#timestampLength}.
       */
      std::size_t visibleLineTimestampLength(std::size_t i) const;

      /**
       * @brief Full text of the entry the currently focused (scrolled-to)
       * line belongs to, including any continuation lines like a stack trace.
       * @return the entry text, or empty if there are no visible lines. The
       * returned reference points at an internal buffer rebuilt on each call,
       * so copy it if it has to outlive the next one.
       */
      const std::string &currentEntryContent() const;

      /**
       * @brief The header line of each entry that has at least one visible line.
       *
       * "Header" means the line the entry's timestamp/level normally sit on.
       * Honors the active filter, in document order. Consecutive visible
       * lines belonging to the same entry (e.g. a stack trace) collapse to a
       * single occurrence, so this is one entry per header, not one per line.
       * Used by {@code /stats} to analyze visible entries without
       * re-exposing the line/entry index machinery those computations rely on.
       *
       * @return the visible entries' header lines, in document order.
       */
      std::vector<std::string> visibleEntryHeaders() const;

      /** @brief Scrolls up by {@code amount} lines, clamped to the start. */
      void scrollUp(int amount);
      /** @brief Scrolls down by {@code amount} lines, clamped to the end. */
      void scrollDown(int amount);
      /** @brief Scrolls to the first line. */
      void scrollToStart();
      /** @brief Scrolls to the last line. */
      void scrollToEnd();
      /** @return the current scroll (viewport) offset. */
      int scrollOffset() const;

    private:
      // Populates mAllLines/mLineEntryIndex/mEntryLineStart from entries.
      // Shared by the constructor and reload(); assumes those three are
      // already empty.
      void buildFromEntries(std::vector<LogEntry> entries);
      void recomputeFiltered();
      // Rebuilds mSubstringGroups from an already-parsed filter expression
      // (setFilter() has to parse it anyway to validate it, so it passes the
      // result in rather than making this re-parse mFilter).
      void buildSubstringGroups(const std::vector<FilterAndGroup> &groups);
      bool matchesSubstringGroups(const std::string &line) const;
      bool matchesRegexGroups(const std::string &line) const;
      void clampScroll();
      // Scans the active (filtered or not) lines for mFindTerm starting at
      // startIndex (wrapping around the ends) and stepping by `direction`
      // (+1 or -1). Moves mScrollOffset to the first match found and returns
      // true, or leaves it unchanged and returns false if there's no match
      // anywhere.
      bool searchFrom(int startIndex, int direction);

      // Number of currently displayed lines: mAllLines.size() when there's
      // no filter, mFilteredIndex.size() otherwise.
      std::size_t activeLineCount() const;
      // The line/entry-index at active (displayed) position `i`, resolved
      // through mFilteredIndex when a filter is active. Avoids keeping a
      // full duplicate copy of every matched line's text around, which
      // matters for large logs with a broad filter -- the header comment on
      // mEntryLineStart below makes the same argument for the unfiltered
      // case.
      const std::string &lineAt(std::size_t i) const;
      std::size_t timestampLengthAt(std::size_t i) const;
      int entryIndexAt(std::size_t i) const;
      // Joins the lines of entry `entryIndex` (via mEntryLineStart) the same
      // way LogEntry::content() does, computed on demand rather than
      // precomputed for every entry up front.
      std::string buildEntryContent(int entryIndex) const;

      std::vector<std::string> mAllLines;
      // Parallel to mAllLines: which entry each line belongs to, so a line
      // focused via scrolling can be traced back to its full entry (e.g. to
      // copy it with its stack trace).
      std::vector<int> mLineEntryIndex;
      // Parallel to mAllLines: the entry's timestampLength on its header
      // line, 0 on every continuation line -- so the view can color a
      // timestamp span without re-detecting it per frame.
      std::vector<std::size_t> mLineTimestampLength;
      // Indices into mAllLines/mLineEntryIndex for the lines matching the
      // active filter, in document order. Empty (and unused -- mAllLines is
      // used directly) when there's no filter.
      std::vector<int> mFilteredIndex;
      // Prefix sums into mAllLines: entry i owns lines
      // [mEntryLineStart[i], mEntryLineStart[i + 1]). Entries are contiguous
      // in mAllLines because the constructor appends them in entry order.
      std::vector<int> mEntryLineStart;
      mutable std::string mEntryContentCache;
      std::string mFilter;
      bool mFilterIsRegex = false;
      // Substring mode: needle text lower-cased ahead of time so
      // icontainsLower() can skip folding it on every byte compared -- which
      // plain icontains() cannot, and which is paid once per line of the
      // document per filter change. Populated when !mFilterIsRegex.
      struct SubstringTerm {
          std::string needleLower;
          bool negate = false;
      };
      // Regexp mode: each term's pattern pre-compiled. Populated when
      // mFilterIsRegex.
      struct RegexTerm {
          std::regex pattern;
          bool negate = false;
      };
      std::vector<std::vector<SubstringTerm>> mSubstringGroups;
      std::vector<std::vector<RegexTerm>> mRegexGroups;
      std::string mFindTerm;
      // mFindTerm lower-cased, for the same reason as SubstringTerm above:
      // searchFrom() walks the whole document looking for it. Kept alongside
      // rather than replacing mFindTerm, which findTerm() hands to the status
      // bar and to "no matches for ..." as the user typed it.
      std::string mFindTermLower;
      int mScrollOffset = 0;
  };

} // namespace logutils

#endif /* MODEL_LOGDOCUMENT_HPP_ */
