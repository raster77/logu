#ifndef UTIL_PATHUTIL_HPP_
#define UTIL_PATHUTIL_HPP_

#include <filesystem>
#include <string>

namespace logutils::path_util {

  /**
   * @brief The current user's home directory.
   *
   * Tries {@code $HOME} first, then {@code %USERPROFILE%}, then
   * {@code %HOMEDRIVE%+%HOMEPATH%}, so the same lookup works on Linux and
   * Windows without a {@code #ifdef}.
   *
   * @return the home directory, or an empty path if none of those are set.
   */
  std::filesystem::path homeDir();

  /**
   * @brief Expands a leading {@code "~"} (alone, or followed by a separator)
   * to {@link #homeDir}.
   *
   * Everything else -- including a {@code "~"} anywhere but the front, and
   * {@code "~user"}, which would need a passwd lookup this doesn't do -- is
   * returned unchanged, as is any {@code "~"} path when the home directory
   * can't be resolved.
   *
   * The shell does this before a path ever reaches {@code argv}, so it only
   * matters where the viewer reads a path from its own command box
   * ({@code "/export ~/out.log"}), where nothing has expanded it and a
   * literal {@code "~"} directory almost certainly doesn't exist.
   *
   * @param path the path to expand.
   * @return the expanded path.
   */
  std::filesystem::path expandUser(const std::string &path);

} // namespace logutils::path_util

#endif /* UTIL_PATHUTIL_HPP_ */
