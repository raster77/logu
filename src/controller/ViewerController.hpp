#ifndef CONTROLLER_VIEWERCONTROLLER_HPP_
#define CONTROLLER_VIEWERCONTROLLER_HPP_

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "../command/CommandDefinition.hpp"
#include "../command/CommandRegistry.hpp"
#include "../command/IViewerActions.hpp"
#include "../model/LogDocument.hpp"
#include "../model/LogEntry.hpp"
#include "../service/LogStats.hpp"

namespace logutils {

  /**
   * @brief The outcome of a key event that may submit the command box, since
   * submitting can end the session ({@code "/quit"}).
   */
  enum class KeyAction { NotHandled, Handled, Quit };

  /**
   * @brief What a {@code --working-dir} reload produced.
   *
   * Carries its own diagnostics rather than letting the code that re-reads
   * the files print them: the viewer owns the terminal while a reload runs,
   * so anything written to stderr lands in the middle of the rendered frame.
   */
  struct ReloadResult {
      /** The freshly merged entries. */
      std::vector<LogEntry> entries;
      /**
       * Files or directories that couldn't be read, one message each. The
       * reload still succeeded -- it just covers less than it should.
       */
      std::vector<std::string> warnings;
      /**
       * Set when the reload failed outright (e.g. the working directory is
       * gone), in which case {@link #entries} is meaningless and the
       * document is left as it was.
       */
      std::string error;
  };

  /**
   * @brief The viewer's Controller: owns the command-box/help/suggestion
   * interaction state, applies key events to the Model ({@link LogDocument}),
   * and dispatches submitted commands through {@link CommandRegistry}.
   *
   * Framework-agnostic: it knows nothing about ftxui, so it can be driven by
   * any view that translates its own input events into these calls.
   */
  class ViewerController : public IViewerActions {
    public:
      /**
       * @brief Constructs a controller over {@code document} and {@code commands}.
       * @param document the model this controller mutates; must outlive it.
       * @param commands the command registry to dispatch submitted commands through; must outlive it.
       * @param initialTheme the theme name to select on startup.
       */
      ViewerController(LogDocument &document, const CommandRegistry &commands,
                       std::string initialTheme = "nord-dark");

      /** @return the command box's text buffer, bound directly to the view's text input widget. */
      std::string &commandInput();
      /** @return the command box's cursor position, bound directly to the view's text input widget. */
      int &commandCursorPosition();

      /** @return the underlying model. */
      const LogDocument &document() const;
      /** @return {@code true} if the help panel is open. */
      bool helpVisible() const;
      /**
       * @brief Scroll position within the help panel (row index into
       * {@code HelpView}'s body), reset to 0 each time {@link #showHelp} opens it.
       * @return the help panel's current row index. The view clamps it
       * against {@code HelpView::rowCount()}, which the caller also uses to
       * clamp {@link #helpScrollDown}'s target so the position can't run
       * away past the last row.
       */
      int helpScrollIndex() const;
      /** @brief Scrolls the help panel up by {@code step} rows. */
      void helpScrollUp(int step);
      /** @brief Scrolls the help panel down by {@code step} rows, clamped to {@code maxIndex}. */
      void helpScrollDown(int step, int maxIndex);

