#include "Testing.hpp"

#include <exception>
#include <iostream>

namespace testing {

  namespace {
    int gFailuresInCurrentTest = 0;
  } // namespace

  std::vector<TestCase> &registry() {
    // Function-local static: the Registrar instances below run during static
    // initialization, before any namespace-scope vector of ours would be
    // guaranteed to exist.
    static std::vector<TestCase> tests;
    return tests;
  }

  Registrar::Registrar(std::string name, std::function<void()> body) {
    registry().push_back({std::move(name), std::move(body)});
  }

  void reportFailure(const char *file, int line, const std::string &detail) {
    ++gFailuresInCurrentTest;
    std::cout << "    " << file << ":" << line << ": " << detail << '\n';
  }

  int runAll() {
    int failedTests = 0;

    for (const TestCase &test : registry()) {
      gFailuresInCurrentTest = 0;
      std::cout << "[ RUN  ] " << test.name << '\n';

      try {
        test.body();
      } catch (const std::exception &e) {
        reportFailure("<test>", 0, std::string("unexpected exception: ") + e.what());
      } catch (...) {
        reportFailure("<test>", 0, "unexpected non-standard exception");
      }

      if (gFailuresInCurrentTest > 0) {
        ++failedTests;
        std::cout << "[ FAIL ] " << test.name << " (" << gFailuresInCurrentTest
                  << " failed check" << (gFailuresInCurrentTest == 1 ? "" : "s") << ")\n";
      } else {
        std::cout << "[  OK  ] " << test.name << '\n';
      }
    }

    std::cout << "\n" << registry().size() - static_cast<std::size_t>(failedTests) << "/"
              << registry().size() << " tests passed\n";
    return failedTests == 0 ? 0 : 1;
  }

} // namespace testing

int main() {
  return testing::runAll();
}
