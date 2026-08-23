#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "cli/AppOptions.hpp"
#include "cli/AppOptionsParser.hpp"
#include "command/CommandRegistry.hpp"
#include "controller/ViewerController.hpp"
#include "model/LogDocument.hpp"
#include "model/LogEntry.hpp"
#include "service/LogEntryDeduplicator.hpp"
#include "service/LogFileParser.hpp"
#include "service/LogMerger.hpp"
#include "service/LogWriter.hpp"
#include "service/TimestampFormat.hpp"
#include "service/WorkingDirWatcher.hpp"
#include "util/DirectoryScan.hpp"
#include "util/TempFile.hpp"
#include "view/TerminalView.hpp"

namespace {

  using namespace logutils;

  // Where a diagnostic about the files being read goes. Worth routing rather
  // than printing on the spot: the same functions run once before the viewer
  // starts (stderr is the right place then) and again for every --working-dir
  // reload, while the viewer owns the terminal -- where a stray line lands in
  // the middle of the rendered frame and corrupts it until the next full
  // redraw.
  using MessageSink = std::function<void(std::string)>;

  // Every regular file inside `dir`, including subdirectories at any depth,
  // sorted by path for a deterministic merge order. Logs found here don't
  // necessarily have a ".log" extension, so all regular files are treated as
  // candidates; the output file itself (if it already exists from a previous
  // run) is skipped so re-running --working-dir doesn't fold the merged file
  // into itself.
  std::vector<std::string> collectWorkingDirLogFiles(
      const std::string &dir, const std::string &outputPath, const MessageSink &warn) {
    // The same exclusion WorkingDirWatcher applies to the same tree -- one
    // implementation, so the merge and the watcher can't disagree about which
    // file is the output.
    const dir_scan::FileExclusion output(outputPath);
    std::vector<std::string> files;

    const std::size_t skippedDirs = dir_scan::forEachRegularFile(
        dir, [&](const std::filesystem::directory_entry &entry) {
          if (output.matches(entry)) {
            return;
          }
          files.push_back(entry.path().string());
        });

    // A partial scan still produces a merged log, just an incomplete one --
    // and it would overwrite a previous, complete run's output. Say so rather
    // than letting that pass as a normal result.
    if (skippedDirs > 0) {
      warn(std::to_string(skippedDirs) +
           (skippedDirs == 1 ? " directory could" : " directories could") + " not be read and " +
           (skippedDirs == 1 ? "was" : "were") +
           " skipped; the merge covers only what could be scanned");
    }

    std::sort(files.begin(), files.end());

    return files;
  }

  // Parses one file and, if `verbose`, reports the timestamp format that was
  // detected for it (or the lack of one) to stderr.
  std::vector<LogEntry> parseAndReport(const ILogParser &parser, const std::string &path, bool verbose) {
    ParseResult result = parser.parse(path);

    if (verbose) {
      std::cerr << path << ": "
          << (result.detectedFormat ? "detected format \"" + *result.detectedFormat + "\""
              : "no timestamp format recognized")
                << '\n';
    }

    return std::move(result.entries);
  }

  // Parses every file in `paths` and concatenates their entries, in order.
  // A file that fails to parse (locked, deleted mid-scan, permission denied,
  // ...) is skipped with a warning rather than aborting the whole merge --
  // --working-dir sweeps every regular file regardless of extension, so
  // hitting an unreadable one is expected, not exceptional.
  std::vector<LogEntry> parseAndMerge(const ILogParser &parser, const std::vector<std::string> &paths,
                                      bool verbose, const MessageSink &warn) {
    std::vector<LogEntry> merged;

    for (const auto &path : paths) {
      std::vector<LogEntry> entries;

      try {
        entries = parseAndReport(parser, path, verbose);
      } catch (const std::exception &e) {
        warn("skipping " + path + ": " + e.what());
        continue;
      }

      merged.insert(merged.end(), std::make_move_iterator(entries.begin()),
                    std::make_move_iterator(entries.end()));
    }

    return merged;
  }

} // namespace

