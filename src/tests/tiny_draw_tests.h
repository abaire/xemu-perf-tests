#ifndef XEMU_PERF_TESTS_TINY_DRAW_TESTS_H
#define XEMU_PERF_TESTS_TINY_DRAW_TESTS_H

#include <memory>
#include <vector>

#include "test_suite.h"

namespace PBKitPlusPlus {
class VertexBuffer;
}

/**
 * Draws hundreds of extremely small pieces of geometry to test pathologically
 * unoptimized draw invocations without fill rate limiting.
 */
class TinyDrawTests : public TestSuite {
 public:
  enum class DrawMode {
    DRAW_ARRAYS,
    DRAW_INLINE_BUFFERS,
    DRAW_INLINE_ARRAYS,
    DRAW_INLINE_ELEMENTS,
  };

 public:
  TinyDrawTests(TestHost &host, std::string output_dir, const Config &config);

  void Initialize() override;
  void Deinitialize() override;

 private:
  void Test(const std::string &test_name, DrawMode draw_mode, bool use_vsh);

 private:
  std::shared_ptr<PBKitPlusPlus::VertexBuffer> vertex_buffer_;
  std::vector<uint32_t> index_buffer_;
};

#endif  // XEMU_PERF_TESTS_TINY_DRAW_TESTS_H
