#include "runtime_config.h"

#include <fstream>
#include <list>

#include "tiny-json.h"

#define MAX_CONFIG_FILE_SIZE (1024 * 1024)

static bool ParseTestSuites(
    json_t const* test_suites, std::vector<std::string>& errors,
    std::map<std::string, RuntimeConfig::SkipConfiguration>& skipped_test_suites,
    std::map<std::string, std::map<std::string, RuntimeConfig::SkipConfiguration>>& skipped_test_cases);

//! C++ wrapper around Tiny-JSON jsonPool_t
class JSONParser : jsonPool_t {
 public:
  JSONParser() : jsonPool_t{&Alloc, &Alloc} {}
  explicit JSONParser(const char* str) : jsonPool_t{&Alloc, &Alloc}, json_string_{str} {
    root_node_ = json_createWithPool(json_string_.data(), this);
  }

  JSONParser(const JSONParser&) = delete;
  JSONParser(JSONParser&&) = delete;
  JSONParser& operator=(const JSONParser&) = delete;
  JSONParser& operator=(JSONParser&&) = delete;

  [[nodiscard]] json_t const* root() const { return root_node_; }

 private:
  static json_t* Alloc(jsonPool_t* pool) {
    const auto list_pool = static_cast<JSONParser*>(pool);
    list_pool->object_list_.emplace_back();
    return &list_pool->object_list_.back();
  }

  std::list<json_t> object_list_{};
  std::string json_string_{};
  json_t const* root_node_{};
};

//! Reads the JSON file at `path` into `out`.
static bool ReadJSONFile(const char* path, std::string& out, std::vector<std::string>& errors) {
  std::string dos_style_path = path;
  std::replace(dos_style_path.begin(), dos_style_path.end(), '/', '\\');

  std::ifstream file(dos_style_path.c_str());
  if (!file) {
    errors.push_back(std::string("Missing file at ") + path);
    return false;
  }

  file.seekg(0, std::ios::end);
  auto end_pos = file.tellg();
  file.seekg(0, std::ios::beg);
  std::streamsize file_size = end_pos - file.tellg();

  if (file_size > MAX_CONFIG_FILE_SIZE || file_size == -1) {
    errors.push_back(std::string("File at ") + path + " is too large.");
    return false;
  }

  out.resize(file_size);
  file.read(&out[0], file_size);
  return true;
}

bool RuntimeConfig::LoadConfig(const char* config_file_path, std::vector<std::string>& errors) {
  std::string json_string;
  if (!ReadJSONFile(config_file_path, json_string, errors)) {
    return false;
  }
  return LoadConfigBuffer(json_string, errors);
}

static bool LoadBool(json_t const* object, const char* key, bool& out) {
  auto property = json_getProperty(object, key);
  if (!property) {
    return true;
  }

  if (json_getType(property) != JSON_BOOLEAN) {
    return false;
  }

  out = json_getBoolean(property);
  return true;
};

static bool LoadString(json_t const* object, const char* key, std::string& out) {
  auto property = json_getProperty(object, key);
  if (!property) {
    return true;
  }

  if (json_getType(property) != JSON_TEXT) {
    return false;
  }

  out = json_getValue(property);
  return true;
};

static bool LoadUint32(json_t const* object, const char* key, uint32_t& out) {
  auto property = json_getProperty(object, key);
  if (!property) {
    return true;
  }

  if (json_getType(property) != JSON_INTEGER) {
    return false;
  }

  auto value = json_getInteger(property);
  if (value < 0) {
    return false;
  }

  out = static_cast<uint32_t>(value & 0xFFFFFFFF);
  return true;
};

