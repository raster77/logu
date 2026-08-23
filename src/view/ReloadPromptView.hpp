#ifndef VIEW_RELOADPROMPTVIEW_HPP_
#define VIEW_RELOADPROMPTVIEW_HPP_

#include <ftxui/dom/elements.hpp>

#include "../theme/Theme.hpp"

namespace logutils::ReloadPromptView {

  /**
   * @brief Builds the confirmation prompt shown in place of the log list
   * when {@code --working-dir} detects that a file was added, removed, or modified.
   *
   * Enter/'y' accepts (re-merge and reload); Esc/'n' declines;
   * {@code TerminalView} wires the actual key handling through
   * {@code ViewerController}.
   *
   * @param t the active color theme.
   * @return the rendered prompt elements.
   */
  ftxui::Elements build(const theme::Theme &t);

} // namespace logutils::ReloadPromptView

#endif /* VIEW_RELOADPROMPTVIEW_HPP_ */
