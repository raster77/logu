#ifndef COMMAND_COMMANDS_HPP_
#define COMMAND_COMMANDS_HPP_

#include <memory>
#include <string>
#include <vector>

#include "ICommand.hpp"

namespace logutils {

  /**
   * @brief A command invoked as {@code "/name <argument>"}, e.g. {@code "/filter error"}.
   *
   * Recognizing the trigger and pulling the argument out of the raw input is
   * identical for every such command, so it's written once here and each
   * command supplies only what to do with the argument it gets.
   */
  class PrefixCommand : public ICommand {
    public:
      /**
       * @brief Callback invoked with the trigger's argument.
       *
       * Receives everything that followed the trigger, with surrounding
       * spaces removed (empty when the command was submitted on its own, or
       * followed by nothing but spaces). Trimming both ends rather than just
       * the leading one keeps a stray trailing keystroke from silently
       * becoming part of a path, a theme name, or a search term -- none of
       * which the viewer shows the user, so {@code "no matches for \"timeout \""}
       * would read as a bug in the tool. A plain function pointer rather than
       * {@code std::function}: every action is a captureless one-liner, so
       * there's nothing to own.
       */
      using Action = void (*)(IViewerActions &actions, std::string argument);

      /**
       * @brief Constructs a command bound to {@code definition} and {@code action}.
       * @param definition must outlive the command; the built-ins point at
       * namespace-scope constants in {@code Commands.cpp}, which
       * {@link CommandRegistry} also hands out by pointer (see {@code filterCommands}).
       * @param action callback run with the trimmed argument on {@link #execute}.
       */
      PrefixCommand(const CommandDef &definition, Action action);

      const CommandDef &definition() const override {
        return mDefinition;
      }

      /**
       * @brief Whether {@code input} invokes this command.
       *
       * Matches the trigger followed by either end of input or a space, so
       * {@code "/find"} and {@code "/find foo"} match but {@code "/findme"}
       * doesn't. Case-insensitive on the trigger, matching the autocomplete
       * list (see {@code CommandRegistry::filterCommands}), so
       * {@code "/FIND foo"} runs the same command {@code "/find foo"} does.
       * The argument itself is passed through untouched -- only the command
       * name is case-folded.
       *
       * @param input the raw command-box text.
       * @return {@code true} if {@code input} invokes this command.
       */
      bool matches(const std::string &input) const override;

      void execute(IViewerActions &actions, const std::string &input) const override;

    private:
      const CommandDef &mDefinition;
      Action mAction;
  };

  /**
   * @brief A command that takes no argument, e.g. {@code "/help"}.
   *
   * Only recognized on its own (case-insensitively, as with
   * {@link PrefixCommand}), give or take trailing spaces, which are ignored
   * for the same reason {@link PrefixCommand} trims them off an argument.
   * Trailing *text* makes the input a different, unknown command rather than
   * this one with a stray argument.
   */
  class ExactCommand : public ICommand {
    public:
      /** @brief Callback invoked when this command is executed. */
      using Action = void (*)(IViewerActions &actions);

      /**
       * @brief Constructs a command bound to {@code definition} and {@code action}.
       * @param definition must outlive the command.
       * @param action callback run on {@link #execute}.
       */
      ExactCommand(const CommandDef &definition, Action action);

      const CommandDef &definition() const override {
        return mDefinition;
      }

      bool matches(const std::string &input) const override;

      void execute(IViewerActions &actions, const std::string &input) const override;

    private:
      const CommandDef &mDefinition;
      Action mAction;
  };

  /**
   * @brief Every built-in command, in registration order.
   *
   * Registration order is also the order autocomplete offers them and
   * {@code /help} lists them. A command whose behavior doesn't fit either
   * {@link PrefixCommand} or {@link ExactCommand} above can still be its own
   * {@link ICommand} implementation and be added to this list.
   *
   * @return the full set of built-in commands, ready for {@link CommandRegistry}.
   */
  std::vector<std::unique_ptr<ICommand>> builtInCommands();

} // namespace logutils

#endif /* COMMAND_COMMANDS_HPP_ */
