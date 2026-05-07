#ifndef XEMU_PERF_TESTS_BUSY_PFIFO_TESTS_H
#define XEMU_PERF_TESTS_BUSY_PFIFO_TESTS_H

#include "test_suite.h"

/**
 * Saturates the PFIFO queue with irrelevant commands and performs a handful of draws designed to render efficiently
 * without being fill rate limited. Intended to identify unintentional stalls in processing the PFIFO command buffer.
 */
class BusyPfifoTests : public TestSuite {
 public:
  BusyPfifoTests(TestHost &host, std::string output_dir, const Config &config);

  void Initialize() override;

 private:
  void Test();
};

#endif  // XEMU_PERF_TESTS_BUSY_PFIFO_TESTS_H
