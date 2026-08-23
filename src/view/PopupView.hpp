#ifndef VIEW_POPUPVIEW_HPP_
#define VIEW_POPUPVIEW_HPP_

#include <string>

#include <ftxui/dom/elements.hpp>

#include "../theme/Theme.hpp"

namespace logutils::PopupView {

  /**
   * @brief Shared building blocks for the panels shown in place of the log
   * list -- help, stats, the theme picker, the reload prompt.
   *
   * All of them are the same bordered, centered box with a title row and
   * (except the prompt) a scrollable body. These are the pieces they share;
   * each view still assembles its own rows, so the ones with an extra
   * separator or a fixed body keep it.
   */

  /**
   * @brief Builds a panel title row: the panel's name on the left, a hint
   * (e.g. {@code "Esc close"}) pushed to the right edge.
   * @param title the panel's name.
   * @param hint the hint text, or empty for a bare title.
   * @param t the active color theme.
   * @return the rendered title row.
   */
  ftxui::Element titleRow(const std::string &title, const std::string &hint, const theme::Theme &t);

  /**
   * @brief How many rows the scrollable body may occupy.
   * @param availableHeight the rows {@code TerminalView} actually leaves for
   * the item area, i.e. its {@code pageSize}, not the raw terminal height.
   * @param chromeRows the rows the panel spends on its own chrome -- title, separators, borders.
   * @return {@code availableHeight - chromeRows}, the body's row budget.
   */
  int contentBudget(int availableHeight, int chromeRows);

  /**
   * @brief Wraps a panel's body so it scrolls within {@code budget} rows.
   *
   * {@code vscroll_indicator} draws a scrollbar reflecting the frame's
   * actual scroll state; {@code yframe} (not {@code frame}) scrolls
   * vertically only, so paragraphs stay constrained to the popup's width and
   * wrap instead of being handed unlimited width. Same
   * {@code element|vscroll_indicator|frame|size(...)} order ftxui's own
   * Dropdown component uses.
   *
   * @param content the body content to wrap.
   * @param budget the row budget, as from {@link #contentBudget}.
   * @return the scrollable element.
   */
  ftxui::Element scrollable(ftxui::Elements content, int budget);

  /**
   * @brief The bordered, centered box every panel sits in.
   * @param rows the panel's rows (title, body, etc.).
   * @param maxWidth caps the box's width; it also never exceeds the terminal's width.
   * @param t the active color theme.
   * @return the single-element {@code Elements} list {@code TerminalView} expects for the item area.
   */
  ftxui::Elements frame(ftxui::Elements rows, int maxWidth, const theme::Theme &t);

  /**
   * @brief The left-hand label column the list panels share: two spaces of
   * indent, then {@code label} padded out to {@code width} columns in the accent colour.
   *
   * Passing the same {@code width} for every row of a panel is what lines
   * the descriptions up with each other.
   *
   * @param label the row's label text.
   * @param width the column width to pad {@code label} to.
   * @param t the active color theme.
   * @return the rendered label column.
   */
  ftxui::Element labelColumn(const std::string &label, std::size_t width, const theme::Theme &t);

  /**
   * @brief A whole list panel -- what {@code "/help"} and {@code "/stats"}
   * both are: a title row, a separator, and a body that scrolls within
   * whatever height is left.
   *
   * Marks {@code focusIndex} in {@code body} as focused so the content frame
   * scrolls that row into view (the same mechanism {@code LogListView} uses
   * for the current line), and clamps it, so a caller's scroll position
   * can't point past the last row.
   *
   * The two panels also shared a chrome-row count and a maximum width; both
   * live here now, so they can't drift apart.
   *
   * @param title the panel's name.
   * @param hint the hint text, or empty for a bare title.
   * @param body the panel's body rows.
   * @param focusIndex the row index to scroll into view, clamped to {@code body}'s size.
   * @param availableHeight the rows {@code TerminalView} has left for the
   * item area (its {@code pageSize}), not the raw terminal height.
   * @param t the active color theme.
   * @return the rendered panel elements.
   */
  ftxui::Elements listPanel(const std::string &title, const std::string &hint,
      ftxui::Elements body, std::size_t focusIndex, int availableHeight, const theme::Theme &t);

} // namespace logutils::PopupView

#endif /* VIEW_POPUPVIEW_HPP_ */
