#include "Theme.hpp"

#include <vector>

namespace logutils::theme {

  namespace {

    using ftxui::Color;

    Theme darkTheme() {
      return Theme{
        "dark",
        Color::RGB(18, 18, 18),
        Color::RGB(230, 230, 230),
        Color::RGB(100, 100, 100),
        Color::RGB(150, 150, 150),
        Color::RGB(150, 150, 150),
        Color::RGB(18, 18, 18),
        Color::RGB(100, 100, 100),
        Color::RGB(220, 70, 70),
      };
    }

    Theme catppuccinTheme() {
      return Theme{
        "catppuccin",
        Color::RGB(30, 30, 46),
        Color::RGB(205, 214, 244),
        Color::RGB(108, 112, 134),
        Color::RGB(180, 190, 254),
        Color::RGB(180, 190, 254),
        Color::RGB(30, 30, 46),
        Color::RGB(108, 112, 134),
        Color::RGB(243, 139, 168),
      };
    }

    Theme catppuccinFrappeTheme() {
      return Theme{
        "catppuccin-frappe",
        Color::RGB(48, 52, 70),
        Color::RGB(198, 208, 245),
        Color::RGB(181, 191, 226),
        Color::RGB(141, 164, 226),
        Color::RGB(141, 164, 226),
        Color::RGB(48, 52, 70),
        Color::RGB(181, 191, 226),
        Color::RGB(231, 130, 132),
      };
    }

    Theme catppuccinMacchiatoTheme() {
      return Theme{
        "catppuccin-macchiato",
        Color::RGB(36, 39, 58),
        Color::RGB(202, 211, 245),
        Color::RGB(184, 192, 224),
        Color::RGB(138, 173, 244),
        Color::RGB(138, 173, 244),
        Color::RGB(36, 39, 58),
        Color::RGB(184, 192, 224),
        Color::RGB(237, 135, 150),
      };
    }

    Theme cursorTheme() {
      return Theme{
        "cursor",
        Color::RGB(24, 24, 24),
        Color::RGB(228, 228, 228),
        Color::RGB(99, 99, 99),
        Color::RGB(136, 192, 208),
        Color::RGB(136, 192, 208),
        Color::RGB(24, 24, 24),
        Color::RGB(99, 99, 99),
        Color::RGB(227, 70, 113),
      };
    }

    Theme draculaTheme() {
      return Theme{
        "dracula",
        Color::RGB(29, 30, 40),
        Color::RGB(248, 248, 242),
        Color::RGB(98, 114, 164),
        Color::RGB(189, 147, 249),
        Color::RGB(189, 147, 249),
        Color::RGB(29, 30, 40),
        Color::RGB(98, 114, 164),
        Color::RGB(255, 85, 85),
      };
    }

    Theme flexokiTheme() {
      return Theme{
        "flexoki",
        Color::RGB(16, 15, 15),
        Color::RGB(206, 205, 195),
        Color::RGB(111, 110, 105),
        Color::RGB(218, 112, 44),
        Color::RGB(218, 112, 44),
        Color::RGB(16, 15, 15),
        Color::RGB(111, 110, 105),
        Color::RGB(209, 77, 65),
      };
    }

    Theme monokaiTheme() {
      return Theme{
        "monokai",
        Color::RGB(39, 40, 34),
        Color::RGB(248, 248, 242),
        Color::RGB(117, 113, 94),
        Color::RGB(174, 129, 255),
        Color::RGB(174, 129, 255),
        Color::RGB(39, 40, 34),
        Color::RGB(117, 113, 94),
        Color::RGB(249, 38, 114),
      };
    }

    Theme nordDarkTheme() {
      return Theme{
        "nord-dark",
        Color::RGB(46, 52, 64),
        Color::RGB(229, 233, 240),
        Color::RGB(97, 110, 136),
        Color::RGB(136, 192, 208),
        Color::RGB(136, 192, 208),
        Color::RGB(46, 52, 64),
        Color::RGB(97, 110, 136),
        Color::RGB(191, 97, 106),
      };
    }

