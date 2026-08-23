#ifndef VIEW_TERMINALVIEW_HPP_
#define VIEW_TERMINALVIEW_HPP_

#include "../command/CommandRegistry.hpp"
#include "../controller/ViewerController.hpp"
#include "../service/WorkingDirWatcher.hpp"

namespace logutils {

  /**
   * @brief The viewer's View: an ftxui full-screen TUI.
   *
   * Renders whatever {@code ViewerController}/{@code LogDocument} currently
   * say (log list or help, command suggestions, status/footer), and
   * translates ftxui input events into {@code ViewerController} calls. Holds
   * no business logic of its own -- layout and widget composition only,
   * delegated to the {@code HelpView}/{@code LogListView}/
   * {@code CommandSuggestionsView}/{@code StatusBarView} sub-views for each region.
   *
   * Typing {@code "/"} brings up a filterable list of commands, navigable
   * with Up/Down and accepted with Enter (Escape dismisses it). Typing
   * {@code "/filter <text>"} and pressing Enter applies a substring filter;
   * {@code "/clear"} (or {@code "/clear all"}) resets it and the find
   * highlight, {@code "/clear filter"} or {@code "/clear find"} resets just
   * one of them; {@code "/find <text>"} jumps to and highlights the next
   * match, with Ctrl+N/Ctrl+P repeating the search forward/backward;
   * {@code "/help"} replaces the log list with a command/shortcut reference
   * until Esc closes it. Up/Down otherwise scrolls one line at a time,
   * PageUp/PageDown scrolls by the visible height; {@code "/quit"} exits.
   */
  class TerminalView {
    public:
      /**
       * @brief Constructs a view over {@code controller} and {@code commands}.
       * @param controller the controller to drive and read state from; must outlive this view.
       * @param commands the command registry, used to render autocomplete/help; must outlive this view.
       */
      TerminalView(ViewerController &controller, const CommandRegistry &commands);

      /**
       * @brief Wires a {@link WorkingDirWatcher} to the session.
       *
       * {@code onChange} (fired on the watcher's background thread) is
       * marshalled onto the ftxui event loop and reported to the controller
       * as {@code requestReload()}. The watcher is started when
       * {@link #run} begins and stopped before it returns; call this before
       * {@link #run}.
       *
       * @param watcher the watcher to wire in; must outlive {@link #run}.
       */
      void watchWorkingDir(WorkingDirWatcher &watcher);

      /** @brief Runs the full-screen event loop until the user quits. */
      void run();

    private:
      ViewerController &mController;
      const CommandRegistry &mCommands;
      WorkingDirWatcher *mWatcher = nullptr;
  };

} // namespace logutils

#endif /* VIEW_TERMINALVIEW_HPP_ */
