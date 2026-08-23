#include "LogEntry.hpp"

namespace logutils {

  std::string LogEntry::content() const {
    return joinLines(lines.begin(), lines.end());
  }

} // namespace logutils
