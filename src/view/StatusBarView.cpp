#include "StatusBarView.hpp"

#include <cstddef>

namespace logutils::StatusBarView {

  std::string footer(const LogDocument &document) {
    // 64-bit throughout rather than int: a merged log runs to millions of
    // lines, and the percentage below multiplies the scroll position by 100 --
    // which overflows a 32-bit int past ~21.5M lines, well inside the range a
    // --working-dir aggregation reaches, and would print a negative percent.
    const std::size_t total = document.visibleLineCount();
    const long long scrollOffset = document.scrollOffset();

    std::string footer = std::to_string(total) + "/" + std::to_string(document.totalLineCount()) + " lines";

    if (total == 0) {
      footer += "  |  line 0/0";
    } else {
      const long long percent =
          (total <= 1) ? 100 : (scrollOffset * 100 / (static_cast<long long>(total) - 1));
      footer += "  |  line " + std::to_string(scrollOffset + 1) + "/" + std::to_string(total) +
          " (" + std::to_string(percent) + "%)";
    }
    if (!document.filter().empty()) {
      footer += document.filterIsRegex() ? "  |  regexp: \"" + document.filter() + "\""
          : "  |  filter: \"" + document.filter() + "\"";
    }

    if (!document.findTerm().empty()) {
      footer += "  |  find: \"" + document.findTerm() + "\"";
    }

    footer += "  |  /help for help";
    return footer;
  }

} // namespace logutils::StatusBarView
