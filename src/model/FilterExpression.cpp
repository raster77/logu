#include "FilterExpression.hpp"

#include <cctype>

namespace logutils {

  namespace {

    // Splits text into tokens on whitespace, keeping double-quoted spans
    // (quotes stripped) as a single token so phrases can contain spaces.
    std::vector<std::string> tokenize(const std::string &text) {
      std::vector<std::string> tokens;
      std::string current;
      bool inQuotes = false;

      for (char c : text) {
        if (c == '"') {
          inQuotes = !inQuotes;
          continue;
        }

        if (!inQuotes && std::isspace(static_cast<unsigned char>(c))) {
          if (!current.empty()) {
            tokens.push_back(std::move(current));
            current.clear();
          }

          continue;
        }

        current += c;
      }

      if (!current.empty()) {
        tokens.push_back(std::move(current));
      }

      return tokens;
    }

  } // namespace

  std::vector<FilterAndGroup> parseFilterExpression(const std::string &text) {
    std::vector<FilterAndGroup> groups;
    FilterAndGroup current;
    bool pendingNegate = false;

    auto flushGroup = [&]() {
      if (!current.empty()) {
        groups.push_back(std::move(current));
        current.clear();
      }
    };

    for (const std::string &token : tokenize(text)) {
      if (token == "||") {
        flushGroup();
        pendingNegate = false;
        continue;
      }

      if (token == "&&") {
        // Adjacent terms are already ANDed by default; explicit "&&" is
        // just a separator.
        continue;
      }

      if (token == "!") {
        pendingNegate = !pendingNegate;
        continue;
      }

      std::string termText = token;
      bool negate = pendingNegate;
      pendingNegate = false;

      while (!termText.empty() && termText.front() == '!') {
        negate = !negate;
        termText.erase(termText.begin());
      }

      if (termText.empty()) {
        continue;
      }

      current.push_back(FilterTerm{std::move(termText), negate});
    }

    flushGroup();

    return groups;
  }

} // namespace logutils
