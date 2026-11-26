#ifndef XEMU_PERF_TESTS_RUNTIME_CONFIG_H
#define XEMU_PERF_TESTS_RUNTIME_CONFIG_H

#include <memory>
#include <vector>

#include "configure.h"
#include "tests/test_suite.h"

class RuntimeConfig {
 public:
  enum class SkipConfiguration {
    DEFAULT,
    SKIPPED,
    UNSKIPPED,
  };

  enum class RunMode {
    //! Each test renders once (repeating its body a few times) and logs submission timing.
    SINGLE,
    //! Each test runs for warmup + measure frames in continuous (60fps-budget) mode and logs per-frame timing.
    SUSTAINED,
    //! Each test with a load knob is searched for the largest load that meets the frame budget, and the results are
    //! written to calibration.json.
    CALIBRATE,
  };

  //! Map of test case name to configured load.
  typedef std::map<std::string, uint32_t> TestLoads;

 public:
  RuntimeConfig() = default;
  explicit RuntimeConfig(const RuntimeConfig&) = delete;

  /**
   * Loads the JSON config file from the given path.
   * @param config_file_path - The path to the file containing the JSON config.
   * @param errors - Vector of strings into which any error messages will be placed.
   * @return true on success, false on failure
   */
  bool LoadConfig(const char* config_file_path, std::vector<std::string>& errors);

  /**
   * Loads the JSON config file from the given string buffer.
   * @param config_content - String containing the JSON config.
   * @param errors - Vector of strings into which any error messages will be placed.
   * @return true on success, false on failure
   */
  bool LoadConfigBuffer(const std::string& config_content, std::vector<std::string>& errors);

  /**
   * Loads the hardware-calibrated test loads from the "loads" object of a calibration.json produced by calibrate mode.
   * These are applied by `ApplyConfig` as each test's default, and are overridden by any "loads" in the config.
   * @param calibration_file_path - Path to the calibration JSON file.
   * @param errors - Vector of strings into which any error messages will be placed.
   * @return true on success, false on failure
   */
  bool LoadCalibrationBaseline(const char* calibration_file_path, std::vector<std::string>& errors);

  /**
   * Processes the JSON config file at the given path and adjusts the given set of test suites. Returns false if parsing
   * fails for any reason.
   */
  bool ApplyConfig(std::vector<std::shared_ptr<TestSuite>>& test_suites, std::vector<std::string>& errors);

  //! Returns the hardware-calibrated loads from the calibration baseline, keyed by "Suite::Test".
  [[nodiscard]] std::map<std::string, uint32_t> baseline_loads() const;

  [[nodiscard]] bool disable_autorun() const { return disable_autorun_; }
  [[nodiscard]] bool enable_autorun_immediately() const { return enable_autorun_immediately_; }
  [[nodiscard]] bool enable_shutdown_on_completion() const { return enable_shutdown_on_completion_; }
  [[nodiscard]] bool skip_tests_by_default() const { return skip_tests_by_default_; }
  [[nodiscard]] uint32_t reboot_or_shutdown_delay_ms() const { return reboot_or_shutdown_delay_ms_; }

  [[nodiscard]] const std::string& output_directory_path() const { return output_directory_path_; }

  [[nodiscard]] RunMode run_mode() const { return run_mode_; }
  [[nodiscard]] uint32_t warmup_frames() const { return warmup_frames_; }
  [[nodiscard]] uint32_t measure_frames() const { return measure_frames_; }
  [[nodiscard]] uint32_t calibration_probe_frames() const { return calibration_probe_frames_; }
  [[nodiscard]] uint32_t calibration_target_busy_p95_us() const { return calibration_target_busy_p95_us_; }

  static std::string SanitizePath(const std::string& path);

 private:
  bool disable_autorun_ = DEFAULT_DISABLE_AUTORUN;
  bool enable_autorun_immediately_ = DEFAULT_AUTORUN_IMMEDIATELY;
  bool enable_shutdown_on_completion_ = DEFAULT_ENABLE_SHUTDOWN;
  bool skip_tests_by_default_ = DEFAULT_SKIP_TESTS_BY_DEFAULT;
  uint32_t reboot_or_shutdown_delay_ms_ = 10000;

  std::string output_directory_path_ = SanitizePath(DEFAULT_OUTPUT_DIRECTORY_PATH);

  RunMode run_mode_ = RunMode::SINGLE;
  uint32_t warmup_frames_ = 30;
  uint32_t measure_frames_ = 240;
  uint32_t calibration_probe_frames_ = 120;
  // 90% of a 60Hz frame, leaving headroom for jitter.
  uint32_t calibration_target_busy_p95_us_ = 15000;

  //! Map of test suite name to a map of test case to configured load.
  std::map<std::string, TestLoads> configured_test_loads_;

  //! Map of test suite name to a map of test case to hardware-calibrated load.
  std::map<std::string, TestLoads> baseline_test_loads_;

  //! Map of test suite name to skip config.
  std::map<std::string, SkipConfiguration> configured_test_suites_;
  //! Map of test suite name to a map of test case to skip config.
  std::map<std::string, std::map<std::string, SkipConfiguration>> configured_test_cases_;
};

#endif  // XEMU_PERF_TESTS_RUNTIME_CONFIG_H
