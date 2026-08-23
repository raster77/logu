#ifndef COMMAND_COMMANDDEFINITION_HPP_
#define COMMAND_COMMANDDEFINITION_HPP_

#include <string>

namespace logutils {

  /** @brief Describes one command-box command for autocomplete/help display. */
  struct CommandDef {
      /** Command name including the leading slash, e.g. {@code "/filter"}. */
      std::string trigger;
      /** Argument placeholder shown next to the trigger, e.g. {@code "<text>"}. */
      std::string argHint;
      /** Human-readable description shown in {@code /help} and autocomplete. */
      std::string description;
  };

  /** @brief Describes one keyboard shortcut for {@code /help} display. */
  struct ShortcutDef {
      /** Key combination, e.g. {@code "Ctrl+Y"}. */
      std::string keys;
      /** Human-readable description of what the shortcut does. */
      std::string description;
  };

} // namespace logutils

#endif /* COMMAND_COMMANDDEFINITION_HPP_ */
