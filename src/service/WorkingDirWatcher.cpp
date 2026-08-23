#include "WorkingDirWatcher.hpp"

#include "../util/DirectoryScan.hpp"

namespace logutils {

  WorkingDirWatcher::WorkingDirWatcher(std::string dir, std::string excludePath,
                                       std::chrono::milliseconds interval)
      : mDir(std::move(dir)), mExcluded(excludePath), mInterval(interval) {
  }

  WorkingDirWatcher::~WorkingDirWatcher() {
    stop();
  }

  void WorkingDirWatcher::setOnChange(std::function<void()> onChange) {
    mOnChange = std::move(onChange);
  }

  void WorkingDirWatcher::start() {
    {
      std::lock_guard<std::mutex> lock(mMutex);

      if (mRunning) {
        return;
      }

      mRunning = true;
      // Under the lock, unlike resume() below -- there's no poll thread yet to
      // contend with, and the baseline has to be in place before one starts.
      mLastSnapshot = takeSnapshot().files;
      mPaused = false;
    }

    mThread = std::thread(&WorkingDirWatcher::loop, this);
  }

  void WorkingDirWatcher::stop() {
    {
      std::lock_guard<std::mutex> lock(mMutex);

      if (!mRunning) {
        return;
      }

      mRunning = false;
    }
    // Wakes loop() out of its wait_for immediately instead of leaving it to
    // block for up to mInterval before it notices mRunning flipped.
    mCv.notify_all();

    if (mThread.joinable()) {
      mThread.join();
    }
  }

  void WorkingDirWatcher::resume() {
    // Snapshot outside the lock: it's a full recursive directory walk, and
    // resume() is called from the UI thread (confirmReload/cancelReload) --
    // holding mMutex across it would contend with the poll thread for no
    // reason.
    Scan fresh = takeSnapshot();
    std::lock_guard<std::mutex> lock(mMutex);

    mLastSnapshot = std::move(fresh.files);
    mPaused = false;
  }

  void WorkingDirWatcher::loop() {
    std::unique_lock<std::mutex> lock(mMutex);

    while (mRunning) {
      // Waits up to mInterval, but wakes immediately if stop() notifies
      // (predicate becomes true) instead of always sleeping the full
      // interval before exit.
      if (mCv.wait_for(lock, mInterval, [this] { return !mRunning; })) {
        return;
      }

      if (mPaused) {
        continue;
      }

      // Snapshot outside the lock -- see resume() for why.
      lock.unlock();
      Scan current = takeSnapshot();
      lock.lock();

      if (!mRunning) {
        return;
      }
      // An incomplete scan is missing files it would otherwise have listed,
      // so it differs from the baseline for a reason the user didn't cause.
      // Treat the round as inconclusive and wait for the next poll rather
      // than prompting to reload over a directory that merely couldn't be
      // read this time.
      if (current.skipped > 0 || current.files == mLastSnapshot) {
        continue;
      }

      mPaused = true;
      std::function<void()> fire = mOnChange;

      // Invoked outside the lock so onChange (which may call resume()
      // re-entrantly, e.g. if the handler runs synchronously) can't deadlock.
      lock.unlock();

      if (fire) {
        fire();
      }

      lock.lock();
    }
  }

  WorkingDirWatcher::Scan WorkingDirWatcher::takeSnapshot() const {
    Scan scan;

    scan.skipped = dir_scan::forEachRegularFile(
        mDir, [&](const std::filesystem::directory_entry &entry) {
          // The same exclusion the merge itself applies to this tree -- see
          // dir_scan::FileExclusion for why the two have to agree.
          if (mExcluded.matches(entry)) {
            return;
          }

          std::error_code mtimeEc;
          const auto mtime = entry.last_write_time(mtimeEc);
          if (mtimeEc) {
            // Counted, not dropped: a file silently missing from the snapshot
            // looks exactly like a deletion, and two failed rounds in a row
            // would cancel out and hide a real change instead.
            ++scan.skipped;
            return;
          }

          scan.files.emplace(entry.path().string(), mtime);
        });

    return scan;
  }

} // namespace logutils
