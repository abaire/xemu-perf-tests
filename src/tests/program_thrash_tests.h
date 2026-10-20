#ifndef XEMU_PERF_TESTS_PROGRAM_THRASH_TESTS_H
#define XEMU_PERF_TESTS_PROGRAM_THRASH_TESTS_H

#include <map>
#include <memory>
#include <string>

#include "test_suite.h"

namespace PBKitPlusPlus {
class VertexBuffer;
}

/**
 * Performs pathological changes to the active vertex shader program between tiny draws.
 */
class ProgramThrashTests : public TestSuite {
 public:
  ProgramThrashTests(TestHost &host, std::string output_dir, const Config &config);

  void Initialize() override;
  void Deinitialize() override;

  //! Adjusts the number of draws performed by the active test in continuous mode, to aid calibration.
  void UpdateUserContext(int direction) override;

 private:
  [[nodiscard]] uint32_t GetNumDraws(const std::string &test_name);
  void PrintNumDraws(const std::string &test_name);

  void TestReupload(const std::string &test_name, bool long_shader);
  void TestResident(const std::string &test_name);

 private:
  std::shared_ptr<PBKitPlusPlus::VertexBuffer> vertex_buffer_;

  //! Map of test name to the number of draws performed per frame in continuous mode.
  std::map<std::string, uint32_t> num_draws_multi_frame_;
  //! The name of the most recently executed test, used to target UpdateUserContext.
  std::string active_test_;
};

#endif  // XEMU_PERF_TESTS_PROGRAM_THRASH_TESTS_H
