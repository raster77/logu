#ifndef VIEW_THEMEPICKERVIEW_HPP_
#define VIEW_THEMEPICKERVIEW_HPP_

#include <ftxui/dom/elements.hpp>

#include "../theme/Theme.hpp"

namespace logutils::ThemePickerView {

  /**
   * @brief Builds the theme list shown in place of the log list while the
   * {@code "/theme"} picker is open: every built-in theme name (see
   * {@code ThemeCatalog::names()}), with {@code selectedIndex} highlighted.
   *
   * Up/Down moves the selection, Enter applies it, Esc cancels;
   * {@code TerminalView} wires the actual key handling through
   * {@code ViewerController}.
   *
   * @param selectedIndex the highlighted row.
   * @param t the active color theme.
   * @param availableHeight the actual rows {@code TerminalView} has left for
   * the item area (its {@code pageSize}), not the raw terminal height, since
   * the command box/status bar/etc. also share the screen; the list scrolls
   * internally when it's taller than that.
   * @return the rendered theme list elements.
   */
  ftxui::Elements build(int selectedIndex, const theme::Theme &t, int availableHeight);

} // namespace logutils::ThemePickerView

#endif /* VIEW_THEMEPICKERVIEW_HPP_ */
