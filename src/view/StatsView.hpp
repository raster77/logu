#ifndef VIEW_STATSVIEW_HPP_
#define VIEW_STATSVIEW_HPP_

#include <cstddef>

#include <ftxui/dom/elements.hpp>

#include "../service/LogStats.hpp"
#include "../theme/Theme.hpp"

namespace logutils::StatsView {

  /**
   * @brief Number of rows {@link #build} produces for {@code levelCount}
   * level rows, i.e. the valid range for {@code scrollIndex} below.
   *
   * Kept in sync with {@link #build}'s row layout so {@code TerminalView}
   * can clamp the scroll position without duplicating it.
   *
   * @param levelCount the number of level rows the panel will show.
   * @return the row count.
   */
  std::size_t rowCount(std::size_t levelCount);

  /**
   * @brief Builds the panel shown in place of the log list while
   * {@code "/stats"} is active: a level breakdown of the entries that were
   * visible (post-filter) when the command was invoked.
   *
   * @param counts the level breakdown to render.
   * @param totalEntries the total visible entry count at invocation time.
   * @param t the active color theme.
   * @param scrollIndex the row (clamped to {@code rowCount()-1}) scrolled
   * into view by the content frame, letting the panel scroll when it's
   * taller than {@code availableHeight}.
   * @param availableHeight the actual rows {@code TerminalView} has left for
   * the item area (its {@code pageSize}), not the raw terminal height, since
   * the command box/status bar/etc. also share the screen.
   * @return the rendered stats panel elements.
   */
  ftxui::Elements build(const std::vector<LogStats::LevelCount> &counts, std::size_t totalEntries,
      const theme::Theme &t, std::size_t scrollIndex, int availableHeight);

} // namespace logutils::StatsView

#endif /* VIEW_STATSVIEW_HPP_ */
