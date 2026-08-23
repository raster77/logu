#include "CommandRegistry.hpp"

#include "../util/TextUtil.hpp"
#include "Commands.hpp"

namespace logutils {

  CommandRegistry::CommandRegistry() : mCommands(builtInCommands()) {
    mLowerTriggers.reserve(mCommands.size());
    for (const auto &cmd : mCommands) {
      mLowerTriggers.push_back(text_util::toLower(cmd->definition().trigger.substr(1)));
    }

    mShortcuts = {
        {"Up / Down", "Scroll one line"},
        {"PageUp / PageDown", "Scroll one page"},
        {"Ctrl+Home / Ctrl+End", "Jump to the first / last line"},
        {"Ctrl+N / Ctrl+P", "Next / previous /find match"},
        {"Ctrl+Y", "Copy the focused entry to the clipboard, including its stack trace"},
        {"Esc", "Close this help"},
    };
  }

  std::vector<const CommandDef*> CommandRegistry::definitions() const {
    std::vector<const CommandDef*> result;
    result.reserve(mCommands.size());

    for (const auto &cmd : mCommands) {
      result.push_back(&cmd->definition());
    }

    return result;
  }

  const std::vector<ShortcutDef> &CommandRegistry::shortcuts() const {
    return mShortcuts;
  }

  std::vector<const CommandDef*> CommandRegistry::filterCommands(const std::string &input) const {
    std::vector<const CommandDef*> result;

    if (input.empty() || input[0] != '/' || input.find(' ') != std::string::npos) {
      return result;
    }

    const std::string query = text_util::toLower(input.substr(1));

    for (std::size_t i = 0; i < mCommands.size(); ++i) {
      if (mLowerTriggers[i].rfind(query, 0) == 0) {
        result.push_back(&mCommands[i]->definition());
      }
    }

    return result;
  }

  bool CommandRegistry::hasSuggestions(const std::string &input) const {
    if (input.empty() || input[0] != '/' || input.find(' ') != std::string::npos) {
      return false;
    }

    const std::string query = text_util::toLower(input.substr(1));

    for (const std::string &trigger : mLowerTriggers) {
      if (trigger.rfind(query, 0) == 0) {
        return true;
      }
    }

    return false;
  }

  std::size_t CommandRegistry::commandCount() const {
    return mCommands.size();
  }

  int CommandRegistry::effectiveSelectedIndex(const std::vector<const CommandDef*> &commands,
                                              const std::string &input, int storedIndex) const {
    const int n = static_cast<int>(commands.size());

    if (storedIndex >= 0 && storedIndex < n) {
      return storedIndex;
    }

    for (int i = 0; i < n; ++i) {
      // Case-insensitive, like filterCommands above and ICommand::matches:
      // having typed "/QUIT" in full should highlight "/quit", not leave the
      // list unselected.
      if (text_util::iequals(commands[static_cast<std::size_t>(i)]->trigger, input)) {
        return i;
      }
    }

    if (n == 1) {
      return 0;
    }

    return -1;
  }

  void CommandRegistry::dispatch(const std::string &input, IViewerActions &actions) const {
    for (const auto &cmd : mCommands) {
      if (cmd->matches(input)) {
        cmd->execute(actions, input);
        return;
      }
    }

    // Built from the registered commands rather than spelled out, so the
    // hint can't drift from what actually exists.
    std::string known;
    for (const auto &cmd : mCommands) {
      const CommandDef &def = cmd->definition();
      known += known.empty() ? "" : ", ";
      known += def.trigger;
      if (!def.argHint.empty()) {
        known += ' ' + def.argHint;
      }
    }

    actions.setStatusMessage("unknown command: \"" + input + "\" (try: " + known + ")");
  }

} // namespace logutils
