#include "Clipboard.hpp"

#include <array>
#include <cstdlib>
#include <iostream>

#include <clip.h>

namespace logutils::clipboard {

  namespace {

    std::string base64Encode(const std::string &data) {
      static constexpr std::array<char, 64> table = {
          'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S',
          'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',
          'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4',
          '5', '6', '7', '8', '9', '+', '/'};

      std::string out;
      out.reserve(((data.size() + 2) / 3) * 4);
      std::size_t i = 0;

      for (; i + 3 <= data.size(); i += 3) {
        const auto b0 = static_cast<unsigned char>(data[i]);
        const auto b1 = static_cast<unsigned char>(data[i + 1]);
        const auto b2 = static_cast<unsigned char>(data[i + 2]);

        out += table[b0 >> 2];
        out += table[((b0 & 0x03) << 4) | (b1 >> 4)];
        out += table[((b1 & 0x0F) << 2) | (b2 >> 6)];
        out += table[b2 & 0x3F];
      }

      const std::size_t remaining = data.size() - i;

      if (remaining == 1) {
        const auto b0 = static_cast<unsigned char>(data[i]);

        out += table[b0 >> 2];
        out += table[(b0 & 0x03) << 4];
        out += "==";
      } else if (remaining == 2) {
        const auto b0 = static_cast<unsigned char>(data[i]);
        const auto b1 = static_cast<unsigned char>(data[i + 1]);

        out += table[b0 >> 2];
        out += table[((b0 & 0x03) << 4) | (b1 >> 4)];
        out += table[(b1 & 0x0F) << 2];
        out += '=';
      }

      return out;
    }

  } // namespace

  void copyToClipboard(const std::string &text) {
    std::string osc52 = "\x1b]52;c;" + base64Encode(text) + "\x07";

    if (std::getenv("TMUX") != nullptr) {
      // tmux intercepts OSC sequences from the running program instead of
      // forwarding them, so they have to be wrapped in tmux's own
      // passthrough escape, with every ESC byte inside the wrapped
      // sequence doubled (tmux's documented escaping rule for Ptmux).
      std::string wrapped = "\x1bPtmux;";

      for (char c : osc52) {
        wrapped += c;

        if (c == '\x1b') {
          wrapped += '\x1b';
        }
      }

      wrapped += "\x1b\\";
      osc52 = std::move(wrapped);
    }

    std::cout << osc52 << std::flush;

    // OSC 52 alone isn't enough: several common terminals (e.g. VTE-based
    // ones like GNOME Terminal or Terminator) don't support it at all, so
    // also set the desktop clipboard directly (X11/Wayland via XWayland,
    // Windows, macOS) via the clip library. Harmless to attempt even when
    // OSC 52 already worked, and silently a no-op if there's no clipboard to
    // reach (e.g. a bare SSH session with no display).
    clip::set_text(text);
  }

} // namespace logutils::clipboard
