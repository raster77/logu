#include "ThemePickerView.hpp"

#include <utility>

#include <ftxui/screen/terminal.hpp>

#include "../theme/ThemeCatalog.hpp"
#include "PopupView.hpp"

namespace logutils::ThemePickerView {

  ftxui::Elements build(int selectedIndex, const theme::Theme &t, int availableHeight) {
    using namespace ftxui;

    const auto &names = theme::names();

    // The theme list is the scrollable part, wrapped in its own fixed-height
    // frame below so the title/border stay put and only this content
    // scrolls.
    Elements content;

    for (int i = 0; i < static_cast<int>(names.size()); ++i) {
      const bool sel = (i == selectedIndex);
      const Color rowBg = sel ? t.selectedBg : t.background;
      const Color rowText = sel ? t.selectedText : t.text;

      Element rowElement = hbox({
        text(sel ? " > " : "   ") | color(rowText),
            text(names[static_cast<std::size_t>(i)]) | color(rowText),
            filler(),
      }) | bgcolor(rowBg);
      // Focused so the content frame scrolls the selection into view when
      // there are more themes than fit on screen.
      content.push_back(sel ? rowElement | focus : rowElement);
    }

    if (content.empty()) {
      content.push_back(text(""));
    }

    // title + 2 separators + key hint + top/bottom border
    constexpr int kChromeRows = 6;
    // Half the terminal rather than a fixed cap: the list is one short name
    // per row, so a wide fixed width would be mostly empty.
    const int maxWidth = ftxui::Terminal::Size().dimx / 2;

    // The key hint sits on its own row at the bottom, like the reload
    // prompt's. It doesn't fit beside the title the way "Esc close" does in
    // the help and stats panels: this popup is half the terminal wide, and a
    // right-aligned hint that long pushes the title clean off the row.
    return PopupView::frame(
        {
          PopupView::titleRow("Select theme", "", t),
          separator() | color(t.dimText),
          PopupView::scrollable(std::move(content), PopupView::contentBudget(availableHeight, kChromeRows)),
          separator() | color(t.dimText),
          text(" \u2191\u2193 move   Enter select   Esc cancel ") | color(t.dimText),
        },
        maxWidth, t);
  }

} // namespace logutils::ThemePickerView
