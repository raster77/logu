#ifndef VIEW_LOGLISTVIEW_HPP_
#define VIEW_LOGLISTVIEW_HPP_

#include <ftxui/dom/elements.hpp>

#include "../model/LogDocument.hpp"
#include "../theme/Theme.hpp"

namespace logutils::LogListView {

  /**
   * @brief Builds a window of elements around the document's current scroll
   * position, not the whole (possibly huge) filtered list.
   *
   * {@code frame()} just needs enough surrounding content to scroll within,
   * and this keeps render cost independent of file size. One page of margin
   * on each side is more than enough slack for {@code frame}'s auto-scroll
   * to work with.
   *
   * @param document the document to render a window of.
   * @param pageSize the viewport height in rows.
   * @param t the active color theme.
   * @return the rendered log line elements.
   */
  ftxui::Elements build(const LogDocument &document, int pageSize, const theme::Theme &t);

} // namespace logutils::LogListView

#endif /* VIEW_LOGLISTVIEW_HPP_ */
