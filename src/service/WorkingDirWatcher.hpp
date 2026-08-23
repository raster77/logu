#ifndef SERVICE_WORKINGDIRWATCHER_HPP_
#define SERVICE_WORKINGDIRWATCHER_HPP_

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include "../util/DirectoryScan.hpp"

namespace logutils {

  /**
   * @brief Polls a {@code --working-dir} directory on a background thread for
   * added, removed, or modified regular files (excluding the merged output
   * file itself), so the viewer can offer to re-merge and reload when the
   * directory changes underneath it.
   *
   * Detection is a path+mtime snapshot diff rather than an OS file-system
   * watch (inotify/ReadDirectoryChangesW/FSEvents), trading a bit of latency
   * for one portable implementation.
   *
   * {@link #start}/{@link #stop} are not safe to call concurrently with each
   * other, and {@link #setOnChange} must be called before {@link #start};
   * everything else is safe to call from any thread.
   */
  class WorkingDirWatcher {
    public:
      /**
       * @brief Constructs a watcher for {@code dir}.
       * @param dir the directory to poll.
       * @param excludePath path to exclude from change detection (the merged output file).
       * @param interval polling interval.
       */
      WorkingDirWatcher(std::string dir, std::string excludePath,
                        std::chrono::milliseconds interval = std::chrono::seconds(2));
      ~WorkingDirWatcher();

      WorkingDirWatcher(const WorkingDirWatcher &) = delete;
      WorkingDirWatcher &operator=(const WorkingDirWatcher &) = delete;

      /**
       * @brief Sets the callback invoked on the watcher's background thread
       * once a change is detected.
       *
       * The watcher then pauses detection (so a single change doesn't fire
       * repeatedly while it's being handled) until {@link #resume} is called.
       *
       * @param onChange the callback to invoke.
       */
      void setOnChange(std::function<void()> onChange);

      /** @brief Starts the background polling thread. */
      void start();
      /** @brief Stops the background polling thread, joining it. */
      void stop();

      /**
       * @brief Takes a fresh snapshot as the new baseline and re-arms detection.
       *
       * Called once a reported change has been handled (accepted or
       * declined), so the same change isn't reported twice.
       */
      void resume();

    private:
      using Snapshot = std::map<std::string, std::filesystem::file_time_type>;

      struct Scan {
          Snapshot files;
          // What this round couldn't read: directories the walk had to give
          // up on (see dir_scan::forEachRegularFile) plus files whose
          // timestamp wouldn't come back. Non-zero means `files` is missing
          // entries it would otherwise have, so it can't be compared against
          // a complete snapshot without reporting changes nothing made --
          // and, for a file dropped twice in a row, without hiding a change
          // that did happen.
          std::size_t skipped = 0;
      };

      Scan takeSnapshot() const;
      void loop();

      std::string mDir;
      // Built once here rather than per snapshot: takeSnapshot() runs every
      // poll interval, and this is the same exclusion the merge applies.
      dir_scan::FileExclusion mExcluded;
      std::chrono::milliseconds mInterval;
      std::function<void()> mOnChange;

      std::thread mThread;

      std::mutex mMutex;
      std::condition_variable mCv;
      bool mRunning = false;
      Snapshot mLastSnapshot;
      bool mPaused = false;
  };

} // namespace logutils

#endif /* SERVICE_WORKINGDIRWATCHER_HPP_ */
