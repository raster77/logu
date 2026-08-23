#include "LogEntryDeduplicator.hpp"

#include <functional>
#include <unordered_map>

namespace logutils {

  std::vector<LogEntry> LogEntryDeduplicator::dedupe(std::vector<LogEntry> entries) const {
    std::vector<LogEntry> result;
    result.reserve(entries.size());

    // Hash of each kept entry's content -> its index in `result`, so a
    // duplicate is confirmed by comparing content() against the
    // already-moved entry in `result` rather than keeping a second full
    // copy of every unique entry's content around just to compare against
    // (a real concern at merge-the-world log volumes). Collisions (several
    // indices sharing a hash) are rare but handled via multimap semantics.
    std::unordered_multimap<std::size_t, std::size_t> seenHashes;
    seenHashes.reserve(entries.size());
    const std::hash<std::string> hasher;

    for (auto &entry : entries) {
      const std::string content = entry.content();
      const std::size_t hash = hasher(content);

      const auto range = seenHashes.equal_range(hash);
      bool isDuplicate = false;

      for (auto it = range.first; it != range.second; ++it) {
        if (result[it->second].content() == content) {
          isDuplicate = true;
          break;
        }
      }

      if (isDuplicate) {
        continue;
      }

      seenHashes.emplace(hash, result.size());
      result.push_back(std::move(entry));
    }

    result.shrink_to_fit();
    return result;
  }

} // namespace logutils
