#include "PopupView.hpp"

#include <algorithm>
#include <utility>

#include <ftxui/screen/terminal.hpp>

namespace logutils::PopupView {

  ftxui::Element titleRow(const std::string &title, const std::string &hint, const theme::Theme &t) {
    using namespace ftxui;

    if (hint.empty()) {
      return text(" " + title + " ") | bold | color(t.text);
    }

    return hbox({
      text(" " + title + " ") | bold | color(t.text),
          filler(),
          text(" " + hint + " ") | color(t.dimText),
    });
  }

  int contentBudget(int availableHeight, int chromeRows) {
    // Never below three rows: a panel squeezed to nothing is worse than one
    // that briefly overflows a very short terminal.
    return std::max(3, availableHeight - chromeRows);
  }

  ftxui::Element scrollable(ftxui::Elements content, int budget) {
    using namespace ftxui;
    // LESS_THAN (not EQUAL) so short content isn't padded out to the budget --
    // it only kicks in once the content is actually taller.
    return vbox(std::move(content)) | vscroll_indicator | yframe | size(HEIGHT, LESS_THAN, budget);
  }

  ftxui::Element labelColumn(const std::string &label, std::size_t width, const theme::Theme &t) {
    using namespace ftxui;

    return hbox({text(label), filler()})
        | size(WIDTH, EQUAL, static_cast<int>(width))
        | bold | color(t.accent);
  }

  ftxui::Elements listPanel(const std::string &title, const std::string &hint,
      ftxui::Elements body, std::size_t focusIndex, int availableHeight, const theme::Theme &t) {
    using namespace ftxui;

    // Title row + separator + the box's top and bottom border.
    constexpr int kChromeRows = 4;
    constexpr int kMaxWidth = 72;

    if (!body.empty()) {
      const std::size_t focusIdx = std::min(focusIndex, body.size() - 1);
      body[focusIdx] = body[focusIdx] | focus;
    }

    return frame(
        {
          titleRow(title, hint, t),
          separator() | color(t.dimText),
          scrollable(std::move(body), contentBudget(availableHeight, kChromeRows)),
        },
        kMaxWidth, t);
  }

  ftxui::Elements frame(ftxui::Elements rows, int maxWidth, const theme::Theme &t) {
    using namespace ftxui;

    const int termWidth = Terminal::Size().dimx;
    // The -8 leaves the popup clear of the edges; the max() keeps the width
    // positive on a terminal too narrow for even that.
    const int popupWidth = std::max(1, std::min(termWidth - 8, maxWidth));

    return {
      vbox(std::move(rows))
          | size(WIDTH, EQUAL, popupWidth)
          | border
          | color(t.dimText)
          | bgcolor(t.background)
          | center,
    };
  }

} // namespace logutils::PopupView
