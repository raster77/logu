#include "LogMerger.hpp"

#include <algorithm>

namespace logutils::LogMerger {

  void sortChronologically(std::vector<LogEntry> &entries) {
    std::stable_sort(entries.begin(), entries.end(),
                     [](const LogEntry &a, const LogEntry &b) { return a.sortKey < b.sortKey; });
  }

} // namespace logutils::LogMerger
