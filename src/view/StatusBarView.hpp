#ifndef VIEW_STATUSBARVIEW_HPP_
#define VIEW_STATUSBARVIEW_HPP_

#include <string>

#include "../model/LogDocument.hpp"

namespace logutils::StatusBarView {

  /**
   * @brief Builds the footer line.
   *
   * Includes visible/total line counts, current position and percentage, the
   * active filter and find term (if any), and a hint pointing at {@code /help}.
   *
   * @param document the document to summarize.
   * @return the rendered footer text.
   */
  std::string footer(const LogDocument &document);

} // namespace logutils::StatusBarView

#endif /* VIEW_STATUSBARVIEW_HPP_ */
