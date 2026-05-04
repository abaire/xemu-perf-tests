#ifndef XEMU_PERF_TESTS_UNIFORM_THRASH_TESTS_H
#define XEMU_PERF_TESTS_UNIFORM_THRASH_TESTS_H

#include "test_suite.h"

/**
 * Performs pathological updates to vertex shader uniforms.
 */
class UniformThrashTests : public TestSuite {
 public:
  UniformThrashTests(TestHost &host, std::string output_dir, const Config &config);

  void Initialize() override;

 private:
  void Test();
};

#endif  // XEMU_PERF_TESTS_UNIFORM_THRASH_TESTS_H
