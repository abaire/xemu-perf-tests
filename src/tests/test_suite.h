#ifndef XEMU_PERF_TESTS_TEST_SUITE_H
#define XEMU_PERF_TESTS_TEST_SUITE_H

#include <chrono>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "test_host.h"

/**
 * Base class for all test suites.
 */
class TestSuite {
 public:
  //! Runtime configuration for TestSuites.
  struct Config {};

 public:
  TestSuite() = delete;
  TestSuite(TestHost &host, std::string output_dir, std::string suite_name, const Config &config);
  virtual ~TestSuite() = default;

  [[nodiscard]] const std::string &Name() const { return suite_name_; };

  //! Called to initialize the test suite.
  virtual void Initialize();

  //! Called to tear down the test suite.
  virtual void Deinitialize() {}

  //! Called before running an individual test within this suite.
  virtual void SetupTest() {}

  //! Called after running an individual test within this suite.
  virtual void TearDownTest() {}

  void DisableTests(const std::set<std::string> &tests_to_skip);

  [[nodiscard]] std::vector<std::string> TestNames() const;
  [[nodiscard]] bool HasEnabledTests() const { return !tests_.empty(); };

  void Run(const std::string &test_name);

  void RunAll();

 protected:
  //! Runs the given body function a number of times and calculates profiling information.
  TestHost::ProfileResults Profile(const std::string &test_name, uint32_t num_iterations,
                                   const std::function<void(void)> &body) const;
  void SetDefaultTextureFormat() const;

 protected:
  TestHost &host_;
  std::string output_dir_;
  std::string suite_name_;

  // Map of `test_name` to `void test()`
  std::map<std::string, std::function<void(void)>> tests_{};
};

#endif  // XEMU_PERF_TESTS_TEST_SUITE_H
