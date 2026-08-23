#ifndef THEME_THEMECATALOG_HPP_
#define THEME_THEMECATALOG_HPP_

#include <string>
#include <vector>

namespace logutils::theme {

  /**
   * @brief Names of all built-in themes, in display/picker order.
   *
   * The first entry ({@code "dark"}) is the fallback {@code Theme::byName()}
   * resolves an unrecognized name to, not the application default -- that is
   * {@code "nord-dark"} (see {@code AppOptions}), or whatever {@code /theme}
   * last saved. Framework-agnostic (no ftxui dependency) so it can be used
   * from the controller and CLI layers as well as the view.
   *
   * @return a reference to a function-local static table (built once) rather
   * than a fresh vector, since this is called from the theme picker's
   * per-frame render path.
   */
  const std::vector<std::string> &names();

} // namespace logutils::theme

#endif /* THEME_THEMECATALOG_HPP_ */
