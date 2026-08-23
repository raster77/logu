#include "ThemeCatalog.hpp"

#include "Theme.hpp"

namespace logutils::theme {

  const std::vector<std::string> &names() {
    // Derived from theme::all() rather than listed again here: the two used
    // to be parallel lists that had to be kept in sync by hand. Building it
    // once into a static keeps names() a cheap reference return for the
    // picker's per-frame render path, and this .cpp can depend on ftxui
    // freely -- it's ThemeCatalog.hpp that has to stay framework-free for the
    // CLI and controller layers.
    static const std::vector<std::string> kNames = [] {
      std::vector<std::string> names;
      names.reserve(all().size());

      for (const Theme &theme : all()) {
        names.push_back(theme.name);
      }

      return names;
    }();

    return kNames;
  }

} // namespace logutils::theme
