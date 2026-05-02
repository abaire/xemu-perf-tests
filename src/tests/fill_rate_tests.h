#ifndef XEMU_PERF_TESTS_FILL_RATE_TESTS_H
#define XEMU_PERF_TESTS_FILL_RATE_TESTS_H

#include "test_suite.h"
#include "vertex_buffer.h"
#include <memory>
#include <vector>

/**
 * Benchmarks fill rate by drawing hundreds of full screen quads.
 */
class FillRateTests : public TestSuite {
 public:
  FillRateTests(TestHost &host, std::string output_dir, const Config &config);

  void Initialize() override;
  void Deinitialize() override;

 private:
  void TestFillRate(bool use_texture);

  std::shared_ptr<PBKitPlusPlus::VertexBuffer> vertex_buffer_;
};

#endif  // XEMU_PERF_TESTS_FILL_RATE_TESTS_H
