#ifndef UTIL_CLIPBOARD_HPP_
#define UTIL_CLIPBOARD_HPP_

#include <string>

namespace logutils::clipboard {

  /**
   * @brief Copies text to the system clipboard through two independent paths,
   * so it works regardless of what the terminal or desktop session supports:
   * <ul>
   *   <li>An OSC 52 terminal escape sequence, understood by many modern
   *   terminals (and transparently wrapped in tmux's passthrough escape when
   *   running inside a tmux session, since tmux otherwise swallows OSC 52
   *   itself instead of forwarding it). Works over SSH with no local tool.</li>
   *   <li>The system clipboard directly (X11/Wayland via XWayland, Windows,
   *   macOS), via the clip library. Covers terminals that don't support
   *   OSC 52 at all (e.g. VTE-based ones like GNOME Terminal or Terminator).</li>
   * </ul>
   *
   * Either path can silently no-op if unsupported (e.g. no display attached);
   * there's no reliable way to detect that from here, so this doesn't report
   * success/failure. Terminals also cap how much an OSC 52 sequence may carry
   * (commonly ~74 kB of base64), so a very long entry -- a deep stack trace --
   * may be dropped by that path alone; the clip library covers it wherever a
   * desktop clipboard is reachable.
   *
   * @param text the text to copy.
   */
  void copyToClipboard(const std::string &text);

} // namespace logutils::clipboard

#endif /* UTIL_CLIPBOARD_HPP_ */
