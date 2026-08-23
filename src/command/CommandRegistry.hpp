#ifndef COMMAND_COMMANDREGISTRY_HPP_
#define COMMAND_COMMANDREGISTRY_HPP_

#include <cstddef>
#include <memory>
#include <vector>

#include "CommandDefinition.hpp"
#include "ICommand.hpp"
#include "IViewerActions.hpp"

namespace logutils {

  /**
   * @brief Holds the set of known commands and keyboard shortcuts, and
   * provides the command-box autocomplete and dispatch logic on top of them.
   */
  class CommandRegistry {
    public:
      CommandRegistry();

      /**
       * @brief All registered command definitions, in registration order.
       *
       * Points into the registry's own storage rather than copying each
       * {@link CommandDef}: {@code /help} renders this every frame, and
       * {@code HelpView::rowCount()} asks for it on every key the panel handles.
       *
       * @return pointers into the registry's own storage, stable for its lifetime.
       */
      std::vector<const CommandDef*> definitions() const;
      /** @return all registered keyboard shortcuts, for {@code /help} display. */
      const std::vector<ShortcutDef> &shortcuts() const;

      /**
       * @brief Commands whose name starts with what's been typed after the {@code '/'}.
       *
       * Returned in registration order (the order {@code builtInCommands()}
       * adds them, which is also the order they're listed in {@code /help}).
       * Matching is case-insensitive, like {@link ICommand#matches}. Empty
       * once the user has typed a space, since at that point they're past the
       * command name and into its arguments.
       *
       * @param input the raw command-box text typed so far.
       * @return pointers into the registry's own storage (stable for its
       * lifetime) rather than copies; called every render frame plus every
       * arrow/page/home/end/escape key.
       */
      std::vector<const CommandDef*> filterCommands(const std::string &input) const;

      /**
       * @brief Whether {@link #filterCommands} would return anything, without
       * building the vector to find out.
       *
       * Most callers only ask this question -- the scroll keys, which reset
       * the selection when the list is gone, and the render path, which
       * decides whether to draw the list at all -- and they ask it on every
       * keystroke and every frame.
       *
       * @param input the raw command-box text typed so far.
       * @return {@code true} if at least one command matches {@code input}.
       */
      bool hasSuggestions(const std::string &input) const;

      /**
       * @brief Number of registered commands.
       *
       * For callers sizing a layout around the list rather than reading it
       * (see {@code HelpView::rowCount}); {@link #definitions} would allocate
       * a vector of pointers just to have its size taken.
       *
       * @return the number of registered commands.
       */
      std::size_t commandCount() const;

      /**
       * @brief Determines the effective selected suggestion.
       *
       * Accounts for auto-selection (a fully typed trigger, case-insensitively,
       * or a single candidate) when no explicit index is set.
       *
       * @param commands the current suggestion list, as from {@link #filterCommands}.
       * @param input the raw command-box text typed so far.
       * @param storedIndex the explicitly selected index, or a sentinel meaning "none".
       * @return the effective selected index into {@code commands}.
       */
      int effectiveSelectedIndex(const std::vector<const CommandDef*> &commands,
                                 const std::string &input, int storedIndex) const;

      /**
       * @brief Runs the matching registered command against raw input.
       *
       * Reports the input as unknown via {@code actions.setStatusMessage()}
       * if no command matches.
       *
       * @param input the raw command-box text to dispatch.
       * @param actions the viewer actions the matched command may invoke.
       */
      void dispatch(const std::string &input, IViewerActions &actions) const;

    private:
      std::vector<std::unique_ptr<ICommand>> mCommands;
      std::vector<ShortcutDef> mShortcuts;
      // Lowercased trigger (without the leading '/') for each entry in
      // mCommands, precomputed once here instead of in filterCommands, which
      // would otherwise re-lowercase every trigger on every call.
      std::vector<std::string> mLowerTriggers;
  };

} // namespace logutils

#endif /* COMMAND_COMMANDREGISTRY_HPP_ */
