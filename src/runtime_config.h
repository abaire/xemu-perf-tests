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
   * Processes the JSON config file at the given path and adjusts the given set of test suites. Returns false if parsing
   * fails for any reason.
   */
  bool ApplyConfig(std::vector<std::shared_ptr<TestSuite>>& test_suites, std::vector<std::string>& errors);

  [[nodiscard]] bool disable_autorun() const { return disable_autorun_; }
  [[nodiscard]] bool enable_autorun_immediately() const { return enable_autorun_immediately_; }
  [[nodiscard]] bool enable_shutdown_on_completion() const { return enable_shutdown_on_completion_; }
  [[nodiscard]] bool skip_tests_by_default() const { return skip_tests_by_default_; }

  [[nodiscard]] const std::string& output_directory_path() const { return output_directory_path_; }

  static std::string SanitizePath(const std::string& path);

 private:
  bool disable_autorun_ = DEFAULT_DISABLE_AUTORUN;
  bool enable_autorun_immediately_ = DEFAULT_AUTORUN_IMMEDIATELY;
  bool enable_shutdown_on_completion_ = DEFAULT_ENABLE_SHUTDOWN;
  bool skip_tests_by_default_ = DEFAULT_SKIP_TESTS_BY_DEFAULT;

  std::string output_directory_path_ = SanitizePath(DEFAULT_OUTPUT_DIRECTORY_PATH);

  //! Map of test suite name to skip config.
  std::map<std::string, SkipConfiguration> configured_test_suites_;
  //! Map of test suite name to a map of test case to skip config.
  std::map<std::string, std::map<std::string, SkipConfiguration>> configured_test_cases_;
};

#endif  // XEMU_PERF_TESTS_RUNTIME_CONFIG_H
