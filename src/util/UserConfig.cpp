#include "UserConfig.hpp"

#include <fstream>
#include <system_error>

#include "PathUtil.hpp"

namespace logutils::user_config {

  std::filesystem::path configDir() {
    const std::filesystem::path home = path_util::homeDir();

    if (home.empty()) {
      return {};
    }

    return home / ".logu";
  }

  std::optional<std::string> loadTheme(const std::filesystem::path &dir) {
    if (dir.empty()) {
      return std::nullopt;
    }

    std::ifstream in(dir / "theme");

    if (!in) {
      return std::nullopt;
    }

    std::string name;
    std::getline(in, name);

    if (name.empty()) {
      return std::nullopt;
    }

    return name;
  }

  void saveTheme(const std::filesystem::path &dir, const std::string &name) {
    if (dir.empty()) {
      return;
    }

    std::error_code error;
    std::filesystem::create_directories(dir, error);

    if (error) {
      return;
    }

    std::ofstream out(dir / "theme", std::ios::out | std::ios::trunc);

    if (!out) {
      return;
    }

    out << name;
  }

} // namespace logutils::user_config
