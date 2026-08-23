#ifndef CLI_APPOPTIONSPARSER_HPP_
#define CLI_APPOPTIONSPARSER_HPP_

#include <optional>

#include "AppOptions.hpp"

namespace logutils {

  /**
   * @brief Wraps CLI11 argument parsing/validation so the composition root
   * doesn't need to know about CLI11 directly.
   */
  class AppOptionsParser {
    public:
      /**
       * @brief Parses and validates {@code argc}/{@code argv} into {@code out}.
       *
       * @param argc argument count, as passed to {@code main}.
       * @param argv argument vector, as passed to {@code main}.
       * @param out populated with the parsed options on success.
       * @return {@code std::nullopt} on success; otherwise the process exit
       * code the caller should return immediately (parse error, {@code --help},
       * or validation failure).
       */
      static std::optional<int> parse(int argc, char **argv, AppOptions &out);
  };

} // namespace logutils

#endif /* CLI_APPOPTIONSPARSER_HPP_ */
