#include "Commands.hpp"

#include <cstddef>
#include <string_view>
#include <utility>

#include "../util/TextUtil.hpp"

namespace logutils {

  namespace {
    const CommandDef FILTER_DEF{"/filter", "<text>",
      "Filter lines by substring; combine terms with &&, ||, ! (blank clears)"};
    const CommandDef REGEXP_DEF{"/regexp", "<pattern>",
      "Filter lines by regular expression, case-insensitive; combine terms with &&, ||, ! "
      "(blank clears)"};
    const CommandDef FIND_DEF{
      "/find", "<text>", "Jump to and highlight the next match (blank clears; Ctrl+N/Ctrl+P repeat)"};
    const CommandDef CLEAR_DEF{"/clear", "[all|filter|find]",
      "Clear the filter and find highlight (blank or \"all\"), or just one of them"};
    const CommandDef HELP_DEF{"/help", "", "Show the command and keyboard shortcut reference"};
    const CommandDef QUIT_DEF{"/quit", "", "Exit the viewer"};
    const CommandDef EXIT_DEF{"/exit", "", "Exit the viewer (alias for /quit)"};
    const CommandDef THEME_DEF{"/theme", "[name]", "Choose a colour theme (blank opens the picker)"};
    const CommandDef COPY_DEF{
      "/copy", "", "Copy the focused entry to the clipboard, including its stack trace"};
    const CommandDef EXPORT_DEF{
      "/export", "<path>", "Write the displayed lines (filtered or not) to a file"};
    const CommandDef STATS_DEF{
      "/stats", "", "Show a log-level breakdown of the currently visible entries"};
  } // namespace

  namespace {

    // Whether `input` opens with `trigger`, ignoring case (see the comment on
    // PrefixCommand::matches for why the command name is case-folded).
    bool startsWithTrigger(const std::string &input, const std::string &trigger) {
      return input.size() >= trigger.size() &&
          text_util::iequals(std::string_view(input).substr(0, trigger.size()), trigger);
    }

  } // namespace

  PrefixCommand::PrefixCommand(const CommandDef &definition, Action action)
      : mDefinition(definition), mAction(action) {
  }

  bool PrefixCommand::matches(const std::string &input) const {
    const std::string &trigger = mDefinition.trigger;
    return startsWithTrigger(input, trigger) &&
        (input.size() == trigger.size() || input[trigger.size()] == ' ');
  }

  void PrefixCommand::execute(IViewerActions &actions, const std::string &input) const {
    // Only ever reached through a matches() that returned true, so the input
    // does start with the trigger and this substr is in range.
    const std::string rest = input.substr(mDefinition.trigger.size());
    const std::size_t start = rest.find_first_not_of(' ');
    if (start == std::string::npos) {
      mAction(actions, std::string());
      return;
    }
    const std::size_t end = rest.find_last_not_of(' ');
    mAction(actions, rest.substr(start, end - start + 1));
  }

  ExactCommand::ExactCommand(const CommandDef &definition, Action action)
      : mDefinition(definition), mAction(action) {
  }

  bool ExactCommand::matches(const std::string &input) const {
    const std::string &trigger = mDefinition.trigger;
    // Trailing spaces are a stray keystroke rather than an argument, and are
    // ignored the same way PrefixCommand::execute trims them off an argument.
    // Anything else after the trigger makes this a different, unknown command.
    return startsWithTrigger(input, trigger) &&
        input.find_first_not_of(' ', trigger.size()) == std::string::npos;
  }

  void ExactCommand::execute(IViewerActions &actions, const std::string & /*input*/) const {
    mAction(actions);
  }

  std::vector<std::unique_ptr<ICommand>> builtInCommands() {
    std::vector<std::unique_ptr<ICommand>> commands;

    const auto prefix = [&commands](const CommandDef &definition, PrefixCommand::Action action) {
      commands.push_back(std::make_unique<PrefixCommand>(definition, action));
    };
    const auto exact = [&commands](const CommandDef &definition, ExactCommand::Action action) {
      commands.push_back(std::make_unique<ExactCommand>(definition, action));
    };

    prefix(FILTER_DEF, [](IViewerActions &actions, std::string argument) {
      actions.applyFilter(std::move(argument));
    });
    prefix(REGEXP_DEF, [](IViewerActions &actions, std::string argument) {
      actions.applyRegexpFilter(std::move(argument));
    });
    prefix(FIND_DEF, [](IViewerActions &actions, std::string argument) {
      actions.applyFind(std::move(argument));
    });
    prefix(CLEAR_DEF, [](IViewerActions &actions, std::string argument) {
      if (argument.empty() || text_util::iequals(argument, "all")) {
        actions.clearFilter();
        actions.clearFind();
      } else if (text_util::iequals(argument, "filter")) {
        actions.clearFilter();
      } else if (text_util::iequals(argument, "find")) {
        actions.clearFind();
      } else {
        actions.setStatusMessage(
            "unknown clear target: \"" + argument + "\" (use all, filter, or find)");
      }
    });
    exact(HELP_DEF, [](IViewerActions &actions) { actions.showHelp(); });
    prefix(THEME_DEF, [](IViewerActions &actions, std::string argument) {
      // "/theme <name>" applies it directly; "/theme" alone opens the picker.
      if (argument.empty()) {
        actions.openThemePicker();
      } else {
        actions.setTheme(std::move(argument));
      }
    });
    exact(COPY_DEF, [](IViewerActions &actions) { actions.copyCurrentEntry(); });
    prefix(EXPORT_DEF, [](IViewerActions &actions, std::string argument) {
      actions.exportVisibleLines(std::move(argument));
    });
    exact(STATS_DEF, [](IViewerActions &actions) { actions.showStats(); });
    exact(QUIT_DEF, [](IViewerActions &actions) { actions.quit(); });
    exact(EXIT_DEF, [](IViewerActions &actions) { actions.quit(); });

    return commands;
  }

} // namespace logutils