    Theme nordLightTheme() {
      return Theme{
        "nord-light",
        Color::RGB(236, 239, 244),
        Color::RGB(46, 52, 64),
        Color::RGB(107, 114, 130),
        Color::RGB(94, 129, 172),
        Color::RGB(94, 129, 172),
        Color::RGB(236, 239, 244),
        Color::RGB(107, 114, 130),
        Color::RGB(191, 97, 106),
      };
    }

    Theme oneDoubleTheme() {
      return Theme{
        "one-double",
        Color::RGB(41, 44, 51),
        Color::RGB(220, 223, 228),
        Color::RGB(141, 144, 150),
        Color::RGB(97, 175, 239),
        Color::RGB(97, 175, 239),
        Color::RGB(41, 44, 51),
        Color::RGB(141, 144, 150),
        Color::RGB(224, 108, 117),
      };
    }

    Theme opencodeTheme() {
      return Theme{
        "openCode",
        Color::RGB(10, 10, 10),
        Color::RGB(238, 238, 238),
        Color::RGB(128, 128, 128),
        Color::RGB(250, 178, 131),
        Color::RGB(250, 178, 131),
        Color::RGB(10, 10, 10),
        Color::RGB(128, 128, 128),
        Color::RGB(224, 108, 117),
      };
    }

    Theme orngTheme() {
      return Theme{
        "orng",
        Color::RGB(10, 10, 10),
        Color::RGB(238, 238, 238),
        Color::RGB(128, 128, 128),
        Color::RGB(236, 91, 43),
        Color::RGB(236, 91, 43),
        Color::RGB(10, 10, 10),
        Color::RGB(128, 128, 128),
        Color::RGB(224, 108, 117),
      };
    }

    Theme osakaJadeTheme() {
      return Theme{
        "osaka-jade",
        Color::RGB(17, 28, 24),
        Color::RGB(193, 196, 151),
        Color::RGB(83, 104, 91),
        Color::RGB(45, 213, 183),
        Color::RGB(45, 213, 183),
        Color::RGB(17, 28, 24),
        Color::RGB(83, 104, 91),
        Color::RGB(255, 83, 69),
      };
    }

    Theme palenightTheme() {
      return Theme{
        "palenight",
        Color::RGB(41, 45, 62),
        Color::RGB(166, 172, 205),
        Color::RGB(103, 110, 149),
        Color::RGB(130, 170, 255),
        Color::RGB(130, 170, 255),
        Color::RGB(41, 45, 62),
        Color::RGB(103, 110, 149),
        Color::RGB(240, 113, 120),
      };
    }

    Theme tokyonightTheme() {
      return Theme{
        "tokyonight",
        Color::RGB(26, 27, 38),
        Color::RGB(192, 202, 245),
        Color::RGB(86, 95, 137),
        Color::RGB(122, 162, 247),
        Color::RGB(122, 162, 247),
        Color::RGB(26, 27, 38),
        Color::RGB(86, 95, 137),
        Color::RGB(247, 118, 142),
      };
    }

    Theme vercelTheme() {
      return Theme{
        "vercel",
        Color::RGB(0, 0, 0),
        Color::RGB(237, 237, 237),
        Color::RGB(135, 135, 135),
        Color::RGB(0, 112, 243),
        Color::RGB(0, 112, 243),
        Color::RGB(0, 0, 0),
        Color::RGB(135, 135, 135),
        Color::RGB(229, 72, 77),
      };
    }

  } // namespace

  const std::vector<Theme> &all() {
    // Built once (function-local static, thread-safe init) instead of
    // constructing the matching Theme -- string field included -- fresh on
    // every call; the render loop calls this every frame.
    static const std::vector<Theme> kThemes = {
      darkTheme(),
      catppuccinTheme(),
      catppuccinFrappeTheme(),
      catppuccinMacchiatoTheme(),
      cursorTheme(),
      draculaTheme(),
      flexokiTheme(),
      monokaiTheme(),
      nordDarkTheme(),
      nordLightTheme(),
      oneDoubleTheme(),
      opencodeTheme(),
      orngTheme(),
      osakaJadeTheme(),
      palenightTheme(),
      tokyonightTheme(),
      vercelTheme(),
    };
    return kThemes;
  }

  const Theme &byName(const std::string &name) {
    for (const Theme &t : all()) {
      if (t.name == name) {
        return t;
      }
    }

    return all().front(); // "dark"
  }

} // namespace logutils::theme
