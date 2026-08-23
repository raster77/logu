#include "AppOptionsParser.hpp"

#include <algorithm>
#include <iostream>
#include <string>

#include <CLI11.hpp>

#include "../theme/ThemeCatalog.hpp"
#include "../util/TextUtil.hpp"
#include "../util/UserConfig.hpp"

namespace logutils {

  std::optional<int> AppOptionsParser::parse(int argc, char **argv, AppOptions &out) {
    CLI::App app{
      "Merges log files from the same application into one, sorted "
      "chronologically. Can also browse a single, already-merged log "
      "file with --view. \".gz\" files are decompressed transparently."};

    auto *filesOpt = app.add_option("files", out.files, "Two or more log files to merge")
                ->check(CLI::ExistingFile);

    auto *viewOpt = app.add_option("--view", out.viewFile,
                                   "Browse a single already-merged log file in the interactive "
                                   "viewer instead of merging (cannot be combined with the file "
                                   "list; pass -o or --no-interactive for plain output instead)")
                ->check(CLI::ExistingFile);

    viewOpt->excludes(filesOpt);

    auto *workingDirOpt = app.add_option("--working-dir", out.workingDir,
                                         "Aggregate every log file found inside DIR, including "
                                         "subdirectories, into one merged log file saved in DIR "
                                         "(files are matched regardless of extension; cannot be "
                                         "combined with the file list or --view)")
                ->check(CLI::ExistingDirectory);

    workingDirOpt->excludes(filesOpt);
    workingDirOpt->excludes(viewOpt);

    auto *outputOpt = app.add_option("-o,--output", out.outputPath,
                                     "Output file (default: stdout, or DIR/merged.log with "
                                     "--working-dir). Combine with --interactive to save while "
                                     "also browsing the result");

    app.add_option("--formats", out.formatsPath,
                   "JSON file of timestamp formats to recognize, as an array of "
                   "{\"name\": ..., \"pattern\": ...} objects (e.g. pattern "
                   "\"yyyy-MM-dd HH:mm:ss.SSS\"). Each file's format is detected "
                   "once, then reused for the rest of that file: each of its first "
                   "2000 lines votes for the first listed format that recognizes it, "
                   "and the most-voted format wins (ties going to whichever is listed "
                   "first). List more specific formats first -- a line votes only "
                   "once, so a less specific format listed earlier takes the vote. "
                   "Falls back to built-in defaults if the file is missing or invalid")
                ->capture_default_str();

    app.add_flag("--dedup", out.dedup,
                 "Remove entries whose content is identical (across every file "
                 "when merging, or within the file when using --view)");

    auto *interactiveOpt = app.add_flag("-i,--interactive,!--no-interactive", out.interactive,
                                        "Browse the result in an interactive terminal viewer. On by "
                                        "default with --view; use --no-interactive to force plain "
                                        "output. Add -o to also save the result");

    app.add_flag("-v,--verbose", out.verbose,
                 "Print the detected timestamp format for each file to stderr "
                 "(or a warning if none matched)");

    // Falls back to the theme last saved via "/theme" (see UserConfig), so a
    // choice made in the picker sticks across runs without needing --theme
    // every time. This has to happen before add_option below, since
    // capture_default_str() snapshots out.theme as the shown default at the
    // point it's called.
    if (const auto saved = user_config::loadTheme(user_config::configDir())) {
      const auto &names = theme::names();
      // Case-insensitive like --theme and "/theme" above, and the catalog's
      // spelling is what's kept: a config file hand-edited to "opencode"
      // still resolves, and still reaches byName() as "openCode".
      const auto match = std::find_if(names.begin(), names.end(),
          [&saved](const std::string &candidate) { return text_util::iequals(candidate, *saved); });
      if (match != names.end()) {
        out.theme = *match;
      }
    }

    app.add_option("--theme", out.theme,
                   "Colour theme for the interactive viewer (default: nord-dark, or the theme "
                   "last saved via \"/theme\"). Case-insensitive")
                // IsMember with ignore_case both accepts a differently-cased
                // name and rewrites it to the catalog's spelling, so
                // "--theme OPENCODE" reaches theme::byName() as "openCode".
                // Matching the viewer's own "/theme", which is case-insensitive
                // like every other step of its command dispatch.
                ->transform(CLI::IsMember(theme::names(), CLI::ignore_case))
                ->capture_default_str();

    try {
      app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
      return app.exit(e);
    }

    // --view is meant for browsing: default to the interactive viewer unless
    // the user asked for plain output (-o) or explicitly opted out.
    if (out.viewMode() && interactiveOpt->count() == 0 && outputOpt->count() == 0) {
      out.interactive = true;
    }

    if (!out.viewMode() && !out.workingDirMode() && out.files.size() < 2) {
      std::cerr << "error: provide two or more log files to merge, --view "
          "FILE to browse a single already-merged log, or "
          "--working-dir DIR to aggregate every log in a directory\n";
      return 1;
    }

    return std::nullopt;
  }

} // namespace logutils
