#ifndef SERVICE_LOGSTATS_HPP_
#define SERVICE_LOGSTATS_HPP_

#include <string>
#include <vector>

namespace logutils::LogStats {

  /** @brief Number of entries at a given log level. */
  struct LevelCount {
      /** Level name, e.g. {@code "ERROR"}, or {@code "(none)"} if unrecognized. */
      std::string level;
      /** Number of headers counted under {@link #level}. */
      int count;
  };

  /**
   * @brief Counts how many of {@code headers} contain a recognized log level token.
   *
   * {@code headers} is one per visible entry -- see
   * {@code LogDocument::visibleEntryHeaders()}. Recognizes
   * TRACE/DEBUG/INFO/WARN[ING]/ERROR/FATAL, case-insensitive, word-bounded.
   * Headers matching none of them are counted under {@code "(none)"}.
   *
   * This is computed on demand from already-parsed entries rather than
   * during parsing: level tokens aren't positioned consistently enough
   * across formats to detect once the way a timestamp is (see
   * {@link TimestampFormat}'s header comment on why that one's worth
   * precomputing), so this only pays the regex cost when {@code /stats} is
   * actually invoked, over whatever's currently visible.
   *
   * @param headers one header line per visible entry.
   * @return per-level counts, sorted by count descending.
   */
  std::vector<LevelCount> countByLevel(const std::vector<std::string> &headers);

} // namespace logutils::LogStats

#endif /* SERVICE_LOGSTATS_HPP_ */
