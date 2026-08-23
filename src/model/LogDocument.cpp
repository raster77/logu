#include "LogDocument.hpp"

#include <algorithm>
#include <cstddef>

#include "../util/TextUtil.hpp"

namespace logutils {

  LogDocument::LogDocument(std::vector<LogEntry> entries) {
    buildFromEntries(std::move(entries));
  }

  void LogDocument::buildFromEntries(std::vector<LogEntry> entries) {
    // One pass to size the vectors: a merged log is millions of lines, and
    // growing two of them geometrically costs more than counting first.
    std::size_t lineCount = 0;

    for (const LogEntry &entry : entries) {
      lineCount += entry.lines.size();
    }

    mAllLines.reserve(lineCount);
    mLineEntryIndex.reserve(lineCount);
    mLineTimestampLength.reserve(lineCount);
    mEntryLineStart.reserve(entries.size() + 1);
    mEntryLineStart.push_back(0);

    for (std::size_t i = 0; i < entries.size(); ++i) {
      for (std::size_t j = 0; j < entries[i].lines.size(); ++j) {
        mAllLines.push_back(std::move(entries[i].lines[j]));
        mLineEntryIndex.push_back(static_cast<int>(i));
        mLineTimestampLength.push_back(j == 0 ? entries[i].timestampLength : 0);
      }

      mEntryLineStart.push_back(static_cast<int>(mAllLines.size()));
    }
  }

  void LogDocument::reload(std::vector<LogEntry> entries) {
    mAllLines.clear();
    mLineEntryIndex.clear();
    mLineTimestampLength.clear();
    mEntryLineStart.clear();
    mEntryContentCache.clear();
    buildFromEntries(std::move(entries));
    recomputeFiltered();
  }

  bool LogDocument::setFilter(std::string needle) {
    const auto groups = parseFilterExpression(needle);
    if (!needle.empty() && groups.empty()) {
      // e.g. "!" or "&&" alone: an operator with nothing to operate on.
      // Leave the current filter untouched instead of locking in a filter
      // with zero usable groups, which would match nothing and blank the
      // screen with no indication of why.
      return false;
    }

    mFilter = std::move(needle);
    mFilterIsRegex = false;
    mRegexGroups.clear();

    buildSubstringGroups(groups);
    recomputeFiltered();

    return true;
  }

  bool LogDocument::setRegexpFilter(std::string pattern) {
    if (pattern.empty()) {
      setFilter("");
      return true;
    }

    const auto groups = parseFilterExpression(pattern);

    // Same guard as setFilter(): a pattern that parses to zero groups
    // ("!", "&&") compiles without error precisely because there is nothing
    // to compile, and would then match no line at all.
    if (groups.empty()) {
      return false;
    }

    std::vector<std::vector<RegexTerm>> compiledGroups;
    compiledGroups.reserve(groups.size());

    try {
      for (const auto &group : groups) {
        std::vector<RegexTerm> compiledGroup;
        compiledGroup.reserve(group.size());

        for (const auto &term : group) {
          compiledGroup.push_back(
              RegexTerm{std::regex(term.text, std::regex::icase), term.negate});
        }

        compiledGroups.push_back(std::move(compiledGroup));
      }
    } catch (const std::regex_error &) {
      return false;
    }

    mFilter = std::move(pattern);
    mFilterIsRegex = true;
    mRegexGroups = std::move(compiledGroups);
    mSubstringGroups.clear();
    recomputeFiltered();

    return true;
  }

  void LogDocument::buildSubstringGroups(const std::vector<FilterAndGroup> &groups) {
    mSubstringGroups.clear();
    mSubstringGroups.reserve(groups.size());

    for (const auto &group : groups) {
      std::vector<SubstringTerm> substringGroup;
      substringGroup.reserve(group.size());

      for (const auto &term : group) {
        substringGroup.push_back(SubstringTerm{text_util::toLower(term.text), term.negate});
      }

      mSubstringGroups.push_back(std::move(substringGroup));
    }
  }

  bool LogDocument::matchesSubstringGroups(const std::string &line) const {
    for (const auto &group : mSubstringGroups) {
      bool allMatch = true;

      for (const auto &term : group) {
        const bool contains = text_util::icontainsLower(line, term.needleLower);
        if (contains == term.negate) {
          allMatch = false;
          break;
        }
      }

      if (allMatch) {
        return true;
      }
    }

    return false;
  }

  bool LogDocument::matchesRegexGroups(const std::string &line) const {
    for (const auto &group : mRegexGroups) {
      bool allMatch = true;

      for (const auto &term : group) {
        const bool matches = std::regex_search(line, term.pattern);
        if (matches == term.negate) {
          allMatch = false;
          break;
        }
      }

      if (allMatch) {
        return true;
      }
    }
    return false;
  }

  void LogDocument::clearFilter() {
    setFilter("");
  }

  const std::string &LogDocument::filter() const {
    return mFilter;
  }

  bool LogDocument::filterIsRegex() const {
    return mFilterIsRegex;
  }