//! Parses an optional "loads" object, keyed by "Suite::Test", into `out`.
static bool ParseLoads(json_t const* loads, std::vector<std::string>& errors,
                       std::map<std::string, RuntimeConfig::TestLoads>& out) {
  if (!loads) {
    return true;
  }
  if (json_getType(loads) != JSON_OBJ) {
    errors.emplace_back("'loads' not an object");
    return false;
  }
  for (auto element = json_getChild(loads); element; element = json_getSibling(element)) {
    std::string fq_name = json_getName(element);
    auto separator = fq_name.find("::");
    if (separator == std::string::npos) {
      errors.emplace_back("loads[" + fq_name + "] must be named 'Suite::Test'. Ignoring");
      continue;
    }
    if (json_getType(element) != JSON_INTEGER || json_getInteger(element) < 0) {
      errors.emplace_back("loads[" + fq_name + "] must be a non-negative integer");
      return false;
    }
    out[fq_name.substr(0, separator)][fq_name.substr(separator + 2)] =
        static_cast<uint32_t>(json_getInteger(element) & 0xFFFFFFFF);
  }
  return true;
}

bool RuntimeConfig::LoadCalibrationBaseline(const char* calibration_file_path, std::vector<std::string>& errors) {
  std::string json_string;
  if (!ReadJSONFile(calibration_file_path, json_string, errors)) {
    return false;
  }

  const JSONParser parser{json_string.c_str()};

  auto root = parser.root();
  if (!root) {
    errors.push_back(std::string("Failed to parse calibration baseline at ") + calibration_file_path);
    return false;
  }

  return ParseLoads(json_getProperty(root, "loads"), errors, baseline_test_loads_);
}

std::map<std::string, uint32_t> RuntimeConfig::baseline_loads() const {
  std::map<std::string, uint32_t> ret;
  for (auto& [suite_name, loads] : baseline_test_loads_) {
    for (auto& [test_name, load] : loads) {
      ret[suite_name + "::" + test_name] = load;
    }
  }
  return ret;
}

bool RuntimeConfig::LoadConfigBuffer(const std::string& config_content, std::vector<std::string>& errors) {
  std::map<std::string, std::vector<std::string>> test_config;

  const JSONParser parser{config_content.c_str()};

  auto root = parser.root();
  if (!root) {
    errors.emplace_back("Failed to parse config file.");
    return false;
  }

  auto settings = json_getProperty(root, "settings");
  if (!settings) {
    errors.emplace_back("'settings' not found");
    return false;
  }
  if (json_getType(settings) != JSON_OBJ) {
    errors.emplace_back("'settings' not an object");
    return false;
  }

  if (!LoadBool(settings, "disable_autorun", disable_autorun_)) {
    errors.emplace_back("settings[disable_autorun] must be a boolean");
    return false;
  }

  if (!LoadBool(settings, "enable_autorun_immediately", enable_autorun_immediately_)) {
    errors.emplace_back("settings[enable_autorun_immediately] must be a boolean");
    return false;
  }

  if (!LoadBool(settings, "enable_shutdown_on_completion", enable_shutdown_on_completion_)) {
    errors.emplace_back("settings[enable_shutdown_on_completion] must be a boolean");
    return false;
  }

  if (!LoadBool(settings, "skip_tests_by_default", skip_tests_by_default_)) {
    errors.emplace_back("settings[skip_tests_by_default] must be a boolean");
    return false;
  }

  if (!LoadString(settings, "output_directory_path", output_directory_path_)) {
    errors.emplace_back("settings[output_directory_path] must be a string");
    return false;
  }
  output_directory_path_ = SanitizePath(output_directory_path_);

  if (!LoadUint32(settings, "reboot_or_shutdown_delay", reboot_or_shutdown_delay_ms_)) {
    errors.emplace_back("settings[reboot_or_shutdown_delay] must be an integer");
    return false;
  }

  {
    std::string run_mode;
    if (!LoadString(settings, "run_mode", run_mode)) {
      errors.emplace_back("settings[run_mode] must be a string");
      return false;
    }
    if (run_mode.empty() || run_mode == "single") {
      run_mode_ = RunMode::SINGLE;
    } else if (run_mode == "sustained") {
      run_mode_ = RunMode::SUSTAINED;
    } else if (run_mode == "calibrate") {
      run_mode_ = RunMode::CALIBRATE;
    } else {
      errors.emplace_back("settings[run_mode] must be one of 'single', 'sustained', 'calibrate'");
      return false;
    }
  }

  if (!LoadUint32(settings, "warmup_frames", warmup_frames_)) {
    errors.emplace_back("settings[warmup_frames] must be an integer");
    return false;
  }

  if (!LoadUint32(settings, "measure_frames", measure_frames_)) {
    errors.emplace_back("settings[measure_frames] must be an integer");
    return false;
  }

  if (!LoadUint32(settings, "calibration_probe_frames", calibration_probe_frames_)) {
    errors.emplace_back("settings[calibration_probe_frames] must be an integer");
    return false;
  }

  if (!LoadUint32(settings, "calibration_target_busy_p95_us", calibration_target_busy_p95_us_)) {
    errors.emplace_back("settings[calibration_target_busy_p95_us] must be an integer");
    return false;
  }

  // Loads live in their own top-level object, keyed by "Suite::Test", so that tools which rewrite "test_suites" to
  // filter tests (e.g., xemu-perf-tester) do not discard calibration data.
  if (!ParseLoads(json_getProperty(root, "loads"), errors, configured_test_loads_)) {
    return false;
  }

  auto test_suites = json_getProperty(root, "test_suites");
  if (!test_suites) {
    return true;
  }

  if (json_getType(test_suites) != JSON_OBJ) {
    errors.emplace_back("'test_suites' not an object");
    return false;
  }

  return ParseTestSuites(test_suites, errors, configured_test_suites_, configured_test_cases_);
}

