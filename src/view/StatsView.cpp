#include "StatsView.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>

#include "PopupView.hpp"

namespace logutils::StatsView {

  namespace {

    // Rows build() emits that aren't a level: the "N visible entries" line,
    // the blank spacer, and the "By level" label. rowCount() and build() both
    // derive from this one constant, so the two can't disagree about the
    // panel's height.
    constexpr std::size_t kFixedRows = 3;

    ftxui::Element row(
        const LogStats::LevelCount &c, std::size_t labelWidth, std::size_t totalEntries, const theme::Theme &t) {
      using namespace ftxui;

      const double pct =
          totalEntries == 0 ? 0.0 : (100.0 * static_cast<double>(c.count) / static_cast<double>(totalEntries));

      // "%f" always uses '.' as the decimal point in the "C" locale, which
      // this program never changes away from (see TextUtil.cpp's header
      // comment on why -- filtering shouldn't quietly depend on LC_*).
      char pctBuf[16];
      std::snprintf(pctBuf, sizeof(pctBuf), "[%6.2f%%]", pct);

      return hbox({
        text("  "),
            PopupView::labelColumn(c.level, labelWidth, t),
            text("  "),
            text(pctBuf) | color(t.accent),
            text(" " + std::to_string(c.count)) | color(t.text),
      });
    }

  } // namespace

  std::size_t rowCount(std::size_t levelCount) {
    return kFixedRows + levelCount;
  }

  ftxui::Elements build(const std::vector<LogStats::LevelCount> &counts, std::size_t totalEntries,
      const theme::Theme &t, std::size_t scrollIndex, int availableHeight) {
    using namespace ftxui;

    std::size_t labelWidth = 0;

    for (const auto &c : counts) {
      labelWidth = std::max(labelWidth, c.level.size());
    }

    // Everything below the title bar is the scrollable part; PopupView::
    // listPanel wraps it in its own fixed-height frame so the title and border
    // stay put and only this content scrolls.
    Elements content;

    content.push_back(text(" " + std::to_string(totalEntries) +
                           (totalEntries == 1 ? " visible entry" : " visible entries")) | color(t.text));
    content.push_back(text(""));
    content.push_back(text(" By level") | bold | color(t.text));

    for (const auto &c : counts) {
      content.push_back(row(c, labelWidth, totalEntries, t));
    }

    return PopupView::listPanel("Stats", "Esc close", std::move(content), scrollIndex, availableHeight, t);
  }

} // namespace logutils::StatsView
