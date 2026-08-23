#ifndef UTIL_TEMPFILE_HPP_
#define UTIL_TEMPFILE_HPP_

#include <filesystem>
#include <string>

namespace logutils::temp_file {

  /**
   * @brief Creates a new, empty file in the system temp directory and
   * returns its path.
   *
   * The file is created, not just named: the caller gets a path that already
   * exists and is already owned by it.
   *
   * That distinction is the whole point. The obvious version -- build a name
   * from a clock reading and hand it to {@code std::ofstream} -- has three
   * problems in a directory every user on the machine can write to:
   * <ul>
   *   <li>{@code std::ofstream} creates with {@code 0666 & ~umask}, so the
   *   merged log (which is application data, potentially sensitive) ends up
   *   world-readable.</li>
   *   <li>{@code std::ofstream} follows symlinks, so anyone able to
   *   pre-create the path as a link to a file the user can write gets that
   *   file overwritten.</li>
   *   <li>a {@code steady_clock} reading is not unique across processes --
   *   its epoch is boot-relative and therefore shared -- so two runs started
   *   close enough together collide, and the first to quit deletes the
   *   other's file.</li>
   * </ul>
   *
   * So on POSIX this goes through {@code mkstemp()}, which creates the file
   * atomically ({@code O_EXCL}, so an existing name or symlink is a failure
   * rather than something to follow) with mode {@code 0600}. On Windows the
   * temp directory is already per-user, and the same exclusive-create
   * guarantee comes from retrying random names with {@code fopen()}'s
   * {@code "x"} mode.
   *
   * @param prefix text prepended to the generated unique part, e.g. {@code "logu-"}.
   * @param suffix text appended to the generated unique part, e.g. {@code ".log"}
   * (together producing e.g. {@code "/tmp/logu-A1b2C3.log"}).
   * @return the path to the newly created, exclusively-owned file.
   * @throws std::runtime_error if no file could be created.
   */
  std::filesystem::path createPrivate(const std::string &prefix, const std::string &suffix);

} // namespace logutils::temp_file

#endif /* UTIL_TEMPFILE_HPP_ */
