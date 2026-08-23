#ifndef UTIL_USERCONFIG_HPP_
#define UTIL_USERCONFIG_HPP_

#include <filesystem>
#include <optional>
#include <string>

namespace logutils::user_config {

  /**
   * @brief The directory where per-user viewer settings (currently just the
   * last selected theme) are persisted.
   *
   * {@code "<home>/.logu"} on both Linux and Windows, with the home directory
   * resolved by {@code path_util::homeDir()}.
   *
   * @return the config directory, or an empty path when the home directory can't be resolved.
   */
  std::filesystem::path configDir();

  /**
   * @brief Reads the theme name saved in {@code "<dir>/theme"}.
   * @param dir the config directory, as from {@link #configDir}.
   * @return the saved theme name, or {@code std::nullopt} if the file
   * doesn't exist, is empty, or can't be read -- callers fall back to their
   * own default, same as a missing {@code formats.json} falls back to the
   * built-in formats.
   */
  std::optional<std::string> loadTheme(const std::filesystem::path &dir);

  /**
   * @brief Creates {@code dir} if needed and writes {@code name} to
   * {@code "<dir>/theme"}, overwriting any previously saved value.
   *
   * Failures (read-only home, missing permissions, ...) are swallowed rather
   * than surfaced: losing the saved theme preference isn't worth
   * interrupting the viewer over.
   *
   * @param dir the config directory, as from {@link #configDir}.
   * @param name the theme name to save.
   */
  void saveTheme(const std::filesystem::path &dir, const std::string &name);

} // namespace logutils::user_config

#endif /* UTIL_USERCONFIG_HPP_ */
