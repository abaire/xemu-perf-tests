#ifndef XEMU_PERF_TESTS_HIGH_VERTEX_COUNT_TESTS_H
#define XEMU_PERF_TESTS_HIGH_VERTEX_COUNT_TESTS_H

#include <memory>
#include <vector>

#include "test_host.h"
#include "test_suite.h"

namespace PBKitPlusPlus {
class VertexBuffer;
}

using namespace PBKitPlusPlus;

/**
 * Tests behavior when large numbers of vertices are specified without an END
 * call.
 */
class HighVertexCountTests : public TestSuite {
 public:
  enum class DrawMode : int {
    DRAW_ARRAYS,
    DRAW_INLINE_BUFFERS,
    DRAW_INLINE_ARRAYS,
    DRAW_INLINE_ELEMENTS,
  };

 public:
  HighVertexCountTests(TestHost &host, std::string output_dir, const Config &config);

  void Initialize() override;
  void Deinitialize() override;

 private:
  void Test(const std::string &name, DrawMode mode);

 private:
  struct GeometryHolder {
    std::shared_ptr<VertexBuffer> vertex_buffer;
    std::vector<uint32_t> index_buffer;

    void reset() {
      vertex_buffer.reset();
      index_buffer.clear();
    }
  };

  GeometryHolder single_frame_geometry_;
  GeometryHolder continuous_geometry_[4];
};

#endif  // XEMU_PERF_TESTS_HIGH_VERTEX_COUNT_TESTS_H
