#ifndef COMMAND_IVIEWERACTIONS_HPP_
#define COMMAND_IVIEWERACTIONS_HPP_

#include <string>

namespace logutils {

  /**
   * @brief The set of things a command is allowed to do to the viewer.
   *
   * Commands depend on this narrow interface rather than the concrete
   * controller, so the controller's other responsibilities (rendering state,
   * key handling) stay invisible to command implementations (Interface
   * Segregation Principle), and the controller can be swapped or mocked
   * without touching command code (Dependency Inversion Principle).
   */
  class IViewerActions {
    public:
      virtual ~IViewerActions() = default;

      /**
       * @brief Applies a substring filter, keeping only matching lines/entries.
       * @param text the substring to filter on.
       */
      virtual void applyFilter(std::string text) = 0;
      /**
       * @brief Applies a regular-expression filter.
       *
       * Reports the pattern as invalid via {@link #setStatusMessage} if it
       * doesn't compile, leaving the current filter (if any) untouched.
       *
       * @param pattern the regular expression to filter on.
       */
      virtual void applyRegexpFilter(std::string pattern) = 0;
      /** @brief Clears any active filter, restoring every line. */
      virtual void clearFilter() = 0;

      /**
       * @brief Jumps to and highlights the first match for {@code text}.
       *
       * Searches from the current position, wrapping around the ends. An
       * empty {@code text} clears the highlight.
       *
       * @param text the search term.
       */
      virtual void applyFind(std::string text) = 0;
      /** @brief Equivalent to {@code applyFind("")} -- clears the active find term/highlight. */
      virtual void clearFind() = 0;
      /**
       * @brief Jumps to the next match for the active find term (set via
       * {@link #applyFind}), wrapping around the end. No-op if there's no
       * active term.
       */
      virtual void findNext() = 0;
      /**
       * @brief Jumps to the previous match for the active find term (set via
       * {@link #applyFind}), wrapping around the start. No-op if there's no
       * active term.
       */
      virtual void findPrevious() = 0;

      /** @brief Opens the help panel listing commands and shortcuts. */
      virtual void showHelp() = 0;
      /**
       * @brief Snapshots the currently visible entries' log-level breakdown
       * for display (see {@code LogStats::countByLevel()}).
       *
       * A snapshot rather than a live view: like {@link #showHelp}, the panel
       * it opens steals all key input except Escape, so the visible set can't
       * change while it's open.
       */
      virtual void showStats() = 0;
      /** @brief Requests the viewer exit. */
      virtual void quit() = 0;
      /**
       * @brief Sets the status bar message.
       * @param message the text to display.
       */
      virtual void setStatusMessage(std::string message) = 0;

      /**
       * @brief Copies the full text of the entry the currently focused line
       * belongs to (including any continuation lines, e.g. a stack trace) to
       * the system clipboard. Reports the outcome via {@link #setStatusMessage}.
       */
      virtual void copyCurrentEntry() = 0;

      /**
       * @brief Sets the active theme by name, or reports it as unknown via
       * {@link #setStatusMessage} if it doesn't match a built-in theme.
       * @param name the theme name.
       */
      virtual void setTheme(std::string name) = 0;
      /** @brief Opens the interactive theme picker (triggered by {@code "/theme"} with no argument). */
      virtual void openThemePicker() = 0;

      /**
       * @brief Writes the currently visible lines (i.e. post-filter, or every
       * line if no filter is active) to the file at {@code path}, overwriting
       * it if it already exists. Reports the outcome via {@link #setStatusMessage}.
       * @param path destination file path.
       */
      virtual void exportVisibleLines(std::string path) = 0;
  };

} // namespace logutils

#endif /* COMMAND_IVIEWERACTIONS_HPP_ */
