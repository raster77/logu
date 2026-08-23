#include "Testing.hpp"

#include <string>
#include <vector>

#include "command/CommandRegistry.hpp"
#include "command/IViewerActions.hpp"

using namespace logutils;

namespace {

  // Records what a dispatched command asked the viewer to do, as
  // "call(argument)", so a whole dispatch is one expectation.
  class RecordingActions : public IViewerActions {
    public:
      std::string calls;

      void applyFilter(std::string text) override { record("applyFilter", text); }
      void applyRegexpFilter(std::string pattern) override { record("applyRegexpFilter", pattern); }
      void clearFilter() override { record("clearFilter", ""); }
      void applyFind(std::string text) override { record("applyFind", text); }
      void clearFind() override { record("clearFind", ""); }
      void findNext() override { record("findNext", ""); }
      void findPrevious() override { record("findPrevious", ""); }
      void showHelp() override { record("showHelp", ""); }
      void showStats() override { record("showStats", ""); }
      void quit() override { record("quit", ""); }
      void setStatusMessage(std::string message) override { record("status", message); }
      void copyCurrentEntry() override { record("copyCurrentEntry", ""); }
      void setTheme(std::string name) override { record("setTheme", name); }
      void openThemePicker() override { record("openThemePicker", ""); }
      void exportVisibleLines(std::string path) override { record("exportVisibleLines", path); }

    private:
      void record(const std::string &call, const std::string &argument) {
        calls += call + "(" + argument + ")";
      }
  };

  std::string dispatch(const std::string &input) {
    const CommandRegistry registry;
    RecordingActions actions;
    registry.dispatch(input, actions);
    // Unknown-command messages carry the whole command list; collapse them so
    // expectations stay about the outcome, not the wording.
    if (actions.calls.rfind("status(unknown command", 0) == 0) {
      return "unknown";
    }
    return actions.calls;
  }

  std::string suggestionsFor(const std::string &input) {
    const CommandRegistry registry;
    std::string out;
    for (const CommandDef *def : registry.filterCommands(input)) {
      out += (out.empty() ? "" : " ") + def->trigger;
    }
    return out;
  }

} // namespace

TEST(commandsDispatchToTheirAction) {
  CHECK_EQ(dispatch("/filter error"), "applyFilter(error)");
  CHECK_EQ(dispatch("/regexp ^2026.*ERROR"), "applyRegexpFilter(^2026.*ERROR)");
  CHECK_EQ(dispatch("/find timeout"), "applyFind(timeout)");
  CHECK_EQ(dispatch("/export /tmp/out.log"), "exportVisibleLines(/tmp/out.log)");
  CHECK_EQ(dispatch("/theme dracula"), "setTheme(dracula)");
  CHECK_EQ(dispatch("/theme"), "openThemePicker()");
  CHECK_EQ(dispatch("/clear"), "clearFilter()clearFind()");
  CHECK_EQ(dispatch("/help"), "showHelp()");
  CHECK_EQ(dispatch("/stats"), "showStats()");
  CHECK_EQ(dispatch("/copy"), "copyCurrentEntry()");
  CHECK_EQ(dispatch("/quit"), "quit()");
  CHECK_EQ(dispatch("/exit"), "quit()");
  // A command with an argument, submitted bare, gets an empty argument --
  // which is how "/filter" alone clears the filter.
  CHECK_EQ(dispatch("/filter"), "applyFilter()");
  CHECK_EQ(dispatch("/find"), "applyFind()");
}

TEST(commandNamesAreCaseInsensitive) {
  CHECK_EQ(dispatch("/FILTER error"), "applyFilter(error)");
  CHECK_EQ(dispatch("/Filter error"), "applyFilter(error)");
  CHECK_EQ(dispatch("/FiNd timeout"), "applyFind(timeout)");
  CHECK_EQ(dispatch("/HELP"), "showHelp()");
  CHECK_EQ(dispatch("/QuIt"), "quit()");
  // The argument keeps its own case.
  CHECK_EQ(dispatch("/FILTER MixedCase"), "applyFilter(MixedCase)");
}

TEST(argumentsAreTrimmedOnBothSides) {
  CHECK_EQ(dispatch("/export   /tmp/out.log   "), "exportVisibleLines(/tmp/out.log)");
  CHECK_EQ(dispatch("/theme  dracula "), "setTheme(dracula)");
  CHECK_EQ(dispatch("/find timeout  "), "applyFind(timeout)");
  // Nothing but spaces is the same as no argument at all.
  CHECK_EQ(dispatch("/theme    "), "openThemePicker()");
  CHECK_EQ(dispatch("/filter  "), "applyFilter()");
  // Spaces inside a quoted term are untouched.
  CHECK_EQ(dispatch("/filter \"connection refused\" "), "applyFilter(\"connection refused\")");
  // Argument-less commands tolerate a stray trailing space.
  CHECK_EQ(dispatch("/quit "), "quit()");
  CHECK_EQ(dispatch("/HELP  "), "showHelp()");
}

