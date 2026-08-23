#ifndef TESTS_TESTING_HPP_
#define TESTS_TESTING_HPP_

#include <functional>
#include <sstream>
#include <string>
#include <vector>

// A test runner in one header and one .cpp, rather than a dependency: the
// project vendors or fetches everything it uses, and pulling in a framework
// to assert on a handful of pure functions would cost more (build time, one
// more thing to update) than it saves. TEST() registers a function; CHECK()
// records a failure and keeps going, so one broken expectation doesn't hide
// the rest of the file.
namespace testing {

  struct TestCase {
      std::string name;
      std::function<void()> body;
  };

  std::vector<TestCase> &registry();

  struct Registrar {
      Registrar(std::string name, std::function<void()> body);
  };

  void reportFailure(const char *file, int line, const std::string &detail);

  // Runs every registered test; returns a process exit code.
  int runAll();

  template <typename A, typename B>
  void checkEqual(const A &actual, const B &expected, const char *actualText,
                  const char *expectedText, const char *file, int line) {
    if (actual == expected) {
      return;
    }
    std::ostringstream out;
    out << actualText << " == " << expectedText << "\n         actual: " << actual
        << "\n       expected: " << expected;
    reportFailure(file, line, out.str());
  }

} // namespace testing

#define TEST(name)                                                     \
  static void name();                                                  \
  static const ::testing::Registrar name##_registrar(#name, name);     \
  static void name()

#define CHECK(expr)                                                    \
  do {                                                                 \
    if (!(expr)) {                                                     \
      ::testing::reportFailure(__FILE__, __LINE__, #expr);             \
    }                                                                  \
  } while (false)

#define CHECK_EQ(actual, expected)                                     \
  ::testing::checkEqual((actual), (expected), #actual, #expected, __FILE__, __LINE__)

#endif /* TESTS_TESTING_HPP_ */
