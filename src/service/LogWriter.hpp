#ifndef SERVICE_LOGWRITER_HPP_
#define SERVICE_LOGWRITER_HPP_

#include <ostream>
#include <string>
#include <vector>

#include "../model/LogEntry.hpp"

namespace logutils {

  /** @brief Abstraction over serializing entries to an output stream. */
  class ILogWriter {
    public:
      virtual ~ILogWriter() = default;
      /**
       * @brief Writes {@code entries} to {@code out}.
       * @param out destination stream.
       * @param entries the entries to serialize, in the order to write them.
       */
      virtual void write(std::ostream &out, const std::vector<LogEntry> &entries) const = 0;
  };

  /** @brief Serializes entries back to plain text, one line per line. */
  class PlainTextLogWriter : public ILogWriter {
    public:
      void write(std::ostream &out, const std::vector<LogEntry> &entries) const override;
  };

} // namespace logutils

#endif /* SERVICE_LOGWRITER_HPP_ */
