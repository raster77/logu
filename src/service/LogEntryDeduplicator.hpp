#ifndef SERVICE_LOGENTRYDEDUPLICATOR_HPP_
#define SERVICE_LOGENTRYDEDUPLICATOR_HPP_

#include <vector>

#include "../model/LogEntry.hpp"

namespace logutils {

  /**
   * @brief Drops entries whose full content (all lines) matches one already
   * seen, keeping the first occurrence.
   */
  class LogEntryDeduplicator {
    public:
      /**
       * @brief Removes duplicate entries, keeping the first occurrence of each.
       *
       * Takes entries by value so kept ones can be moved (rather than copied)
       * into the result; pass an rvalue (e.g. {@code std::move(merged)}) to
       * avoid an extra copy of the whole input at the call site.
       *
       * @param entries the entries to deduplicate.
       * @return the deduplicated entries, in their original relative order.
       */
      std::vector<LogEntry> dedupe(std::vector<LogEntry> entries) const;
  };

} // namespace logutils

#endif /* SERVICE_LOGENTRYDEDUPLICATOR_HPP_ */
