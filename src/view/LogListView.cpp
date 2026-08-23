#include "LogListView.hpp"

#include <algorithm>
#include <cstddef>
#include <string>

#include "../util/TextUtil.hpp"

namespace logutils::LogListView {

  namespace {

    // Mirrors the word-wrapping ftxui's paragraphAlignLeft() does internally
    // (split on spaces into a gapped flexbox), but lets the caller decide how
    // each word is rendered -- which is the whole reason this exists rather
    // than paragraphAlignLeft: that one takes a single plain string, with no
    // way to inject per-span colors while keeping the wrap.
    //
    // `buildWord(wordStart, wordEnd)` is called once per word, with the
    // half-open character range of that word within `line`, and returns the
    // element for it. A template rather than a std::function so the call
    // inlines: this runs for every visible row of every frame.
    template <typename BuildWord>
    ftxui::Element buildWrappedLine(const std::string &line, BuildWord buildWord) {
      using namespace ftxui;

      Elements words;
      std::size_t wordStart = 0;

      while (true) {
        std::size_t wordEnd = line.find(' ', wordStart);
        if (wordEnd == std::string::npos) {
          wordEnd = line.size();
        }

        words.push_back(buildWord(wordStart, wordEnd));

        if (wordEnd == line.size()) {
          break;
        }

        wordStart = wordEnd + 1;
      }

      static const auto config = FlexboxConfig().SetGap(1, 0);

      return flexbox(std::move(words), config);
    }

    // Collapses a one-element span list into that element, since wrapping a
    // lone text() in an hbox costs a node for nothing.
    ftxui::Element joinSpans(ftxui::Elements spans) {
      return spans.size() == 1 ? spans[0] : ftxui::hbox(std::move(spans));
    }

    // Highlights every case-insensitive occurrence of `needleLower` within a
    // word by inverting the row's own text/background colors for just that
    // span. Requires a non-empty needleLower (an empty needle would match at
    // every position).
    //
    // Known limitation: matching happens within a word, so a find term
    // containing a space ("connection refused") still moves the cursor to the
    // matching line -- LogDocument searches the whole line -- but nothing on
    // that line is highlighted. Spanning the gap would mean giving up the
    // word-flexbox that provides the wrapping.
    ftxui::Element buildHighlightedLine(
        const std::string &line, const std::string &needleLower, const theme::Theme &t) {
      using namespace ftxui;

      const std::string lineLower = text_util::toLower(line);

      return buildWrappedLine(line, [&](std::size_t wordStart, std::size_t wordEnd) {
        const std::string word = line.substr(wordStart, wordEnd - wordStart);
        const std::string wordLower = lineLower.substr(wordStart, wordEnd - wordStart);

        Elements spans;
        std::size_t pos = 0;
        std::size_t match = wordLower.find(needleLower);

        while (match != std::string::npos) {
          if (match > pos) {
            spans.push_back(text(word.substr(pos, match - pos)) | color(t.selectedText));
          }

          spans.push_back(text(word.substr(match, needleLower.size())) | color(t.selectedBg) |
                          bgcolor(t.selectedText));
          pos = match + needleLower.size();
          match = wordLower.find(needleLower, pos);
        }

        spans.push_back(text(word.substr(pos)) | color(t.selectedText));

        return joinSpans(std::move(spans));
      });
    }

    // Colors the leading `timestampLength` characters of `line` (the entry
    // header's detected timestamp -- see LogEntry::timestampLength) with
    // t.accent and the rest with t.text. Splits by character offset rather
    // than by word because a timestamp can itself contain a space (e.g.
    // "yyyy-MM-dd HH:mm:ss"), so the boundary doesn't always land on a word
    // edge.
    ftxui::Element buildTimestampColoredLine(
        const std::string &line, std::size_t timestampLength, const theme::Theme &t) {
      using namespace ftxui;

      return buildWrappedLine(line, [&](std::size_t wordStart, std::size_t wordEnd) {
        if (wordStart >= timestampLength) {
          return text(line.substr(wordStart, wordEnd - wordStart)) | color(t.text);
        }

        Elements spans;
        const std::size_t tsPartEnd = std::min(wordEnd, timestampLength);
        spans.push_back(text(line.substr(wordStart, tsPartEnd - wordStart)) | color(t.accent));

        if (tsPartEnd < wordEnd) {
          spans.push_back(text(line.substr(tsPartEnd, wordEnd - tsPartEnd)) | color(t.text));
        }

        return joinSpans(std::move(spans));
      });
    }

  } // namespace

  ftxui::Elements build(const LogDocument &document, int pageSize, const theme::Theme &t) {
    using namespace ftxui;

    const int total = static_cast<int>(document.visibleLineCount());
    const int scrollOffset = document.scrollOffset();
    const int windowStart = std::max(0, scrollOffset - pageSize);
    const int windowEnd = std::min(total, scrollOffset + 2 * pageSize);
    const std::string needleLower = text_util::toLower(document.findTerm());

    Elements itemElements;
    itemElements.reserve(static_cast<std::size_t>(std::max(0, windowEnd - windowStart)));

    for (int i = windowStart; i < windowEnd; ++i) {
      const std::string &line = document.visibleLineAt(static_cast<std::size_t>(i));

      if (i == scrollOffset) {
        // The focused row is always the current find match (setFindTerm/
        // findNext/findPrevious move the scroll position there), so this
        // is the only row that ever needs the term highlighted.
        Element element = (!needleLower.empty() && text_util::icontainsLower(line, needleLower))
                        ? buildHighlightedLine(line, needleLower, t)
                            : paragraphAlignLeft(line) | color(t.selectedText);
        itemElements.push_back(element | focus | bgcolor(t.selectedBg));
      } else {
        const std::size_t timestampLength =
            document.visibleLineTimestampLength(static_cast<std::size_t>(i));
        itemElements.push_back(timestampLength > 0
                ? buildTimestampColoredLine(line, timestampLength, t)
                : paragraphAlignLeft(line) | color(t.text));
      }
    }
    if (itemElements.empty()) {
      itemElements.push_back(text("(no matching lines)") | color(t.dimText));
    }

    return itemElements;
  }

} // namespace logutils::LogListView
