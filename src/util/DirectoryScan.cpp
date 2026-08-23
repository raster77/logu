#include "DirectoryScan.hpp"

#include <system_error>
#include <utility>
#include <vector>

#include "TextUtil.hpp"

namespace logutils::dir_scan {

  namespace fs = std::filesystem;

  std::size_t forEachRegularFile(
      const fs::path &root, const std::function<void(const fs::directory_entry &)> &onFile) {
    std::size_t skippedDirs = 0;

    // Note the absence of directory_options::skip_permission_denied: that
    // option turns an unreadable directory into a silently empty one, which
    // is exactly the case skippedDirs exists to report. Without it the error
    // surfaces here, gets counted, and the walk carries on regardless -- the
    // scan is just as tolerant, and the caller can now tell that it was
    // partial.
    std::error_code rootError;
    fs::directory_iterator rootIt(root, rootError);

    if (rootError) {
      return 1;
    }

    const fs::directory_iterator end;
    std::vector<fs::directory_iterator> stack;

    stack.push_back(std::move(rootIt));

    while (!stack.empty()) {
      if (stack.back() == end) {
        stack.pop_back();
        continue;
      }

      const fs::directory_entry entry = *stack.back();

      // Advance before looking at `entry` (which is a copy, so it stays
      // valid): that way a failed increment ends only the directory it
      // happened in, and can't leave the same entry on top to be visited
      // forever.
      std::error_code incrementError;

      stack.back().increment(incrementError);

      if (incrementError) {
        ++skippedDirs;
        stack.pop_back();
      }

      std::error_code typeError;
      if (entry.is_directory(typeError) && !typeError) {
        // recursive_directory_iterator descends into directory symlinks only
        // under follow_directory_symlink; match that, which also keeps
        // symlink loops out of the stack.
        std::error_code linkError;

        if (entry.is_symlink(linkError) || linkError) {
          continue;
        }

        std::error_code childError;
        fs::directory_iterator child(entry.path(), childError);

        if (childError) {
          ++skippedDirs;
          continue;
        }

        stack.push_back(std::move(child));
        continue;
      }

      if (entry.is_regular_file(typeError) && !typeError) {
        onFile(entry);
      }
    }

    return skippedDirs;
  }

  FileExclusion::FileExclusion(const std::string &path)
      : mPath(path), mFilename(mPath.filename().string()) {
  }

  bool FileExclusion::matches(const fs::directory_entry &entry) const {
    if (mFilename.empty()) {
      return false;
    }

    if (!text_util::iequals(entry.path().filename().string(), mFilename)) {
      return false;
    }

    std::error_code equivalenceError;

    return fs::equivalent(entry.path(), mPath, equivalenceError) && !equivalenceError;
  }

} // namespace logutils::dir_scan
