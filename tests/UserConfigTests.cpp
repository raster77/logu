#include "Testing.hpp"

#include <filesystem>
#include <system_error>
#include <unistd.h>

#include "util/UserConfig.hpp"

using namespace logutils;

namespace {

  namespace fs = std::filesystem;

  // A directory under the system temp dir, removed when the test ends.
  class TempDir {
    public:
      TempDir() : mPath(fs::temp_directory_path() / uniqueName()) {
      }

      ~TempDir() {
        std::error_code ignored;
        fs::remove_all(mPath, ignored);
      }

      TempDir(const TempDir &) = delete;
      TempDir &operator=(const TempDir &) = delete;

      const fs::path &path() const {
        return mPath;
      }

    private:
      static std::string uniqueName() {
        static int counter = 0;
        return "logu-tests-userconfig-" + std::to_string(::getpid()) + "-" + std::to_string(counter++);
      }

      fs::path mPath;
  };

} // namespace

TEST(loadThemeMissingDirReturnsNullopt) {
  const TempDir dir;
  // Deliberately not created: saveTheme() is what's expected to create it.
  CHECK(!user_config::loadTheme(dir.path()).has_value());
}

TEST(loadThemeEmptyPathReturnsNullopt) {
  CHECK(!user_config::loadTheme(fs::path()).has_value());
}

TEST(saveThenLoadRoundTrips) {
  const TempDir dir;
  user_config::saveTheme(dir.path(), "nord-dark");
  const auto loaded = user_config::loadTheme(dir.path());
  CHECK(loaded.has_value());
  CHECK_EQ(*loaded, "nord-dark");
}

TEST(saveThemeOverwritesPreviousValue) {
  const TempDir dir;
  user_config::saveTheme(dir.path(), "nord-dark");
  user_config::saveTheme(dir.path(), "solarized-light");
  const auto loaded = user_config::loadTheme(dir.path());
  CHECK(loaded.has_value());
  CHECK_EQ(*loaded, "solarized-light");
}

TEST(saveThemeCreatesMissingDirectory) {
  const TempDir dir;
  const fs::path nested = dir.path() / "nested" / ".logu";
  CHECK(!fs::exists(nested));
  user_config::saveTheme(nested, "dark");
  CHECK(fs::exists(nested / "theme"));
}

TEST(saveThemeWithEmptyDirIsNoop) {
  // Should not throw or crash -- an unresolvable home directory just means
  // the theme choice doesn't persist.
  user_config::saveTheme(fs::path(), "dark");
}
