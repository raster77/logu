#include "ViewerController.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "../theme/ThemeCatalog.hpp"
#include "../util/Clipboard.hpp"
#include "../util/PathUtil.hpp"
#include "../util/TextUtil.hpp"
#include "../util/UserConfig.hpp"

namespace logutils {

  ViewerController::ViewerController(
      LogDocument &document, const CommandRegistry &commands, std::string initialTheme)
            : mDocument(document), mCommands(commands), mThemeName(std::move(initialTheme)) {
  }

  std::string &ViewerController::commandInput() {
    return mCommandInput;
  }

  int &ViewerController::commandCursorPosition() {
    return mCommandCursor;
  }

  const LogDocument &ViewerController::document() const {
    return mDocument;
  }

  bool ViewerController::helpVisible() const {
    return mHelpVisible;
  }

  int ViewerController::helpScrollIndex() const {
    return mHelpScrollIndex;
  }

  void ViewerController::helpScrollUp(int step) {
    mHelpScrollIndex = std::max(0, mHelpScrollIndex - step);
  }

  void ViewerController::helpScrollDown(int step, int maxIndex) {
    mHelpScrollIndex = std::min(maxIndex, mHelpScrollIndex + step);
  }

  bool ViewerController::statsVisible() const {
    return mStatsVisible;
  }

  int ViewerController::statsScrollIndex() const {
    return mStatsScrollIndex;
  }

  void ViewerController::statsScrollUp(int step) {
    mStatsScrollIndex = std::max(0, mStatsScrollIndex - step);
  }

  void ViewerController::statsScrollDown(int step, int maxIndex) {
    mStatsScrollIndex = std::min(maxIndex, mStatsScrollIndex + step);
  }

  const std::vector<LogStats::LevelCount> &ViewerController::statsCounts() const {
    return mStatsCounts;
  }

  std::size_t ViewerController::statsTotalEntries() const {
    return mStatsTotalEntries;
  }

  const std::string &ViewerController::statusMessage() const {
    return mStatusMessage;
  }

  std::uint64_t ViewerController::statusMessageGeneration() const {
    return mStatusMessageGeneration;
  }

  void ViewerController::clearStatusMessageIfCurrent(std::uint64_t generation) {
    if (generation == mStatusMessageGeneration) {
      mStatusMessage.clear();
    }
  }

  int ViewerController::suggestionSelectedIndex() const {
    return mSuggestionSelectedIndex;
  }

  std::vector<const CommandDef*> ViewerController::currentSuggestions() const {
    return mCommands.filterCommands(mCommandInput);
  }

  const std::string &ViewerController::themeName() const {
    return mThemeName;
  }

  bool ViewerController::themePickerVisible() const {
    return mThemePickerVisible;
  }

  int ViewerController::themePickerIndex() const {
    return mThemePickerIndex;
  }

  void ViewerController::themePickerMoveUp() {
    mThemePickerIndex = std::max(0, mThemePickerIndex - 1);
  }

  void ViewerController::themePickerMoveDown() {
    const int n = static_cast<int>(theme::names().size());
    mThemePickerIndex = std::min(n - 1, mThemePickerIndex + 1);
  }

  void ViewerController::confirmThemePicker() {
    const auto &names = theme::names();

    if (mThemePickerIndex >= 0 && mThemePickerIndex < static_cast<int>(names.size())) {
      mThemeName = names[static_cast<std::size_t>(mThemePickerIndex)];
      user_config::saveTheme(user_config::configDir(), mThemeName);
    }

    mThemePickerVisible = false;
  }

  void ViewerController::cancelThemePicker() {
    mThemePickerVisible = false;
  }

  void ViewerController::resetSuggestionIfNone() {
    if (!mCommands.hasSuggestions(mCommandInput)) {
      mSuggestionSelectedIndex = -1;
    }
  }