static RuntimeConfig::SkipConfiguration MakeSkipConfiguration(bool is_skipped) {
  if (is_skipped) {
    return RuntimeConfig::SkipConfiguration::SKIPPED;
  }
  return RuntimeConfig::SkipConfiguration::UNSKIPPED;
}

static bool ParseTestCase(json_t const* test_case, const std::string& suite_name, const std::string& test_name,
                          std::vector<std::string>& errors, RuntimeConfig::SkipConfiguration& config_value,
                          const std::string& test_case_error_message_prefix) {
  for (auto element = json_getChild(test_case); element; element = json_getSibling(element)) {
    std::string name = json_getName(element);
    auto type = json_getType(element);

    if (name != "skipped") {
      errors.emplace_back(test_case_error_message_prefix + "[" + name + "] unsupported. Ignoring");
      continue;
    }

    if (type == JSON_BOOLEAN) {
      config_value = MakeSkipConfiguration(json_getBoolean(element));
    } else {
      errors.emplace_back(test_case_error_message_prefix + "[skipped] must be a boolean");
      return false;
    }
  }

  return true;
}

static bool ParseTestCases(
    json_t const* test_suite, const std::string& suite_name, std::vector<std::string>& errors,
    std::map<std::string, RuntimeConfig::SkipConfiguration>& configured_suites,
    std::map<std::string, std::map<std::string, RuntimeConfig::SkipConfiguration>>& configured_cases,
    const std::string& suite_error_message_prefix) {
  std::map<std::string, RuntimeConfig::SkipConfiguration> case_settings;

  for (auto test_or_skipped = json_getChild(test_suite); test_or_skipped;
       test_or_skipped = json_getSibling(test_or_skipped)) {
    std::string test_name = json_getName(test_or_skipped);
    auto type = json_getType(test_or_skipped);

    if (test_name == "skipped") {
      if (type == JSON_BOOLEAN) {
        configured_suites[suite_name] = MakeSkipConfiguration(json_getBoolean(test_or_skipped));
        continue;
      }

      errors.emplace_back(suite_error_message_prefix + "[skipped] must be a boolean.");
      return false;
    }

    if (type == JSON_OBJ) {
      auto config_value = RuntimeConfig::SkipConfiguration::DEFAULT;
      if (!ParseTestCase(test_or_skipped, suite_name, test_name, errors, config_value,
                         suite_error_message_prefix + "[" + test_name + "]")) {
        return false;
      }
      if (config_value != RuntimeConfig::SkipConfiguration::DEFAULT) {
        case_settings[test_name] = config_value;
      }
    } else {
      errors.emplace_back(suite_error_message_prefix + "[" + test_name + "] must be an object. Ignoring");
      continue;
    }
  }

  if (!case_settings.empty()) {
    configured_cases[suite_name] = case_settings;
  }

  return true;
}

