#ifndef UTIL_DIRECTORYSCAN_HPP_
#define UTIL_DIRECTORYSCAN_HPP_

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>

namespace logutils::dir_scan {

  /**
   * @brief Calls {@code onFile} for every regular file under {@code root}, at any depth.
   *
   * Directory symlinks are reported but not descended into, matching
   * {@code std::filesystem::recursive_directory_iterator}'s default.
   *
   * The difference is what an error costs. That iterator abandons the whole
   * traversal as soon as {@code increment()} fails, so a single unreadable or
   * just-deleted subdirectory silently drops every file after it -- and
   * {@code --working-dir} is pointed at directories of live logs, where a
   * directory disappearing mid-scan is expected rather than exceptional.
   * This keeps its own stack of plain {@code directory_iterator}s so an
   * error is confined to the directory it happened in, and the rest of the
   * tree is still walked.
   *
   * @param root the directory to walk.
   * @param onFile called once per regular file encountered.
   * @return the number of directories skipped, whether because they couldn't
   * be opened (permissions, deleted mid-scan) or because listing them failed
   * partway through, so a caller can tell a complete scan from a partial one
   * instead of quietly acting on whatever it got. Callers that can't report
   * it (a background poll, say) may ignore the count.
   */
  std::size_t forEachRegularFile(
      const std::filesystem::path &root,
      const std::function<void(const std::filesystem::directory_entry &)> &onFile);

  /**
   * @brief Recognizes one specific file encountered during a directory walk.
   *
   * In practice, the merged output file, which both the
   * {@code --working-dir} merge and the watcher that polls the same tree
   * have to skip. They must agree on what counts as "the output file": if
   * they don't, the watcher reports a change to a file the merge
   * deliberately ignores, and the viewer prompts to reload in a loop. Hence
   * one implementation, used by both, rather than the same few lines written
   * twice.
   *
   * Matching is by identity ({@code std::filesystem::equivalent}), not by
   * path string, so a symlink or a differently-spelled path to the same file
   * still counts. That costs two {@code stat()} calls, so it is guarded by a
   * filename pre-filter -- at most one file in the tree can be it, and this
   * runs for every file on every poll. The pre-filter is case-insensitive
   * because {@code equivalent()} itself treats {@code "MERGED.LOG"} and
   * {@code "merged.log"} as one file on Windows: a pre-filter has to be at
   * least as permissive as the check it guards.
   */
  class FileExclusion {
    public:
      /**
       * @brief Constructs an exclusion for the file at {@code path}.
       * @param path path to the file to exclude; empty (e.g. a run with no
       * output file) excludes nothing.
       */
      explicit FileExclusion(const std::string &path);

      /**
       * @brief Whether {@code entry} is the excluded file.
       * @param entry a directory entry encountered during a walk.
       * @return {@code true} if {@code entry} is the same file as this exclusion's path.
       */
      bool matches(const std::filesystem::directory_entry &entry) const;

    private:
      std::filesystem::path mPath;
      std::string mFilename;
  };

} // namespace logutils::dir_scan

#endif /* UTIL_DIRECTORYSCAN_HPP_ */
