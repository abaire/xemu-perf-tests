#ifndef XEMU_PERF_TESTS_VERTEX_BUFFER_ALLOCATION_TESTS_H
#define XEMU_PERF_TESTS_VERTEX_BUFFER_ALLOCATION_TESTS_H

#include <memory>
#include <vector>

#include "test_host.h"
#include "test_suite.h"

namespace PBKitPlusPlus {
class VertexBuffer;
}

using namespace PBKitPlusPlus;

/**
 * Tests behavior of pathological GL buffer allocations due to use of certain vertex specification methods.
 */
class VertexBufferAllocationTests : public TestSuite {
 public:
  enum DrawMode {
    DRAW_ARRAYS,
    DRAW_INLINE_BUFFERS,
    DRAW_INLINE_ARRAYS,
    DRAW_INLINE_ELEMENTS,
    DRAW_INLINE_ARRAYS_INLINE_ELEMENTS_INTERSPERSED,
  };

 public:
  VertexBufferAllocationTests(TestHost &host, std::string output_dir, const Config &config);

  void Initialize() override;
  void Deinitialize() override;

 private:
  void TestMixedSizes(const std::string &name, DrawMode mode);
};

#endif  // XEMU_PERF_TESTS_VERTEX_BUFFER_ALLOCATION_TESTS_H