  void ViewerController::handleArrowUp() {
    const auto suggestions = mCommands.filterCommands(mCommandInput);

    if (suggestions.empty()) {
      mSuggestionSelectedIndex = -1;
      mDocument.scrollUp(1);
      return;
    }
    // Moves relative to the row the user can actually see highlighted, which
    // is the *effective* index -- a fully typed trigger or a lone candidate is
    // auto-selected for display while mSuggestionSelectedIndex is still -1
    // (see CommandRegistry::effectiveSelectedIndex). Stepping from the stored
    // -1 instead made the first Up jump to the last row and the first Down
    // appear to do nothing, both times landing somewhere other than one row
    // from what was highlighted.
    const int n = static_cast<int>(suggestions.size());
    const int current =
        mCommands.effectiveSelectedIndex(suggestions, mCommandInput, mSuggestionSelectedIndex);

    mSuggestionSelectedIndex = (current < 0) ? n - 1 : std::max(0, current - 1);
  }

  void ViewerController::handleArrowDown() {
    const auto suggestions = mCommands.filterCommands(mCommandInput);

    if (suggestions.empty()) {
      mSuggestionSelectedIndex = -1;
      mDocument.scrollDown(1);
      return;
    }

    const int n = static_cast<int>(suggestions.size());
    const int current = mCommands.effectiveSelectedIndex(suggestions, mCommandInput, mSuggestionSelectedIndex);

    mSuggestionSelectedIndex = std::min(n - 1, current + 1);
  }

  void ViewerController::handlePageUp(int pageSize) {
    resetSuggestionIfNone();
    mDocument.scrollUp(pageSize);
  }

  void ViewerController::handlePageDown(int pageSize) {
    resetSuggestionIfNone();
    mDocument.scrollDown(pageSize);
  }

  void ViewerController::handleHome() {
    resetSuggestionIfNone();
    mDocument.scrollToStart();
  }

  void ViewerController::handleEnd() {
    resetSuggestionIfNone();
    mDocument.scrollToEnd();
  }

  bool ViewerController::handleEscape() {
    if (mHelpVisible) {
      mHelpVisible = false;
      return true;
    }

    if (mStatsVisible) {
      mStatsVisible = false;
      return true;
    }

    // Anything typed, not just an open suggestion list: the list disappears as
    // soon as the first space is typed, which used to leave "/filter foo"
    // with no way to abandon it from the keyboard at all.
    if (!mCommandInput.empty()) {
      mSuggestionSelectedIndex = -1;
      mCommandInput.clear();
      mCommandCursor = 0;
      return true;
    }

    return false;
  }

  KeyAction ViewerController::handleSuggestionReturn() {
    const auto suggestions = mCommands.filterCommands(mCommandInput);

    if (suggestions.empty()) {
      return KeyAction::NotHandled;
    }

    const int n = static_cast<int>(suggestions.size());

    if (mSuggestionSelectedIndex >= n) {
      mSuggestionSelectedIndex = n - 1;
    }

    const int effectiveIdx = mCommands.effectiveSelectedIndex(suggestions, mCommandInput, mSuggestionSelectedIndex);

    if (effectiveIdx < 0) {
      // No row highlighted: let the caller fall back to a plain submit.
      return KeyAction::NotHandled;
    }

    const CommandDef &selected = *suggestions[static_cast<std::size_t>(effectiveIdx)];
    mCommandInput = selected.trigger;
    mSuggestionSelectedIndex = -1;

    // Completing rather than running, so the user can type the argument. The
    // test is "does this command take an argument at all", which makes a
    // command whose argument is *optional* need a second Enter to do anything:
    // "/theme" and "/clear" both open/act on their own, but Enter on the fully
    // typed name only turns it into "/theme ". Known wart rather than a bug --
    // the second Enter runs it -- and worth knowing about before changing this
    // branch. The CommandDefs already distinguish the two cases by convention
    // ("<text>" required, "[name]" optional), so `argHint.front() != '['` would
    // let an optional-argument command run straight away.
    if (!selected.argHint.empty()) {
      mCommandInput += ' ';
      mCommandCursor = static_cast<int>(mCommandInput.size());
      return KeyAction::Handled;
    }

    submitCommand();

    return mQuitRequested ? KeyAction::Quit : KeyAction::Handled;
  }

