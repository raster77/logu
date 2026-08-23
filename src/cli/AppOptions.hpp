#ifndef CLI_APPOPTIONS_HPP_
#define CLI_APPOPTIONS_HPP_

#include <string>
#include <vector>

namespace logutils {

  /**
   * @brief Parsed and validated command-line options for the application.
   *
   * Populated by {@link AppOptionsParser::parse}. The three CLI modes (merge
   * a file list, {@link #viewMode}, {@link #workingDirMode}) are mutually
   * exclusive; the parser guarantees at most one of {@link #viewFile} and
   * {@link #workingDir} is set.
   */
  struct AppOptions {
      /** Log files to merge, in the order given on the command line. */
      std::vector<std::string> files;
      /** Path to an already-merged log to open directly in the viewer, or empty. */
      std::string viewFile;
      /** Directory to recursively scan and merge, or empty. */
      std::string workingDir;
      /** Destination path for the merged output, or empty to write to stdout. */
      std::string outputPath;
      /** Path to the timestamp format catalog JSON file. */
      std::string formatsPath = "formats.json";
      /** Whether to remove entries with identical content after merging. */
      bool dedup = false;
      /** Whether to open the interactive terminal viewer instead of writing plain text. */
      bool interactive = false;
      /** Whether to emit extra diagnostic output. */
      bool verbose = false;
      /** Name of the color theme to select on startup. */
      std::string theme = "nord-dark";

      /**
       * @brief Whether the app should open {@link #viewFile} directly in the viewer.
       * @return {@code true} if {@link #viewFile} is set.
       */
      bool viewMode() const {
        return !viewFile.empty();
      }

      /**
       * @brief Whether the app should recursively merge {@link #workingDir}.
       * @return {@code true} if {@link #workingDir} is set.
       */
      bool workingDirMode() const {
        return !workingDir.empty();
      }
  };

} // namespace logutils

#endif /* CLI_APPOPTIONS_HPP_ */
