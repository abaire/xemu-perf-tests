#ifndef XEMU_PERF_TESTS_LOGGER_H
#define XEMU_PERF_TESTS_LOGGER_H

#include <fstream>
#include <string>

class Logger {
 public:
  static void Initialize(const std::string &log_path, bool truncate_log);

  static std::ofstream Log();

 private:
  explicit Logger(const std::string &path, bool truncate_log = false);

  std::string log_path_;

  static Logger *singleton_;
};

#endif  // XEMU_PERF_TESTS_LOGGER_H
