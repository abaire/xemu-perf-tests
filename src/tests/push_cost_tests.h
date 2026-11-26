#ifndef XEMU_PERF_TESTS_PUSH_COST_TESTS_H
#define XEMU_PERF_TESTS_PUSH_COST_TESTS_H

#include <memory>
#include <string>

#include "test_suite.h"

/**
 * Diagnostics that break down the CPU cost of building a pushbuffer. Each test writes the same stream of single-DWORD
 * NV097_NO_OPERATION methods, `load` DWORDs per frame, to a different destination or through a different path, so
 * comparing their `submit_minus_reset_us` separates the cost of the memory being written, of the GPU consuming the
 * commands, and of the pbkitplusplus Pushbuffer API.
 *
 * These are not benchmarks, and are excluded from calibration.
 */
class PushCostTests : public TestSuite {
 public:
  PushCostTests(TestHost &host, std::string output_dir, const Config &config);

  void Initialize() override;
  void Deinitialize() override;

 private:
  void TestRamWrite();
  void TestPushbufferWrite();
  void TestPushbufferSubmit();
  void TestPushbufferAPI();

  //! Runs `body` with the standard frame setup and reports results under `test_name`.
  void RunFrame(const char *test_name, const std::function<void()> &body);

  //! Ordinary cached RAM, used as the destination for RamWrite.
  std::unique_ptr<uint32_t[]> ram_buffer_;
};

#endif  // XEMU_PERF_TESTS_PUSH_COST_TESTS_H