      /** @return {@code true} if the stats panel is open. */
      bool statsVisible() const;
      /**
       * @brief Snapshot taken by {@link #showStats}: level counts at the
       * moment {@code "/stats"} was invoked.
       * @return the snapshotted level counts.
       */
      const std::vector<LogStats::LevelCount> &statsCounts() const;
      /** @return the snapshotted total entry count, taken by {@link #showStats}. */
      std::size_t statsTotalEntries() const;
      /**
       * @brief Same idea as {@link #helpScrollIndex}, but for the stats panel
       * ({@code StatsView::rowCount()}).
       * @return the stats panel's current row index.
       */
      int statsScrollIndex() const;
      /** @brief Scrolls the stats panel up by {@code step} rows. */
      void statsScrollUp(int step);
      /** @brief Scrolls the stats panel down by {@code step} rows, clamped to {@code maxIndex}. */
      void statsScrollDown(int step, int maxIndex);
      /** @return the current status bar message. */
      const std::string &statusMessage() const;
      /**
       * @brief Generation counter for the status message.
       *
       * Bumped on every call to {@link #setStatusMessage}, including clears,
       * so a caller (the view, to auto-clear a notification after a few
       * seconds) can tell whether a message it saw is still the one showing,
       * or has since been replaced by something newer.
       *
       * @return the current status message generation.
       */
      std::uint64_t statusMessageGeneration() const;
      /**
       * @brief Clears the status message, but only if it's still the one
       * that was current when {@code generation} was captured.
       *
       * Otherwise a timer meant for an older message could wipe out a newer
       * one that replaced it.
       *
       * @param generation the generation captured via {@link #statusMessageGeneration}.
       */
      void clearStatusMessageIfCurrent(std::uint64_t generation);
      /** @return the effective selected index into {@link #currentSuggestions}. */
      int suggestionSelectedIndex() const;
      /** @return the command suggestions matching the current command box text. */
      std::vector<const CommandDef*> currentSuggestions() const;

      /** @return the active theme's name. */
      const std::string &themeName() const;
      /**
       * @brief Whether the interactive theme picker is open (opened by
       * {@code "/theme"} with no argument).
       *
       * While the picker is open it steals Up/Down/Enter from the log list,
       * same as the help panel steals all keys.
       *
       * @return {@code true} if the theme picker is open.
       */
      bool themePickerVisible() const;
      /** @return the currently highlighted row in the theme picker. */
      int themePickerIndex() const;
      /** @brief Moves the theme picker's highlighted row up by one. */
      void themePickerMoveUp();
      /** @brief Moves the theme picker's highlighted row down by one. */
      void themePickerMoveDown();
      /** @brief Applies the highlighted theme and closes the picker. */
      void confirmThemePicker();
      /** @brief Closes the picker without changing the active theme. */
      void cancelThemePicker();

      /**
       * @brief Whether the {@code --working-dir} change-detection prompt is open.
       *
       * Opened by {@link #requestReload} (called once {@code WorkingDirWatcher}
       * reports a change); steals all keys like the theme picker/help panel
       * while visible.
       *
       * @return {@code true} if the reload prompt is open.
       */
      bool reloadPromptVisible() const;
      /** @brief Opens the reload prompt. */
      void requestReload();

      /**
       * @brief Step 1 of 3: accepts the reload prompt on the UI thread and
       * puts the viewer in its "reloading" state.
       *
       * A reload runs in three steps rather than one call, because
       * re-reading a directory of logs takes long enough to be felt: doing
       * it inside the key handler would freeze the frame with nothing on
       * screen to say why.
       * <ol>
       *   <li>{@link #beginReload} accepts the prompt on the UI thread and
       *   puts the viewer in its "reloading" state.</li>
       *   <li>{@link #performReload} does the work. It's the only part meant
       *   to run off the UI thread, and touches no controller state -- it
       *   only reads the handler set before the session started.</li>
       *   <li>{@link #finishReload} applies that result back on the UI thread.</li>
       * </ol>
       *
       * @return {@code false} (already resolved, nothing to run) when no
       * handler is set.
       */
      bool beginReload();
      /** @brief Step 2 of 3: performs the re-parse/re-merge; safe to call off the UI thread. */
      ReloadResult performReload() const;
      /** @brief Step 3 of 3: applies {@code result} back on the UI thread. */
      void finishReload(ReloadResult result);