  KeyAction ViewerController::handleInputSubmit() {
    submitCommand();
    return mQuitRequested ? KeyAction::Quit : KeyAction::Handled;
  }

  void ViewerController::submitCommand() {
    if (!mCommandInput.empty()) {
      mCommands.dispatch(mCommandInput, *this);
    }

    mCommandInput.clear();
  }

  void ViewerController::applyFilter(std::string text) {
    // The rejection message needs the text, but only on the failure path --
    // build it there instead of copying every filter into the model.
    std::string rejected = "filter has no usable terms: \"" + text + "\"";

    if (!mDocument.setFilter(std::move(text))) {
      setStatusMessage(std::move(rejected));
      return;
    }

    setStatusMessage("");
  }

  void ViewerController::applyRegexpFilter(std::string pattern) {
    std::string rejected = "invalid regexp: \"" + pattern + "\"";

    if (!mDocument.setRegexpFilter(std::move(pattern))) {
      setStatusMessage(std::move(rejected));
      return;
    }

    setStatusMessage("");
  }

  void ViewerController::clearFilter() {
    mDocument.clearFilter();
    setStatusMessage("");
  }

  void ViewerController::applyFind(std::string text) {
    const bool hasTerm = !text.empty();
    const bool found = mDocument.setFindTerm(std::move(text));

    setStatusMessage((hasTerm && !found) ? "no matches for \"" + mDocument.findTerm() + "\"" : "");
  }

  void ViewerController::clearFind() {
    mDocument.setFindTerm("");
    setStatusMessage("");
  }

  void ViewerController::findNext() {
    if (mDocument.findTerm().empty()) {
      return;
    }

    setStatusMessage(mDocument.findNext() ? "" : "no matches for \"" + mDocument.findTerm() + "\"");
  }

  void ViewerController::findPrevious() {
    if (mDocument.findTerm().empty()) {
      return;
    }

    setStatusMessage(mDocument.findPrevious() ? "" : "no matches for \"" + mDocument.findTerm() + "\"");
  }

  void ViewerController::showHelp() {
    mHelpVisible = true;
    mHelpScrollIndex = 0;
    setStatusMessage("");
  }

  void ViewerController::showStats() {
    const auto headers = mDocument.visibleEntryHeaders();

    mStatsTotalEntries = headers.size();
    mStatsCounts = LogStats::countByLevel(headers);
    mStatsVisible = true;
    mStatsScrollIndex = 0;
    setStatusMessage("");
  }

  void ViewerController::quit() {
    mQuitRequested = true;
  }

  void ViewerController::setStatusMessage(std::string message) {
    mStatusMessage = std::move(message);
    ++mStatusMessageGeneration;
  }

  void ViewerController::copyCurrentEntry() {
    const std::string &content = mDocument.currentEntryContent();

    if (content.empty()) {
      setStatusMessage("nothing to copy");
      return;
    }

    clipboard::copyToClipboard(content);

    const auto lineCount = std::count(content.begin(), content.end(), '\n');

    setStatusMessage(
        "copied " + std::to_string(lineCount) + (lineCount == 1 ? " line to clipboard" : " lines to clipboard"));
  }

  void ViewerController::setTheme(std::string name) {
    const auto &names = theme::names();
    // Case-insensitive, like every other step the argument passed through to
    // get here (the trigger in ICommand::matches, the autocomplete list, the
    // selected-row match) -- "/theme Dracula" should not be rejected by the
    // one comparison at the end of that chain. The canonical spelling from the
    // catalog is what gets stored, so a theme like "openCode" is written back
    // to the config file the way byName() expects to read it.
    const auto match = std::find_if(names.begin(), names.end(),
        [&name](const std::string &candidate) { return text_util::iequals(candidate, name); });

    if (match == names.end()) {
      std::string list;

      for (const auto &n : names) {
        list += " " + n;
      }

      setStatusMessage("unknown theme: \"" + name + "\". Available:" + list);

      return;
    }

    mThemeName = *match;
    user_config::saveTheme(user_config::configDir(), mThemeName);
    setStatusMessage("");
  }