static bool ParseTestSuites(
    json_t const* test_suites, std::vector<std::string>& errors,
    std::map<std::string, RuntimeConfig::SkipConfiguration>& skipped_test_suites,
    std::map<std::string, std::map<std::string, RuntimeConfig::SkipConfiguration>>& skipped_test_cases) {
  std::string test_suites_error_message_prefix("test_suites[");
  for (auto suite = json_getChild(test_suites); suite; suite = json_getSibling(suite)) {
    std::string suite_name = json_getName(suite);
    auto suite_error_message_prefix = test_suites_error_message_prefix + suite_name + "]";

    if (json_getType(suite) != JSON_OBJ) {
      errors.emplace_back(suite_error_message_prefix + " must be an object. Ignoring");
      continue;
    }

    if (!ParseTestCases(suite, suite_name, errors, skipped_test_suites, skipped_test_cases,
                        suite_error_message_prefix)) {
      return false;
    }
  }

  return true;
}

bool RuntimeConfig::ApplyConfig(std::vector<std::shared_ptr<TestSuite>>& test_suites,
                                std::vector<std::string>& errors) {
  std::vector<std::shared_ptr<TestSuite>> filtered_test_suites;

  for (auto& suite : test_suites) {
    auto default_skip_test_case = skip_tests_by_default_;

    auto entry = configured_test_suites_.find(suite->Name());
    if (entry != configured_test_suites_.end()) {
      switch (entry->second) {
        case SkipConfiguration::SKIPPED:
          default_skip_test_case = true;
          break;
        case SkipConfiguration::UNSKIPPED:
          default_skip_test_case = false;
          break;
        default:
          break;
      }
    }

    auto test_case_config = configured_test_cases_.find(suite->Name());
    std::set<std::string> skipped_test_cases;

    for (auto& test_case : suite->TestNames()) {
      bool skip_test_case = default_skip_test_case;

      if (test_case_config != configured_test_cases_.end()) {
        auto explicit_config = test_case_config->second.find(test_case);
        if (explicit_config != test_case_config->second.end()) {
          skip_test_case = explicit_config->second == SkipConfiguration::SKIPPED;
        }
      }

      if (skip_test_case) {
        skipped_test_cases.insert(test_case);
      }
    }

    if (!skipped_test_cases.empty()) {
      suite->DisableTests(skipped_test_cases);
    }

    auto baseline_loads = baseline_test_loads_.find(suite->Name());
    if (baseline_loads != baseline_test_loads_.end()) {
      for (auto& [test_name, load] : baseline_loads->second) {
        suite->SetBaselineLoad(test_name, load);
      }
    }

    auto loads = configured_test_loads_.find(suite->Name());
    if (loads != configured_test_loads_.end()) {
      for (auto& [test_name, load] : loads->second) {
        suite->SetLoad(test_name, load);
      }
    }

    if (suite->HasEnabledTests()) {
      filtered_test_suites.push_back(suite);
    }
  }

  test_suites = filtered_test_suites;

  return true;
}

std::string RuntimeConfig::SanitizePath(const std::string& path) {
  std::string sanitized(path);
  std::replace(sanitized.begin(), sanitized.end(), '/', '\\');
  return std::move(sanitized);
}
