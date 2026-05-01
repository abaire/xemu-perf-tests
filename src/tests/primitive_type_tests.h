#ifndef XEMU_PERF_TESTS_PRIMITIVE_TYPE_TESTS_H
#define XEMU_PERF_TESTS_PRIMITIVE_TYPE_TESTS_H

#include "test_suite.h"

/**
 * Tests rendering of various primitive types with a reasonably small number of
 * vertices. Intended to catch driver tesselation issues related to complex primitives (e.g., quads, polygons).
 */
class PrimitiveTypeTests : public TestSuite {
 public:
  PrimitiveTypeTests(TestHost &host, std::string output_dir, const Config &config);

  void Initialize() override;

 private:
  void Test(const std::string &name, const TestHost::DrawPrimitive primitive, bool use_vsh);
};

#endif  // XEMU_PERF_TESTS_PRIMITIVE_TYPE_TESTS_H
