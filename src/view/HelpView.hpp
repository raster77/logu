#ifndef VIEW_HELPVIEW_HPP_
#define VIEW_HELPVIEW_HPP_

#include <cstddef>

#include <ftxui/dom/elements.hpp>

#include "../command/CommandRegistry.hpp"
#include "../theme/Theme.hpp"

namespace logutils::HelpView {

  /**
   * @brief Number of rows {@link #build} produces for the given registry,
   * i.e. the valid range for {@code scrollIndex} below.
   *
   * Kept in sync with {@link #build}'s row layout so {@code TerminalView}
   * can clamp the scroll position without duplicating it.
   *
   * @param commands the registry to size the panel for.
   * @return the row count.
   */
  std::size_t rowCount(const CommandRegistry &commands);

  /**
   * @brief Builds the reference panel shown in place of the log list while
   * {@code "/help"} is active: the command list followed by the keyboard
   * shortcut list.
   *
   * @param commands the registry to list commands/shortcuts from.
   * @param t the active color theme.
   * @param scrollIndex the row (clamped to {@code rowCount()-1}) scrolled
   * into view by the content frame, letting the panel scroll when it's
   * taller than {@code availableHeight}.
   * @param availableHeight the actual rows {@code TerminalView} has left for
   * the item area (its {@code pageSize}), not the raw terminal height, since
   * the command box/status bar/etc. also share the screen.
   * @return the rendered help panel elements.
   */
  ftxui::Elements build(
      const CommandRegistry &commands, const theme::Theme &t, std::size_t scrollIndex, int availableHeight);

} // namespace logutils::HelpView

#endif /* VIEW_HELPVIEW_HPP_ */
