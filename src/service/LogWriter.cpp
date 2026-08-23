#include "LogWriter.hpp"

namespace logutils {

  void PlainTextLogWriter::write(std::ostream &out, const std::vector<LogEntry> &entries) const {
    for (const auto &entry : entries) {
      for (const auto &line : entry.lines) {
        out << line << '\n';
      }
    }
  }

} // namespace logutils