  void ViewerController::exportVisibleLines(std::string path) {
    if (path.empty()) {
      setStatusMessage("usage: /export <path>");
      return;
    }

    // Expanded here because nothing else will have: a path typed into the
    // viewer's command box never passed through a shell, so "~/out.log" would
    // otherwise be taken literally and fail against a "~" directory that
    // almost certainly doesn't exist.
    const std::filesystem::path target = path_util::expandUser(path);
    std::ofstream out(target, std::ios::out | std::ios::trunc);

    if (!out) {
      setStatusMessage("failed to open \"" + path + "\" for writing");
      return;
    }

    const auto lineCount = mDocument.visibleLineCount();

    for (std::size_t i = 0; i < lineCount; ++i) {
      out << mDocument.visibleLineAt(i) << '\n';
    }

    out.close();

    if (!out) {
      setStatusMessage("failed to write to \"" + path + "\"");
      return;
    }

    // The expanded path, not what was typed: "exported 12 lines to
    // /home/me/out.log" is where the file actually is.
    setStatusMessage("exported " + std::to_string(lineCount) +
                     (lineCount == 1 ? " line to " : " lines to ") + target.string());
  }

  bool ViewerController::reloadPromptVisible() const {
    return mReloadPromptVisible;
  }

  void ViewerController::requestReload() {
    mReloadPromptVisible = true;
  }

  bool ViewerController::beginReload() {
    mReloadPromptVisible = false;

    if (!mReloadHandler) {
      if (mReloadResolvedCallback) {
        mReloadResolvedCallback();
      }

      return false;
    }

    setStatusMessage("reloading...");

    return true;
  }

  ReloadResult ViewerController::performReload() const {
    ReloadResult result;
    // The handler re-reads files from disk, which can race with further
    // changes to the directory (e.g. a file removed right after being
    // modified) and throw. Catch it here, on the thread that called it, so it
    // can't escape into the event loop -- or, now that this runs off the UI
    // thread, into std::terminate.
    try {
      result = mReloadHandler();
    } catch (const std::exception &e) {
      result.error = std::string("reload failed: ") + e.what();
    }
    return result;
  }

  void ViewerController::finishReload(ReloadResult result) {
    if (!result.error.empty()) {
      setStatusMessage(std::move(result.error));
    } else {
      mDocument.reload(std::move(result.entries));
      std::string message = "working directory changed: reloaded " +
                            std::to_string(mDocument.totalLineCount()) + " lines";
      if (!result.warnings.empty()) {
        // Only the first warning fits a one-line status bar; the count tells
        // the user the rest exist rather than pretending they don't.
        message += " (" + result.warnings.front();
        if (result.warnings.size() > 1) {
          message += "; " + std::to_string(result.warnings.size() - 1) + " more";
        }
        message += ")";
      }
      setStatusMessage(std::move(message));
    }

    if (mReloadResolvedCallback) {
      mReloadResolvedCallback();
    }
  }

  void ViewerController::cancelReload() {
    mReloadPromptVisible = false;

    if (mReloadResolvedCallback) {
      mReloadResolvedCallback();
    }
  }

  void ViewerController::setReloadHandler(std::function<ReloadResult()> handler) {
    mReloadHandler = std::move(handler);
  }

  void ViewerController::setReloadResolvedCallback(std::function<void()> callback) {
    mReloadResolvedCallback = std::move(callback);
  }

  void ViewerController::openThemePicker() {
    const auto &names = theme::names();
    const auto it = std::find(names.begin(), names.end(), mThemeName);

    mThemePickerIndex = (it != names.end()) ? static_cast<int>(std::distance(names.begin(), it)) : 0;
    mThemePickerVisible = true;

    setStatusMessage("");
  }

} // namespace logutils
