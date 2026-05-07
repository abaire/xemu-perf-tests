#ifndef XEMU_PERF_TESTS_SURFACE_RENDERING_TESTS_H
#define XEMU_PERF_TESTS_SURFACE_RENDERING_TESTS_H

#include "test_suite.h"

/**
 * Tests behavior when rendering to offscreen surfaces with a final textured composition at the end.
 */
class SurfaceRenderingTests : public TestSuite {
 public:
  SurfaceRenderingTests(TestHost &host, std::string output_dir, const Config &config);

  void Initialize() override;

 private:
  void Test();
};

#endif  // XEMU_PERF_TESTS_SURFACE_RENDERING_TESTS_H