TEST(nearMissesAreNotCommands) {
  CHECK_EQ(dispatch("/filterx"), "unknown");
  CHECK_EQ(dispatch("/FILTERX"), "unknown");
  CHECK_EQ(dispatch("/helpme"), "unknown");
  // Text after a command that takes no argument is a different command, not
  // this one with something ignored.
  CHECK_EQ(dispatch("/help now"), "unknown");
  CHECK_EQ(dispatch("/quit now"), "unknown");
  CHECK_EQ(dispatch("/exit now"), "unknown");
  CHECK_EQ(dispatch("filter error"), "unknown");
  CHECK_EQ(dispatch("/"), "unknown");
}

TEST(clearTakesAnOptionalTarget) {
  // Blank or "all" clears both the filter and the find highlight.
  CHECK_EQ(dispatch("/clear"), "clearFilter()clearFind()");
  CHECK_EQ(dispatch("/clear all"), "clearFilter()clearFind()");
  CHECK_EQ(dispatch("/clear ALL"), "clearFilter()clearFind()");
  // A specific target clears just that one.
  CHECK_EQ(dispatch("/clear filter"), "clearFilter()");
  CHECK_EQ(dispatch("/clear find"), "clearFind()");
  CHECK_EQ(dispatch("/clear FIND"), "clearFind()");
  // Anything else is reported rather than silently ignored.
  CHECK_EQ(dispatch("/clear bogus"), "status(unknown clear target: \"bogus\" (use all, filter, or find))");
}

TEST(unknownCommandListsWhatExists) {
  const CommandRegistry registry;
  RecordingActions actions;
  registry.dispatch("/nope", actions);

  // The hint is built from the registry, so every registered command appears.
  for (const CommandDef *def : registry.definitions()) {
    CHECK(actions.calls.find(def->trigger) != std::string::npos);
  }
}

TEST(autocompleteFiltersOnThePrefix) {
  CHECK_EQ(suggestionsFor("/"),
      "/filter /regexp /find /clear /help /theme /copy /export /stats /quit /exit");
  CHECK_EQ(suggestionsFor("/f"), "/filter /find");
  CHECK_EQ(suggestionsFor("/fi"), "/filter /find");
  CHECK_EQ(suggestionsFor("/fil"), "/filter");
  CHECK_EQ(suggestionsFor("/F"), "/filter /find"); // case-insensitive, like dispatch
  CHECK_EQ(suggestionsFor("/QUIT"), "/quit");
  // Past the command name, there's nothing left to complete.
  CHECK_EQ(suggestionsFor("/filter "), "");
  CHECK_EQ(suggestionsFor("filter"), "");
  CHECK_EQ(suggestionsFor(""), "");
  CHECK_EQ(suggestionsFor("/zzz"), "");
}

// hasSuggestions() exists to answer "is the list open?" without building the
// list, so its only contract is that it never disagrees with the thing it
// stands in for.
TEST(hasSuggestionsAgreesWithFilterCommands) {
  const CommandRegistry registry;
  const std::string inputs[] = {
      "", "/", "/f", "/fi", "/filter", "/FILTER", "/filter foo", "/ ", "/zzz", "filter", "//"};

  for (const std::string &input : inputs) {
    CHECK_EQ(registry.hasSuggestions(input), !registry.filterCommands(input).empty());
  }
}

TEST(commandCountMatchesTheDefinitionList) {
  const CommandRegistry registry;
  CHECK_EQ(registry.commandCount(), registry.definitions().size());
}

TEST(effectiveSelectionFallsBackToTheObviousCandidate) {
  const CommandRegistry registry;

  const auto one = registry.filterCommands("/fil");
  // A single candidate is selected even with no explicit highlight.
  CHECK_EQ(registry.effectiveSelectedIndex(one, "/fil", -1), 0);

  const auto several = registry.filterCommands("/f");
  // Ambiguous and unhighlighted: nothing is selected, so Enter submits what
  // was typed instead of guessing.
  CHECK_EQ(registry.effectiveSelectedIndex(several, "/f", -1), -1);
  // A fully typed trigger selects itself, whatever its case.
  CHECK_EQ(registry.effectiveSelectedIndex(several, "/find", -1), 1);
  CHECK_EQ(registry.effectiveSelectedIndex(several, "/FIND", -1), 1);
  // An explicit highlight wins.
  CHECK_EQ(registry.effectiveSelectedIndex(several, "/f", 1), 1);
}