// Composition root: parses CLI options, wires the parsing/merge/write
// services together to produce the merged entries, then either writes them
// out directly or hands them to the MVC viewer (Model: LogDocument,
// View: TerminalView, Controller: ViewerController).
int main(int argc, char **argv) {
  using namespace logutils;

  AppOptions options;

  if (auto exitCode = AppOptionsParser::parse(argc, argv, options)) {
    return *exitCode;
  }

  // --working-dir saves its result in the directory it aggregates rather
  // than to stdout, so it gets its own default output path -- but only for a
  // non-interactive (e.g. cron/batch) run, where that file is the entire
  // point since nothing else persists the aggregation. An interactive run
  // instead falls back to a temp file further down, like a plain merge does.
  if (options.workingDirMode() && options.outputPath.empty() && !options.interactive) {
    options.outputPath = (std::filesystem::path(options.workingDir) / "merged.log").string();
  }

  try {
    const TimestampFormatCatalog formats = TimestampFormatCatalog::loadOrDefault(options.formatsPath, options.verbose);
    const MmapLogParser parser(formats);
    std::vector<LogEntry> merged;

    // Nothing owns the terminal yet, so the initial pass reports straight to
    // stderr.
    const MessageSink toStderr = [](std::string message) {
      std::cerr << "warning: " << message << '\n';
    };

    if (options.viewMode()) {
      merged = parseAndReport(parser, options.viewFile, options.verbose);
    } else if (options.workingDirMode()) {
      merged = parseAndMerge(
          parser, collectWorkingDirLogFiles(options.workingDir, options.outputPath, toStderr),
          options.verbose, toStderr);
    } else {
      merged = parseAndMerge(parser, options.files, options.verbose, toStderr);
    }

    LogMerger::sortChronologically(merged);

    if (options.dedup) {
      const LogEntryDeduplicator deduplicator;
      merged = deduplicator.dedupe(std::move(merged));
    }

    // A plain merge or --working-dir aggregation (not --view, which already
    // has its own file) that's about to be browsed interactively still needs
    // somewhere to write the merged result if -o wasn't given. Use a temp
    // file for that rather than leaving nothing on disk (or, for
    // --working-dir, defaulting to DIR/merged.log -- see above), and remove
    // it again once the viewer quits -- see tempFileGuard below.
    std::optional<std::filesystem::path> tempOutputPath;
    if (options.interactive && options.outputPath.empty() && !options.viewMode()) {
      // Created here, not merely named: the temp directory is world-writable,
      // so the file has to come into existence exclusively and owner-only
      // rather than be conjured by the std::ofstream below. See
      // temp_file::createPrivate.
      tempOutputPath = temp_file::createPrivate("logu-", ".log");
      options.outputPath = tempOutputPath->string();
    }

    struct TempFileGuard {
      const std::optional<std::filesystem::path> &path;
      ~TempFileGuard() {
        if (path) {
          std::error_code removeError;
          std::filesystem::remove(*path, removeError);
        }
      }
    } tempFileGuard{tempOutputPath};

    const PlainTextLogWriter writer;
    if (!options.outputPath.empty()) {
      std::ofstream out(options.outputPath);

      if (!out) {
        std::cerr << "could not write to: " << options.outputPath << '\n';
        return 1;
      }

      writer.write(out, merged);
    }

    if (options.interactive) {
      LogDocument document(std::move(merged));
      CommandRegistry commands;
      ViewerController controller(document, commands, options.theme);
      TerminalView view(controller, commands);

      // --working-dir gets a background watcher: if a file is added,
      // removed, or modified while the viewer is open, prompt before
      // re-merging and reloading rather than silently changing what's on
      // screen underneath the user.
      std::unique_ptr<WorkingDirWatcher> watcher;

      if (options.workingDirMode()) {
        watcher = std::make_unique<WorkingDirWatcher>(options.workingDir, options.outputPath);

        // Runs on a worker thread (see ViewerController::performReload), so it
        // touches nothing the UI thread owns: it only reads the options and
        // the parser, and hands back what it produced. Diagnostics are
        // collected instead of printed -- see MessageSink above -- and the
        // per-file format report --verbose asks for is dropped, being a
        // startup diagnostic that would flood the one-line status bar.
        controller.setReloadHandler([&]() {
          ReloadResult result;
          const MessageSink collect = [&result](std::string message) {
            result.warnings.push_back(std::move(message));
          };

          result.entries = parseAndMerge(
              parser, collectWorkingDirLogFiles(options.workingDir, options.outputPath, collect),
              /*verbose=*/false, collect);

          LogMerger::sortChronologically(result.entries);

          if (options.dedup) {
            const LogEntryDeduplicator deduplicator;
            result.entries = deduplicator.dedupe(std::move(result.entries));
          }

          if (!options.outputPath.empty()) {
            std::ofstream out(options.outputPath);

            if (out) {
              writer.write(out, result.entries);
            } else {
              collect("could not write to " + options.outputPath);
            }
          }

          return result;
        });

        controller.setReloadResolvedCallback([&watcher] { watcher->resume(); });

        view.watchWorkingDir(*watcher);
      }

      view.run();
    } else if (options.outputPath.empty()) {
      writer.write(std::cout, merged);
    }
  } catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << '\n';
    return 1;
  }

  return 0;
}
