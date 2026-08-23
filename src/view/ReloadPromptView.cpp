#include "ReloadPromptView.hpp"

#include <utility>

#include <ftxui/screen/terminal.hpp>

#include "PopupView.hpp"

namespace logutils::ReloadPromptView {

  ftxui::Elements build(const theme::Theme &t) {
    using namespace ftxui;

    Elements rows;
    rows.push_back(PopupView::titleRow("Working directory changed", "", t));
    rows.push_back(separator() | color(t.dimText));
    rows.push_back(text("A file was added, removed, or modified.") | color(t.text));
    rows.push_back(text("Re-merge and reload now?") | color(t.text));
    rows.push_back(separator() | color(t.dimText));
    rows.push_back(text(" Enter/y: reload   Esc/n: not now ") | color(t.dimText));

    // No scrollable body: the prompt is a fixed handful of rows, so it only
    // needs the shared box around them.
    return PopupView::frame(std::move(rows), Terminal::Size().dimx / 2, t);
  }

} // namespace logutils::ReloadPromptView
