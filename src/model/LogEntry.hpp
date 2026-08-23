#ifndef MODEL_LOGENTRY_HPP_
#define MODEL_LOGENTRY_HPP_

#include <string>
#include <vector>

namespace logutils {

  /**
   * @brief Joins a range of lines into the one text form an entry has
   * wherever it leaves the model -- clipboard, export, plain output.
   *
   * Each line is followed by {@code '\n'}, including the last. Shared by
   * {@link LogEntry#content} and {@link LogDocument}, which holds its lines
   * flattened across all entries and so joins a sub-range of its own storage
   * rather than a {@link LogEntry}'s vector. A template over the iterators
   * for exactly that reason: the two callers hold the same strings in
   * different containers.
   *
   * @param first iterator to the first line.
   * @param last iterator past the last line.
   * @return the joined text, one trailing {@code '\n'} per line.
   */
  template <typename It>
  std::string joinLines(It first, It last) {
    std::string joined;

    for (It it = first; it != last; ++it) {
      joined += *it;
      joined += '\n';
    }

    return joined;
  }

  /**
   * @brief One entry of a merged log.
   *
   * A log file is a sequence of entries; an entry starts with a line that has
   * a timestamp and may continue on lines without one (e.g. an exception
   * stack trace), which stay grouped with it when sorting.
   */
  struct LogEntry {
      /** Key used to chronologically sort entries across merged files. */
      std::string sortKey;
      /** The header line followed by any continuation lines. */
      std::vector<std::string> lines;
      /**
       * Length of the raw timestamp text at the start of {@link #lines}[0],
       * as found by {@code TimestampFormat::match()}; 0 if {@code lines[0]}
       * has no recognized timestamp (e.g. leading lines before the first
       * matching line). Lets the viewer color just that span without
       * re-detecting it per frame.
       */
      std::size_t timestampLength = 0;

      /** @return this entry's lines joined into one text via {@link joinLines}. */
      std::string content() const;
  };

} // namespace logutils

#endif /* MODEL_LOGENTRY_HPP_ */
