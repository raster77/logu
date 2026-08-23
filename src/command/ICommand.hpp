#ifndef COMMAND_ICOMMAND_HPP_
#define COMMAND_ICOMMAND_HPP_

#include <string>

#include "CommandDefinition.hpp"
#include "IViewerActions.hpp"

namespace logutils {

  /**
   * @brief A single command box command (e.g. {@code "/filter"}, {@code "/quit"}).
   *
   * New commands are added by implementing this interface and registering an
   * instance with {@link CommandRegistry}, without modifying any dispatch
   * logic (Open/Closed Principle).
   */
  class ICommand {
    public:
      virtual ~ICommand() = default;

      /** @return this command's display/autocomplete definition. */
      virtual const CommandDef &definition() const = 0;

      /**
       * @brief Whether raw command-box input (e.g. {@code "/filter foo"}) is
       * recognized as an invocation of this command.
       * @param input the raw text typed into the command box.
       * @return {@code true} if this command should handle {@code input}.
       */
      virtual bool matches(const std::string &input) const = 0;

      /**
       * @brief Runs the command against the raw input, driving the viewer
       * through {@code actions}.
       * @param actions the viewer actions this command may invoke.
       * @param input the raw text typed into the command box.
       */
      virtual void execute(IViewerActions &actions, const std::string &input) const = 0;
  };

} // namespace logutils

#endif /* COMMAND_ICOMMAND_HPP_ */
