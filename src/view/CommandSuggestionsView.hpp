#ifndef VIEW_COMMANDSUGGESTIONSVIEW_HPP_
#define VIEW_COMMANDSUGGESTIONSVIEW_HPP_

#include <vector>

#include <ftxui/dom/elements.hpp>

#include "../command/CommandDefinition.hpp"
#include "../theme/Theme.hpp"

namespace logutils::CommandSuggestionsView {

  /**
   * @brief Builds the {@code "/command"} autocomplete list shown above the command box.
   * @param suggestions the candidate commands to list.
   * @param selectedIndex the highlighted row, or a negative value if none is selected.
   * @param t the active color theme.
   * @return the rendered suggestion list.
   */
  ftxui::Element build(const std::vector<const CommandDef*> &suggestions, int selectedIndex, const theme::Theme &t);

} // namespace logutils::CommandSuggestionsView

#endif /* VIEW_COMMANDSUGGESTIONSVIEW_HPP_ */
