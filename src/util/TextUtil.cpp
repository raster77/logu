#include "TextUtil.hpp"

#include <algorithm>
#include <cstddef>

namespace logutils::text_util {

  namespace {

    // ASCII-only, deliberately not std::tolower: that one is a locale-aware,
    // out-of-line call, and it dominates the cost of icontains() below, which
    // runs over every line of the document on every keystroke-driven filter
    // change. Log filtering also shouldn't quietly change behavior with
    // LC_CTYPE. This program never calls setlocale, so std::tolower is running
    // in the "C" locale anyway -- where the two agree exactly, non-ASCII bytes
    // (e.g. UTF-8 continuation bytes) being left alone by both.
    //
    // Length-preserving, which buildHighlightedLine() relies on: it maps
    // offsets in a lower-cased copy back onto the original line.
    constexpr char asciiLower(char c) {
      return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    }

    // The shared core of icontains()/icontainsLower(). FoldNeedle is a
    // template parameter rather than a runtime flag so the fold either
    // compiles into the inner loop or vanishes from it entirely -- this loop
    // runs over every line of the document on every keystroke-driven filter
    // change.
    template <bool FoldNeedle>
    bool containsImpl(std::string_view hay, std::string_view needle) {
      if (needle.empty()) {
        return true;
      }

      if (needle.size() > hay.size()) {
        return false;
      }

      const auto foldNeedle = [](char c) {
        if constexpr (FoldNeedle) {
          return asciiLower(c);
        } else {
          return c;
        }
      };

      // Scan for the first character, then compare the rest in place. The
      // std::search + std::tolower predicate form this replaced allocated
      // nothing either, but lowered both sides through a function call per byte
      // compared, which cost more than the allocation it had saved.
      const char first = foldNeedle(needle[0]);
      const std::size_t lastStart = hay.size() - needle.size();

      for (std::size_t i = 0; i <= lastStart; ++i) {
        if (asciiLower(hay[i]) != first) {
          continue;
        }

        std::size_t k = 1;

        while (k < needle.size() && asciiLower(hay[i + k]) == foldNeedle(needle[k])) {
          ++k;
        }

        if (k == needle.size()) {
          return true;
        }
      }

      return false;
    }

  } // namespace

  std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), asciiLower);
    return s;
  }

  bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
      return false;
    }

    for (std::size_t i = 0; i < a.size(); ++i) {
      if (asciiLower(a[i]) != asciiLower(b[i])) {
        return false;
      }
    }

    return true;
  }

  bool icontains(std::string_view hay, std::string_view needle) {
    return containsImpl<true>(hay, needle);
  }

  bool icontainsLower(std::string_view hay, std::string_view needleLower) {
    return containsImpl<false>(hay, needleLower);
  }

} // namespace logutils::text_util
