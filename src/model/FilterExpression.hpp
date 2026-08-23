#ifndef MODEL_FILTEREXPRESSION_HPP_
#define MODEL_FILTEREXPRESSION_HPP_

#include <string>
#include <vector>

namespace logutils {

  /**
   * @brief A single term of a filter expression: literal text (a substring in
   * {@code /filter}, a regex pattern in {@code /regexp}) plus whether it's
   * negated by a leading {@code '!'}.
   */
  struct FilterTerm {
      /** The literal substring or regex pattern text. */
      std::string text;
      /** Whether this term is negated (matches when {@link #text} does not match). */
      bool negate = false;
  };

  /**
   * @brief One AND-group: every term in it must match (honoring per-term
   * negation) for the group to match.
   */
  using FilterAndGroup = std::vector<FilterTerm>;

  /**
   * @brief Parses a {@code /filter} or {@code /regexp} argument into an
   * OR-of-AND-groups expression: at least one group must have all of its
   * terms match.
   *
   * Terms combine with {@code "&&"} (AND, also the default when terms are
   * simply space-separated with no operator between them) and {@code "||"}
   * (OR); a leading {@code "!"} on a term negates it. There's no
   * parenthesized grouping, so precedence is fixed: {@code "!"} binds to a
   * single term, {@code "&&"}/implicit-AND binds tighter than {@code "||"}.
   * Wrap a term containing whitespace in double quotes to keep it as one
   * term, e.g. {@code "connection refused" && !timeout}.
   *
   * @param text the raw filter expression text.
   * @return the parsed OR-of-AND-groups expression.
   */
  std::vector<FilterAndGroup> parseFilterExpression(const std::string &text);

} // namespace logutils

#endif /* MODEL_FILTEREXPRESSION_HPP_ */
