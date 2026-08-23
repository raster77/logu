#ifndef SERVICE_LOGMERGER_HPP_
#define SERVICE_LOGMERGER_HPP_

#include <vector>

#include "../model/LogEntry.hpp"

namespace logutils::LogMerger {

  /**
   * @brief Stable-sorts entries chronologically by {@link LogEntry#sortKey}.
   *
   * Entries with equal timestamps keep their original order, so when merging
   * several files' entries (already concatenated by the caller), ties break
   * in favor of whichever file was concatenated first. A no-op when the input
   * is already sorted (e.g. a single already-merged file).
   *
   * @param entries entries to sort in place.
   */
  void sortChronologically(std::vector<LogEntry> &entries);

} // namespace logutils::LogMerger

#endif /* SERVICE_LOGMERGER_HPP_ */