  void LogDocument::recomputeFiltered() {
    mFilteredIndex.clear();

    if (mFilter.empty()) {
      // No filter: lineAt()/entryIndexAt() fall back to mAllLines/
      // mLineEntryIndex directly, so release any previously filtered index
      // instead of keeping it (or a duplicated copy of the whole document)
      // around.
      mFilteredIndex.shrink_to_fit();
    } else if (mFilterIsRegex) {
      for (std::size_t i = 0; i < mAllLines.size(); ++i) {
        if (matchesRegexGroups(mAllLines[i])) {
          mFilteredIndex.push_back(static_cast<int>(i));
        }
      }
    } else {
      for (std::size_t i = 0; i < mAllLines.size(); ++i) {
        if (matchesSubstringGroups(mAllLines[i])) {
          mFilteredIndex.push_back(static_cast<int>(i));
        }
      }
    }

    clampScroll();
  }

  bool LogDocument::setFindTerm(std::string term) {
    mFindTerm = std::move(term);
    mFindTermLower = text_util::toLower(mFindTerm);

    if (mFindTerm.empty()) {
      return true;
    }

    return searchFrom(mScrollOffset, 1);
  }

  bool LogDocument::findNext() {
    if (mFindTerm.empty()) {
      return false;
    }

    return searchFrom(mScrollOffset + 1, 1);
  }

  bool LogDocument::findPrevious() {
    if (mFindTerm.empty()) {
      return false;
    }

    return searchFrom(mScrollOffset - 1, -1);
  }

  const std::string &LogDocument::findTerm() const {
    return mFindTerm;
  }

  bool LogDocument::searchFrom(int startIndex, int direction) {
    const int n = static_cast<int>(activeLineCount());

    if (n == 0) {
      return false;
    }

    int idx = ((startIndex % n) + n) % n;

    for (int steps = 0; steps < n; ++steps) {
      if (text_util::icontainsLower(lineAt(static_cast<std::size_t>(idx)), mFindTermLower)) {
        mScrollOffset = idx;
        return true;
      }

      idx = ((idx + direction) % n + n) % n;
    }
    return false;
  }

  void LogDocument::clampScroll() {
    const int maxOffset = std::max(0, static_cast<int>(activeLineCount()) - 1);
    mScrollOffset = std::clamp(mScrollOffset, 0, maxOffset);
  }

  std::size_t LogDocument::activeLineCount() const {
    return mFilter.empty() ? mAllLines.size() : mFilteredIndex.size();
  }

  const std::string &LogDocument::lineAt(std::size_t i) const {
    const std::size_t idx = mFilter.empty() ? i : static_cast<std::size_t>(mFilteredIndex[i]);
    return mAllLines[idx];
  }

  std::size_t LogDocument::timestampLengthAt(std::size_t i) const {
    const std::size_t idx = mFilter.empty() ? i : static_cast<std::size_t>(mFilteredIndex[i]);
    return mLineTimestampLength[idx];
  }

  int LogDocument::entryIndexAt(std::size_t i) const {
    const std::size_t idx = mFilter.empty() ? i : static_cast<std::size_t>(mFilteredIndex[i]);
    return mLineEntryIndex[idx];
  }

  std::string LogDocument::buildEntryContent(int entryIndex) const {
    const auto start = static_cast<std::size_t>(mEntryLineStart[static_cast<std::size_t>(entryIndex)]);
    const auto end = static_cast<std::size_t>(mEntryLineStart[static_cast<std::size_t>(entryIndex) + 1]);

    return joinLines(mAllLines.begin() + static_cast<std::ptrdiff_t>(start),
                     mAllLines.begin() + static_cast<std::ptrdiff_t>(end));
  }

  std::size_t LogDocument::totalLineCount() const {
    return mAllLines.size();
  }

  std::size_t LogDocument::visibleLineCount() const {
    return activeLineCount();
  }

  const std::string &LogDocument::visibleLineAt(std::size_t i) const {
    return lineAt(i);
  }

  std::size_t LogDocument::visibleLineTimestampLength(std::size_t i) const {
    return timestampLengthAt(i);
  }

  const std::string &LogDocument::currentEntryContent() const {
    static const std::string empty;

    if (activeLineCount() == 0) {
      return empty;
    }

    mEntryContentCache = buildEntryContent(entryIndexAt(static_cast<std::size_t>(mScrollOffset)));

    return mEntryContentCache;
  }

  std::vector<std::string> LogDocument::visibleEntryHeaders() const {
    std::vector<std::string> headers;
    int lastEntryIndex = -1;
    const std::size_t n = activeLineCount();

    for (std::size_t i = 0; i < n; ++i) {
      const int entryIndex = entryIndexAt(i);
      if (entryIndex != lastEntryIndex) {
        const auto start = static_cast<std::size_t>(mEntryLineStart[static_cast<std::size_t>(entryIndex)]);
        headers.push_back(mAllLines[start]);
        lastEntryIndex = entryIndex;
      }
    }

    return headers;
  }

  void LogDocument::scrollUp(int amount) {
    mScrollOffset = std::max(0, mScrollOffset - amount);
  }

  void LogDocument::scrollDown(int amount) {
    const int maxOffset = std::max(0, static_cast<int>(activeLineCount()) - 1);
    mScrollOffset = std::min(maxOffset, mScrollOffset + amount);
  }

  void LogDocument::scrollToStart() {
    mScrollOffset = 0;
  }

  void LogDocument::scrollToEnd() {
    mScrollOffset = std::max(0, static_cast<int>(activeLineCount()) - 1);
  }

  int LogDocument::scrollOffset() const {
    return mScrollOffset;
  }

} // namespace logutils
