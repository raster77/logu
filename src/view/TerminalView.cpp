#include "TerminalView.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/screen/terminal.hpp>

#include "../theme/Theme.hpp"
#include "CommandSuggestionsView.hpp"
#include "HelpView.hpp"
#include "LogListView.hpp"
#include "ReloadPromptView.hpp"
#include "StatsView.hpp"
#include "StatusBarView.hpp"
#include "ThemePickerView.hpp"

namespace logutils {

  TerminalView::TerminalView(ViewerController &controller, const CommandRegistry &commands)
            : mController(controller), mCommands(commands) {
  }

  void TerminalView::watchWorkingDir(WorkingDirWatcher &watcher) {
    mWatcher = &watcher;
  }

  void TerminalView::run() {
    using namespace ftxui;

    auto screen = ScreenInteractive::Fullscreen();
    screen.TrackMouse(false);
    int pageSize = 10;

    // The watcher's onChange callback below captures `screen`, so the watcher
    // thread has to be stopped before `screen` goes out of scope -- including
    // when it goes out of scope because Loop() threw, which a plain call at
    // the end of run() would skip. Declared after `screen` so it's destroyed
    // first; stop() is a no-op on a watcher that was never started.
    struct WatcherGuard {
        WorkingDirWatcher *watcher;
        ~WatcherGuard() {
          if (watcher != nullptr) {
            watcher->stop();
          }
        }
    } watcherGuard{mWatcher};

    // Re-reading a directory of logs takes long enough to be felt, so it runs
    // here rather than inside the key handler: the handler would hold up the
    // event loop for the whole re-parse, freezing the frame -- including the
    // "reloading..." status it just set, which would never be drawn.
    //
    // Declared after `screen` (the worker posts back to it) and guarded like
    // the watcher, so an in-flight reload is joined before either goes away,
    // on the exception path too.
    std::thread reloadThread;

    struct ThreadGuard {
        std::thread &thread;

        ~ThreadGuard() {
          if (thread.joinable()) {
            thread.join();
          }
        }
    } reloadGuard{reloadThread};

    const auto startReload = [this, &screen, &reloadThread] {
      if (!mController.beginReload()) {
        return;
      }

      // The previous reload has necessarily finished (the watcher stays
      // paused until finishReload resumes it, so no second prompt can appear
      // before then); this just releases its thread handle.
      if (reloadThread.joinable()) {
        reloadThread.join();
      }

      reloadThread = std::thread([this, &screen] {
        // ftxui's Post takes a copyable closure, and a ReloadResult owns the
        // whole document -- hand it over through a shared_ptr rather than
        // copying every line.
        auto result = std::make_shared<ReloadResult>(mController.performReload());
        screen.Post([this, result] { mController.finishReload(std::move(*result)); });
        // Same reason as the watcher callback below: a bare Closure doesn't
        // mark the frame dirty, so without this the reloaded document would
        // only appear on the next keypress.
        screen.PostEvent(Event::Custom);
      });
    };

    if (mWatcher != nullptr) {
      mWatcher->setOnChange([this, &screen] {
        // Post the mutation as a Closure (runs on the main thread, so it's
        // thread-safe) but *also* post Event::Custom right after: FTXUI only
        // marks the frame dirty when handling an Event, not a bare Closure,
        // so without it the redraw would be silently skipped until the next
        // real keypress.
        screen.Post([this] { mController.requestReload(); });
        screen.PostEvent(Event::Custom);
      });

      mWatcher->start();
    }

    // Auto-clears a status/notification message (e.g. "copied N lines to
    // clipboard") a few seconds after it's shown, so it doesn't sit there
    // until something else happens to overwrite it. Runs on its own thread
    // rather than blocking the event loop for the duration: arm() just resets
    // a deadline and wakes the thread, so back-to-back notifications reuse
    // the one thread instead of piling up sleepers. Guarded the same way as
    // reloadThread/mWatcher above: it captures `screen`, so it must be torn
    // down before `screen` goes out of scope.
    struct StatusMessageTimer {
        ViewerController &controller;
        ScreenInteractive &screen;
        std::mutex mutex;
        std::condition_variable cv;
        bool stop = false;
        bool pending = false;
        std::uint64_t generation = 0;
        std::chrono::steady_clock::time_point deadline;
        std::thread thread;

        StatusMessageTimer(ViewerController &c, ScreenInteractive &s) : controller(c), screen(s) {
          thread = std::thread([this] { loop(); });
        }

        ~StatusMessageTimer() {
          {
            std::lock_guard<std::mutex> lock(mutex);
            stop = true;
          }

          cv.notify_one();
          thread.join();
        }

        // Arms (or re-arms) the timer to clear whichever message is current
        // as of `gen`, five seconds from now.
        void arm(std::uint64_t gen) {
          {
            std::lock_guard<std::mutex> lock(mutex);
            pending = true;
            generation = gen;
            deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
          }

          cv.notify_one();
        }

        void loop() {
          std::unique_lock<std::mutex> lock(mutex);

          while (!stop) {
            if (!pending) {
              cv.wait(lock);
              continue;
            }

            if (cv.wait_until(lock, deadline) == std::cv_status::timeout) {
              const std::uint64_t firedGeneration = generation;
              pending = false;
              lock.unlock();
              screen.Post([this, firedGeneration] { controller.clearStatusMessageIfCurrent(firedGeneration); });
              // A bare Closure doesn't mark the frame dirty (same reason the
              // reload/watcher callbacks above post this too), so without it
              // the cleared message would only disappear on the next keypress.
              screen.PostEvent(Event::Custom);
              lock.lock();
            }
            // Otherwise woken early by arm() (new deadline) or the destructor
            // (stop) -- loop back around and re-check either way.
          }
        }
    } statusMessageTimer(mController, screen);
    // Tracks the last generation this timer was told about, so the Renderer
    // below only calls arm() when the message actually changed, not on every
    // redraw.
    std::uint64_t lastNotifiedStatusGeneration = mController.statusMessageGeneration();

    InputOption inputOption;
    inputOption.content = &mController.commandInput();
    inputOption.cursor_position = &mController.commandCursorPosition();
    inputOption.placeholder = "\"/\" to run a command";
    inputOption.multiline = false;
    inputOption.on_enter = [&] {
      if (mController.handleInputSubmit() == KeyAction::Quit) {
        screen.ExitLoopClosure()();
      }
    };

    Component filterInput = Input(inputOption);

    Component root = Renderer(filterInput, [&] {
      const theme::Theme &t = theme::byName(mController.themeName());
      const std::vector<const CommandDef*> suggestions = mController.currentSuggestions();

      // (Re)arm the auto-clear timer whenever the status message changed
      // since the last frame -- a fresh 5-second countdown for a new/updated
      // message, nothing to do if it was cleared some other way.
      const std::uint64_t statusGeneration = mController.statusMessageGeneration();

      if (statusGeneration != lastNotifiedStatusGeneration) {
        lastNotifiedStatusGeneration = statusGeneration;
        if (!mController.statusMessage().empty()) {
          statusMessageTimer.arm(statusGeneration);
        }
      }

      // Constrain the list to the terminal's current width/height so long
      // lines wrap instead of being clipped, and PageUp/PageDown can scroll
      // by the actual visible height.
      const int contentWidth = std::max(10, Terminal::Size().dimx - 4);
      const int chromeRows = 6 + static_cast<int>(suggestions.size());
      pageSize = std::max(1, Terminal::Size().dimy - chromeRows);

      // Whichever panel is up replaces the log list; at most one is ever
      // visible, and they're tested in the order they take over the screen.
      const auto buildItems = [&]() -> Elements {
        if (mController.reloadPromptVisible()) {
          return ReloadPromptView::build(t);
        }

        if (mController.themePickerVisible()) {
          return ThemePickerView::build(mController.themePickerIndex(), t, pageSize);
        }

        if (mController.helpVisible()) {
          return HelpView::build(
              mCommands, t, static_cast<std::size_t>(mController.helpScrollIndex()), pageSize);
        }

        if (mController.statsVisible()) {
          return StatsView::build(mController.statsCounts(), mController.statsTotalEntries(), t,
                                  static_cast<std::size_t>(mController.statsScrollIndex()), pageSize);
        }

        return LogListView::build(mController.document(), pageSize, t);
      };

      Elements itemElements = buildItems();

      // vscroll_indicator draws a scrollbar reflecting the frame's actual
      // scroll state -- same element|vscroll_indicator|frame order ftxui's
      // own Dropdown component uses. Harmless for the popup views (Help,
      // Stats, etc.) since they already fit within this box and don't need
      // to scroll here; it only draws anything when the log list itself
      // overflows. vscroll_indicator only sets the glyph's character, not
      // its color (it "follows the content"), so without an explicit
      // color() wrapping it here -- unlike the popups, which pick up dimText
      // for free from PopupView::frame's outer color() -- the scrollbar
      // renders in the terminal's default foreground instead of the theme.
      Elements rows = {
          vbox(itemElements) | size(WIDTH, LESS_THAN, contentWidth) | vscroll_indicator |
              color(t.dimText) | frame | flex,
          separator() | color(t.dimText),
      };

      if (!suggestions.empty()) {
        const int selIdx = mCommands.effectiveSelectedIndex(
            suggestions, mController.commandInput(), mController.suggestionSelectedIndex());
        rows.push_back(CommandSuggestionsView::build(suggestions, selIdx, t));
      }

      const Color inputColor = mController.commandInput().empty() ? t.placeholderText : t.text;

      rows.push_back(hbox({
        text("> ") | color(t.accent),
            filterInput->Render() | flex | color(inputColor),
      }) | border | color(t.dimText));

      rows.push_back(text(StatusBarView::footer(mController.document())) | color(t.dimText));

      if (!mController.statusMessage().empty()) {
        rows.push_back(text(mController.statusMessage()) | color(t.errorText));
      }

      return vbox(rows) | bgcolor(t.background);
    });

    root = CatchEvent(root, [&](Event event) {
      if (mController.reloadPromptVisible()) {
        if (event == Event::Return || event == Event::Character('y') || event == Event::Character('Y')) {
          startReload();
          return true;
        }

        if (event == Event::Escape || event == Event::Character('n') || event == Event::Character('N')) {
          mController.cancelReload();
          return true;
        }
        return true;
      }

      if (mController.themePickerVisible()) {
        if (event == Event::Escape) {
          mController.cancelThemePicker();
          return true;
        }

        if (event == Event::ArrowUp) {
          mController.themePickerMoveUp();
          return true;
        }

        if (event == Event::ArrowDown) {
          mController.themePickerMoveDown();
          return true;
        }

        if (event == Event::Return) {
          mController.confirmThemePicker();
          return true;
        }

        return true;
      }

      // Help and stats are the same interaction -- scroll within bounds,
      // Escape closes, everything else is swallowed -- over different state.
      const auto handlePanelKey = [&](int maxIndex, const std::function<void(int)> &scrollUp,
                                      const std::function<void(int, int)> &scrollDown) {
        if (event == Event::Escape) {
          mController.handleEscape();
        } else if (event == Event::ArrowUp) {
          scrollUp(1);
        } else if (event == Event::ArrowDown) {
          scrollDown(1, maxIndex);
        } else if (event == Event::PageUp) {
          scrollUp(pageSize);
        } else if (event == Event::PageDown) {
          scrollDown(pageSize, maxIndex);
        }

        return true;
      };

      if (mController.helpVisible()) {
        return handlePanelKey(
            static_cast<int>(HelpView::rowCount(mCommands)) - 1,
            [this](int step) { mController.helpScrollUp(step); },
            [this](int step, int maxIndex) { mController.helpScrollDown(step, maxIndex); });
      }

      if (mController.statsVisible()) {
        return handlePanelKey(
            static_cast<int>(StatsView::rowCount(mController.statsCounts().size())) - 1,
            [this](int step) { mController.statsScrollUp(step); },
            [this](int step, int maxIndex) { mController.statsScrollDown(step, maxIndex); });
      }

      if (event == Event::ArrowUp) {
        mController.handleArrowUp();
        return true;
      }

      if (event == Event::ArrowDown) {
        mController.handleArrowDown();
        return true;
      }

      if (event == Event::Return) {
        switch (mController.handleSuggestionReturn()) {
          case KeyAction::Quit:
            screen.ExitLoopClosure()();
            return true;
          case KeyAction::Handled:
            return true;
          case KeyAction::NotHandled:
            break; // Fall through to the input's own on_enter submit.
        }
      }

      if (event == Event::Escape) {
        return mController.handleEscape();
      }

      if (event == Event::PageUp) {
        mController.handlePageUp(pageSize);
        return true;
      }

      if (event == Event::PageDown) {
        mController.handlePageDown(pageSize);
        return true;
      }

      // Not bound by default in FTXUI; these are the standard xterm CSI
      // sequences for Ctrl+Home/Ctrl+End (same modifier convention as
      // Event::ArrowUpCtrl et al.).
      static const Event CTRL_HOME = Event::Special("\x1B[1;5H");
      static const Event CTRL_END = Event::Special("\x1B[1;5F");

      if (event == CTRL_HOME) {
        mController.handleHome();
        return true;
      }

      if (event == CTRL_END) {
        mController.handleEnd();
        return true;
      }

      if (event == Event::CtrlN) {
        mController.findNext();
        return true;
      }

      if (event == Event::CtrlP) {
        mController.findPrevious();
        return true;
      }

      if (event == Event::CtrlY) {
        mController.copyCurrentEntry();
        return true;
      }

      return false;
    });

    filterInput->TakeFocus();
    screen.Loop(root);
    // watcherGuard stops the watcher from here on, on both the normal and the
    // exception path.
  }

} // namespace logutils
