#include "CommandSuggestionsView.hpp"

namespace logutils::CommandSuggestionsView {

  ftxui::Element build(const std::vector<const CommandDef*> &suggestions, int selectedIndex, const theme::Theme &t) {
    using namespace ftxui;

    Elements suggestionRows;

    for (int i = 0; i < static_cast<int>(suggestions.size()); ++i) {
      const CommandDef &cmd = *suggestions[static_cast<std::size_t>(i)];
      const bool sel = (i == selectedIndex);
      const std::string arg = cmd.argHint.empty() ? "" : (" " + cmd.argHint);
      const Color rowText = sel ? t.selectedText : t.text;
      const Color rowDim = sel ? t.selectedText : t.dimText;
      const Color rowMark = sel ? t.selectedText : t.accent;

      Element row = hbox({
        text(sel ? " > " : "   ") | color(rowMark),
            text(cmd.trigger + arg) | bold | color(rowText),
            filler(),
            text("  " + cmd.description) | color(rowDim),
      });

      if (sel) {
        row = row | bgcolor(t.selectedBg);
      }

      suggestionRows.push_back(row);
    }

    return vbox(suggestionRows);
  }

} // namespace logutils::CommandSuggestionsView
