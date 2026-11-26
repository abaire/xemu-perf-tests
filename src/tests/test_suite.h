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
  //! measured frames. If `log` is true, also logs a sustained-mode results record.
  //! If `abort_busy_us` is not 0, the run stops early once any frame after the first is busy for longer than that.
  TestHost::SustainedSummary RunSustained(const std::string &test_name, uint32_t warmup_frames, uint32_t measure_frames,
                                          bool log = true, uint32_t abort_busy_us = 0);

  //! Parameters for Calibrate().
  struct CalibrationSettings {
    uint32_t warmup_frames;
    //! Number of measured frames for each probe during the search.
    uint32_t probe_frames;
    //! Number of measured frames used to confirm the final load.
    uint32_t confirm_frames;
    //! A load passes if its busy p95 is at most this, with no missed vblanks or fence timeouts.
    uint32_t target_busy_p95_us;
  };

  //! Result of Calibrate().
  struct CalibrationResult {
    //! "ok", "no_load" (the test has no load knob), "over_budget" (fails even at a load of 1), "hit_cap" (still
    //! passes at the largest load searched), or "unconfirmed" (the search found a passing load but the confirmation
    //! runs failed, even after stepping the load down), or "excluded" (the test opted out of calibration).
    std::string status;
    uint32_t load;
    //! Number of sustained runs performed.
    uint32_t runs;
    //! Summary of the final confirmation run (or the last run, if there was none).
    TestHost::SustainedSummary summary;

    //! One sustained run performed during calibration.
    struct Run {
      uint32_t load;
      //! True for a confirmation run, false for a search probe.
      bool confirm;
      bool passed;
      TestHost::SustainedSummary summary;
    };
    //! Every run performed, in order, including retries.
    std::vector<Run> history;
  };

  //! Searches for the largest load at which the given test meets the target, then confirms it with a logged run. On
  //! return the load is left set to the result.
  CalibrationResult Calibrate(const std::string &test_name, const CalibrationSettings &settings);

  //! Overrides the work-scaling "load" knob for the given test (used to calibrate tests to the 60fps budget).
  void SetLoad(const std::string &test_name, uint32_t load) { loads_[test_name] = load; }

  //! Sets the hardware-calibrated load for the given test. It is used whenever no override is set via `SetLoad`.
  void SetBaselineLoad(const std::string &test_name, uint32_t load) { baseline_loads_[test_name] = load; }

  //! Returns the load for the given test: the override set via `SetLoad` if any, otherwise the calibrated baseline if
  //! any, otherwise `default_value`.
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

  // Tests that have a load knob but must not be calibrated, e.g. diagnostics that compare runs at equal loads.
  std::set<std::string> uncalibrated_tests_{};

  // Map of `test_name` to hardware-calibrated load, used when there is no entry in `loads_`.
  std::map<std::string, uint32_t> baseline_loads_{};

  // Load value most recently returned by GetLoad for each test, used to annotate results.
  mutable std::map<std::string, uint32_t> effective_loads_{};
};

#endif  // XEMU_PERF_TESTS_TEST_SUITE_H
