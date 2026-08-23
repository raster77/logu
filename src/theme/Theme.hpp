#ifndef THEME_THEME_HPP_
#define THEME_THEME_HPP_

#include <string>
#include <vector>

#include <ftxui/screen/color.hpp>

namespace logutils::theme {

  /**
   * @brief Colour palette for one visual theme.
   *
   * Sized to what the viewer actually renders: a bordered log list, a
   * command box with autocomplete, a footer, and a help/theme picker panel.
   * View-layer only (unlike {@link ThemeCatalog}, which is name-only and
   * framework-agnostic).
   */
  struct Theme {
      /** Theme name, as resolved by {@link #byName}. */
      std::string name;
      /** Base background color. */
      ftxui::Color background;
      /** Primary text color. */
      ftxui::Color text;
      /** Muted/secondary text color. */
      ftxui::Color dimText;
      /** Accent color, e.g. for the timestamp span and borders. */
      ftxui::Color accent;
      /** Background color for the selected row/item. */
      ftxui::Color selectedBg;
      /** Text color for the selected row/item. */
      ftxui::Color selectedText;
      /** Color for placeholder text, e.g. an empty command box. */
      ftxui::Color placeholderText;
      /** Color for error/warning text. */
      ftxui::Color errorText;
  };

  /**
   * @brief Every built-in theme, in display/picker order.
   *
   * The first ({@code "dark"}) is what {@link #byName} falls back to for an
   * unrecognized name, not the application default -- that is
   * {@code "nord-dark"} (see {@code AppOptions}), or whatever {@code /theme}
   * last saved. The single source of truth for what themes exist:
   * {@code ThemeCatalog::names()} is derived from it, so a theme can't end up
   * in the picker without a palette (it would silently render as
   * {@code "dark"}) or carry a palette no name reaches.
   *
   * @return the full theme table, in display order.
   */
  const std::vector<Theme> &all();

  /**
   * @brief Resolves a theme name to its palette.
   *
   * Falls back to {@code "dark"} for an unrecognized name.
   *
   * @param name the theme name (see {@code ThemeCatalog::names()}).
   * @return a reference into {@link #all}'s table (built once) rather than a
   * fresh {@code Theme}, since this is called from the render loop every frame.
   */
  const Theme &byName(const std::string &name);

} // namespace logutils::theme

#endif /* THEME_THEME_HPP_ */
