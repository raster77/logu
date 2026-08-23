#ifndef UTIL_TEXTUTIL_HPP_
#define UTIL_TEXTUTIL_HPP_

#include <string>
#include <string_view>

namespace logutils::text_util {

  /**
   * @brief Lower-cases {@code s} in place (returned by value).
   *
   * ASCII-only and length-preserving; bytes outside A-Z are left alone. See
   * {@code TextUtil.cpp} for why this doesn't use {@code std::tolower}.
   *
   * @param s the string to lower-case.
   * @return the lower-cased string.
   */
  std::string toLower(std::string s);

  /**
   * @brief Case-insensitive substring search with no intermediate allocation.
   *
   * Unlike {@code toLower(hay).find(needle)}, this doesn't copy {@code hay}
   * just to test membership. {@code needle} does not need to be lower-cased
   * ahead of time. Case-folds on the same ASCII rule as {@link #toLower}, so
   * a needle already lower-cased by {@link #toLower} matches exactly what an
   * un-lowered one would.
   *
   * @param hay the text to search.
   * @param needle the substring to search for.
   * @return {@code true} if {@code hay} contains {@code needle}, case-insensitively.
   */
  bool icontains(std::string_view hay, std::string_view needle);

  /**
   * @brief {@link #icontains} for a needle the caller has already folded with {@link #toLower}.
   *
   * Identical results, but it skips re-folding the needle on every byte
   * compared -- which {@link #icontains} has to do, having no way to know.
   * Worth the separate entry point only where the same needle is tested
   * against very many lines (a filter term against a whole document, a find
   * term against every line scanned); anywhere else, prefer {@link #icontains}.
   *
   * Passing a needle that is not already lower-case makes this silently miss
   * matches, so it is the caller's job to have run it through {@link #toLower}.
   *
   * @param hay the text to search.
   * @param needleLower the substring to search for, already lower-cased.
   * @return {@code true} if {@code hay} contains {@code needleLower}, case-insensitively.
   */
  bool icontainsLower(std::string_view hay, std::string_view needleLower);

  /**
   * @brief Case-insensitive equality, same ASCII rule as {@link #toLower} and {@link #icontains}.
   *
   * Allocates nothing, so it's usable as a cheap pre-filter in a hot loop.
   *
   * @param a first string.
   * @param b second string.
   * @return {@code true} if {@code a} and {@code b} are equal, case-insensitively.
   */
  bool iequals(std::string_view a, std::string_view b);

} // namespace logutils::text_util

#endif /* UTIL_TEXTUTIL_HPP_ */
