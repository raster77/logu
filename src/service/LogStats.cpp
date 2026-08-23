#include "LogStats.hpp"

#include <algorithm>
#include <cctype>
#include <regex>

namespace logutils::LogStats {

  namespace {

    const std::regex &levelPattern() {
      static const std::regex pattern(R"(\b(TRACE|DEBUG|INFO|WARNING|WARN|ERROR|FATAL)\b)", std::regex::icase);
      return pattern;
    }

    void bump(std::vector<LevelCount> &counts, const std::string &level) {
      for (auto &c : counts) {
        if (c.level == level) {
          ++c.count;
          return;
        }
      }

      counts.push_back({level, 1});
    }

  } // namespace

  std::vector<LevelCount> countByLevel(const std::vector<std::string> &headers) {
    std::vector<LevelCount> counts;
    std::smatch match;

    for (const auto &header : headers) {
      if (std::regex_search(header, match, levelPattern())) {
        std::string level = match[1].str();
        std::transform(level.begin(), level.end(), level.begin(),
            [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

            if (level == "WARNING") {
          level = "WARN";
        }

        bump(counts, level);
      } else {
        bump(counts, "(none)");
      }
    }

    // stable_sort, not sort: levels tied on count would otherwise be free to
    // swap rows between two /stats invocations over the same data. Stability
    // pins them to bump()'s insertion order, i.e. first seen in the document.
    std::stable_sort(counts.begin(), counts.end(),
        [](const LevelCount &a, const LevelCount &b) { return a.count > b.count; });
    return counts;
  }

} // namespace logutils::LogStats
