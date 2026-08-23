#include "HelpView.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "PopupView.hpp"

namespace logutils::HelpView {

  namespace {

    struct Row {
        std::string trigger;
        std::string description;
    };

    // Rows build() emits that are neither a command nor a shortcut: the
    // "Commands" label, the blank spacer, and the "Keyboard shortcuts" label.
    // rowCount() and build() both derive from this one constant, so the two
    // can't disagree about the panel's height.
    constexpr std::size_t kFixedRows = 3;

    ftxui::Element row(const Row &r, std::size_t triggerWidth, const theme::Theme &t) {
      using namespace ftxui;
      return hbox({
        text("  "),
            PopupView::labelColumn(r.trigger, triggerWidth, t),
            text("   "),
            paragraph(r.description) | color(t.text) | flex,
      });
    }

  } // namespace

  std::size_t rowCount(const CommandRegistry &commands) {
    // commandCount(), not definitions().size(): this is called on every key
    // the panel handles, and definitions() would build a vector of pointers
    // just to have its size taken.
    return kFixedRows + commands.commandCount() + commands.shortcuts().size();
  }

  ftxui::Elements build(
      const CommandRegistry &commands, const theme::Theme &t, std::size_t scrollIndex, int availableHeight) {
    using namespace ftxui;

    std::vector<Row> commandRows;
    for (const CommandDef *cmd : commands.definitions()) {
      const std::string arg = cmd->argHint.empty() ? "" : (" " + cmd->argHint);
      commandRows.push_back({cmd->trigger + arg, cmd->description});
    }

    std::vector<Row> shortcutRows;

    for (const auto &sc : commands.shortcuts()) {
      shortcutRows.push_back({sc.keys, sc.description});
    }

    // Shared column width so commands and shortcuts line up with each other.
    std::size_t triggerWidth = 0;

    for (const auto &r : commandRows) {
      triggerWidth = std::max(triggerWidth, r.trigger.size());
    }

    for (const auto &r : shortcutRows) {
      triggerWidth = std::max(triggerWidth, r.trigger.size());
    }

    // Everything below the title bar is the scrollable part; PopupView::
    // listPanel wraps it in its own fixed-height frame so the title and border
    // stay put and only this content scrolls.
    Elements content;
    content.push_back(text(" Commands") | bold | color(t.text));

    for (const auto &r : commandRows) {
      content.push_back(row(r, triggerWidth, t));
    }

    content.push_back(text(""));
    content.push_back(text(" Keyboard shortcuts") | bold | color(t.text));

    for (const auto &r : shortcutRows) {
      content.push_back(row(r, triggerWidth, t));
    }

    return PopupView::listPanel("Help", "Esc close", std::move(content), scrollIndex,
                                availableHeight, t);
  }

} // namespace logutils::HelpView