      /** @brief Declines the reload prompt without touching the document. */
      void cancelReload();
      /**
       * @brief Supplies the callback that performs the actual re-parse/re-merge.
       *
       * Called from {@link #performReload}, i.e. off the UI thread: it must
       * not touch the document, the view, or this controller. Exceptions
       * escaping it are caught and reported as a failed reload.
       *
       * @param handler the callback to run when the user accepts a reload.
       */
      void setReloadHandler(std::function<ReloadResult()> handler);
      /**
       * @brief Supplies a callback run once every reload is resolved
       * (finished, failed, or declined), on the UI thread (e.g. to resume the watcher).
       * @param callback the callback to run.
       */
      void setReloadResolvedCallback(std::function<void()> callback);

      /** @brief Handles the Up arrow key. */
      void handleArrowUp();
      /** @brief Handles the Down arrow key. */
      void handleArrowDown();
      /** @brief Handles the Page Up key, scrolling by {@code pageSize} lines. */
      void handlePageUp(int pageSize);
      /** @brief Handles the Page Down key, scrolling by {@code pageSize} lines. */
      void handlePageDown(int pageSize);
      /** @brief Handles the Home key. */
      void handleHome();
      /** @brief Handles the End key. */
      void handleEnd();

      /**
       * @brief Handles the Escape key.
       *
       * Closes the help or stats panel; failing that, clears whatever is in
       * the command box (and any suggestion selection with it).
       *
       * @return whether it did something, i.e. whether the key should be consumed.
       */
      bool handleEscape();

      /**
       * @brief Accepts the highlighted command suggestion on Enter, if any
       * suggestion list is open and a row is effectively selected.
       *
       * Note that accepting a command that takes an argument only
       * *completes* it -- {@code "/theme"} + Enter leaves {@code "/theme "}
       * in the box and waits, and it takes a second Enter to run it. That is
       * deliberate for {@code "/find <text>"}, whose argument is required,
       * but it also catches {@code "/theme [name]"} and
       * {@code "/clear [all|filter|find]"}, whose arguments are optional and
       * which are perfectly runnable bare. See the comment in the implementation.
       *
       * @return {@link KeyAction#NotHandled} when there's nothing to accept,
       * so the caller can fall back to a plain command-box submit.
       */
      KeyAction handleSuggestionReturn();

      /** @brief Submits whatever is currently in the command box. */
      KeyAction handleInputSubmit();

      // IViewerActions
      void applyFilter(std::string text) override;
      void applyRegexpFilter(std::string pattern) override;
      void clearFilter() override;
      void applyFind(std::string text) override;
      void clearFind() override;
      void findNext() override;
      void findPrevious() override;
      void showHelp() override;
      void showStats() override;
      void quit() override;
      void setStatusMessage(std::string message) override;
      void copyCurrentEntry() override;
      void setTheme(std::string name) override;
      void openThemePicker() override;
      void exportVisibleLines(std::string path) override;

    private:
      void submitCommand();
      // Drops the stored suggestion selection when no list is open. Shared by
      // every key that scrolls the document, since each of them leaves the
      // suggestion list behind.
      void resetSuggestionIfNone();

      LogDocument &mDocument;
      const CommandRegistry &mCommands;
      std::string mCommandInput;
      int mCommandCursor = 0;
      std::string mStatusMessage;
      std::uint64_t mStatusMessageGeneration = 0;
      bool mHelpVisible = false;
      int mHelpScrollIndex = 0;
      bool mStatsVisible = false;
      int mStatsScrollIndex = 0;
      std::vector<LogStats::LevelCount> mStatsCounts;
      std::size_t mStatsTotalEntries = 0;
      bool mQuitRequested = false;
      int mSuggestionSelectedIndex = -1;
      std::string mThemeName;
      bool mThemePickerVisible = false;
      int mThemePickerIndex = 0;
      bool mReloadPromptVisible = false;
      std::function<ReloadResult()> mReloadHandler;
      std::function<void()> mReloadResolvedCallback;
  };

} // namespace logutils

#endif /* CONTROLLER_VIEWERCONTROLLER_HPP_ */
