#include "PathUtil.hpp"

#include <cstdlib>

namespace logutils::path_util {

  namespace {

    // Returns the value of `name`, or an empty string if it's unset/empty.
    std::string env(const char *name) {
      const char *value = std::getenv(name);
      return value ? std::string(value) : std::string();
    }

  } // namespace

  std::filesystem::path homeDir() {
    std::string home = env("HOME");

    if (home.empty()) {
      home = env("USERPROFILE");
    }

    if (home.empty()) {
      const std::string drive = env("HOMEDRIVE");
      const std::string path = env("HOMEPATH");

      if (!drive.empty() && !path.empty()) {
        home = drive + path;
      }
    }

    if (home.empty()) {
      return {};
    }

    return std::filesystem::path(home);
  }

  std::filesystem::path expandUser(const std::string &path) {
    if (path.empty() || path.front() != '~') {
      return std::filesystem::path(path);
    }
    // "~something" is a different user's home on a shell, which resolving
    // would take a passwd lookup this deliberately doesn't do -- leave it
    // alone rather than silently expanding it to the wrong home.
    if (path.size() > 1 && path[1] != '/' && path[1] != std::filesystem::path::preferred_separator) {
      return std::filesystem::path(path);
    }

    const std::filesystem::path home = homeDir();

    if (home.empty()) {
      return std::filesystem::path(path);
    }

    if (path.size() == 1) {
      return home;
    }
    // Skip the separator as well: appending an absolute-looking "/out.log"
    // to a path replaces it instead of joining, on both operator/ and
    // append().
    return home / path.substr(2);
  }

} // namespace logutils::path_util
