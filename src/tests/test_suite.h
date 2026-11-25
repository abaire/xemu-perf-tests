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
  //! In multiframe mode, this will be called each frame.
  virtual void SetupTest() {}

  //! Called after running an individual test within this suite.
  //! In multiframe mode, this will be called each frame.
  virtual void TearDownTest() {}

  void DisableTests(const std::set<std::string> &tests_to_skip);

  [[nodiscard]] std::vector<std::string> TestNames() const;
  [[nodiscard]] bool HasEnabledTests() const { return !tests_.empty(); };

  void Run(const std::string &test_name, uint32_t frame_count);

  void RunAll();

  //! Runs the given test repeatedly for `warmup_frames` + `measure_frames` frames, recording per-frame timing for the
  //! measured frames and logging a sustained-mode results record.
  void RunSustained(const std::string &test_name, uint32_t warmup_frames, uint32_t measure_frames);

  //! Overrides the work-scaling "load" knob for the given test (used to calibrate tests to the 60fps budget).
  void SetLoad(const std::string &test_name, uint32_t load) { loads_[test_name] = load; }

  //! Returns the configured load for the given test, or `default_value` if none has been configured.
  [[nodiscard]] uint32_t GetLoad(const std::string &test_name, uint32_t default_value) const;

  //! General purpose +1/-1 callback allowing user interaction via the left/right DPAD.
  virtual void UpdateUserContext(int direction) {};

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

  // Map of `test_name` to configured load knob value.
  std::map<std::string, uint32_t> loads_{};

  // Load value most recently returned by GetLoad for each test, used to annotate results.
  mutable std::map<std::string, uint32_t> effective_loads_{};
};

#endif  // XEMU_PERF_TESTS_TEST_SUITE_H
