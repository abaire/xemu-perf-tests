#include "vertex_buffer_allocation_tests.h"

#include <pbkit/pbkit.h>

#include "debug_output.h"
// #include "pbkit_ext.h"
#include "shaders/passthrough_vertex_shader.h"
#include "test_host.h"
#include "vertex_buffer.h"

using namespace PBKitPlusPlus;

static constexpr char kTinyAllocationTest[] = "TinyAlloc";
static constexpr char kMixedVertexCountTest[] = "MixedVtxAlloc";

static constexpr uint32_t kMixedVertexBufferSizes[] = {
    0x2a12, 0x17cdc, 0xcb43,  0x91f5,  0x15225, 0x14a0f, 0x12921, 0x12327,
    0x3c,   0x1bde6, 0x1b31e, 0x1a2e3, 0x1d001, 0x1FFE0, 0x12a7a, 0x9ef7,
};

static uint32_t kVertexAttributes =
    TestHost::POSITION | TestHost::DIFFUSE | TestHost::SPECULAR | TestHost::WEIGHT | TestHost::TEXCOORD0;

// Keep in sync with the size required for kVertexAttributes
// 4 position, 1 weight, 4 diffuse, 4 specular, 2 texcoord0
static constexpr uint32_t kArrayEntriesPerVertex = 15;

static constexpr uint32_t kSmallestVertexBufferSize = kArrayEntriesPerVertex * 4;

static std::string MakeTestName(const std::string &prefix, VertexBufferAllocationTests::DrawMode draw_mode) {
  std::string ret = prefix;

  switch (draw_mode) {
    case VertexBufferAllocationTests::DRAW_ARRAYS:
      ret += "-arrays";
      break;
    case VertexBufferAllocationTests::DRAW_INLINE_BUFFERS:
      ret += "-inlinebuffers";
      break;
    case VertexBufferAllocationTests::DRAW_INLINE_ARRAYS:
      ret += "-inlinearrays";
      break;
    case VertexBufferAllocationTests::DRAW_INLINE_ELEMENTS:
      ret += "-inlineelements";
      break;
  }

  return ret;
}

/**
 * Initializes the test suite and creates test cases.
 *
 * @tc MixedVtxAlloc-arrays
 *  Tests NV097_DRAW_ARRAYS with multiple successive draws using a variety of different vertex counts.
 *
 * @tc MixedVtxAlloc-inlinebuffers
 *  Tests immediate mode (e.g., NV097_SET_VERTEX3F) with multiple successive draws using a variety of different vertex
 *  counts.
 *
 * @tc MixedVtxAlloc-inlinearrays
 *  Tests NV097_INLINE_ARRAY with multiple successive draws using a variety of different vertex counts.
 *
 * @tc MixedVtxAlloc-inlineelements
 *  Tests NV097_ARRAY_ELEMENT16 / NV097_ARRAY_ELEMENT32 with multiple successive draws using a variety of different
 *  vertex counts.
 *
 * @tc TinyAlloc-arrays
 *  Tests NV097_DRAW_ARRAYS with a large number of single quad draws.
 *
 * @tc TinyAlloc-inlinebuffers
 *  Tests immediate mode (e.g., NV097_SET_VERTEX3F) with a large number of single quad draws.
 *
 * @tc TinyAlloc-inlinearrays
 *  Tests NV097_INLINE_ARRAY with a large number of single quad draws.
 *
 * @tc TinyAlloc-inlineelements
 *  Tests NV097_ARRAY_ELEMENT16 / NV097_ARRAY_ELEMENT32 with a large number of single quad draws.
 */
VertexBufferAllocationTests::VertexBufferAllocationTests(TestHost &host, std::string output_dir, const Config &config)
    : TestSuite(host, std::move(output_dir), "Vertex buffer allocation", config) {
  for (auto draw_mode : {DRAW_ARRAYS, DRAW_INLINE_BUFFERS, DRAW_INLINE_ARRAYS, DRAW_INLINE_ELEMENTS}) {
    auto name = MakeTestName(kMixedVertexCountTest, draw_mode);
    tests_[name] = [this, name, draw_mode]() { TestMixedSizes(name, draw_mode); };

    name = MakeTestName(kTinyAllocationTest, draw_mode);
    tests_[name] = [this, name, draw_mode]() { TestTinyAllocations(name, draw_mode); };
  }
}

static void CreateGeometry(TestHost &host, std::vector<uint32_t> &index_buffer, uint32_t target_array_entries) {
  index_buffer.clear();

  static constexpr float kQuadSize = 16.f;
  static constexpr float kQuadZ = 0.f;

  const uint32_t target_quads = target_array_entries / (4 * kArrayEntriesPerVertex);
  ASSERT(target_quads > 0);

  float red = 0.f;
  float green = 0.5f;
  float blue = 0.75f;

  static constexpr float kRedInc = 0.03f;
  static constexpr float kGreenInc = 0.05f;
  static constexpr float kBlueInc = 0.01f;

  auto increment_colors = [&red, &green, &blue]() {
    red += kRedInc;
    green += kGreenInc;
    blue += kBlueInc;

    if (red > 1.f) {
      red -= 1.f;
    }
    if (green > 1.f) {
      green -= 1.f;
    }
    if (blue > 1.f) {
      blue -= 1.f;
    }
  };

  auto vertex_buffer = host.AllocateVertexBuffer(target_quads * 4);
  vertex_buffer->SetPositionIncludesW(true);
  auto vertex = vertex_buffer->Lock();
  auto add_quad = [&vertex, &red, &green, &blue, &increment_colors](float left, float top, float alpha) {
    vertex->SetPosition(left, top, kQuadZ);
    vertex->SetDiffuse(red, green, blue, alpha);
    increment_colors();
    vertex->SetSpecular(red, green, blue, alpha);
    increment_colors();
    vertex->SetWeight(0.f);
    vertex->SetTexCoord0(0.f, 0.f);
    ++vertex;

    vertex->SetPosition(left + kQuadSize, top, kQuadZ);
    vertex->SetDiffuse(red, green, blue, alpha);
    increment_colors();
    vertex->SetSpecular(red, green, blue, alpha);
    increment_colors();
    vertex->SetWeight(1.f);
    vertex->SetTexCoord0(1.f, 0.f);
    ++vertex;

    vertex->SetPosition(left + kQuadSize, top + kQuadSize, kQuadZ);
    vertex->SetDiffuse(red, green, blue, alpha);
    increment_colors();
    vertex->SetSpecular(red, green, blue, alpha);
    increment_colors();
    vertex->SetWeight(2.f);
    vertex->SetTexCoord0(1.f, 1.f);
    ++vertex;

    vertex->SetPosition(left, top + kQuadSize, kQuadZ);
    vertex->SetDiffuse(red, green, blue, alpha);
    increment_colors();
    vertex->SetSpecular(red, green, blue, alpha);
    increment_colors();
    vertex->SetWeight(3.f);
    vertex->SetTexCoord0(0.f, 1.f);
    ++vertex;
  };

  uint32_t vertex_index = 0;
  const auto x_range = static_cast<uint32_t>(host.GetFramebufferWidth() - kQuadSize);
  const auto y_range = static_cast<uint32_t>(host.GetFramebufferHeight() - kQuadSize);
  float alpha = 1.f;
  for (auto quad_count = 0; quad_count < target_quads; ++quad_count) {
    auto left = static_cast<float>(rand() % x_range);
    auto top = static_cast<float>(rand() % y_range);
    add_quad(left, top, alpha);
    index_buffer.emplace_back(vertex_index++);
    index_buffer.emplace_back(vertex_index++);
    index_buffer.emplace_back(vertex_index++);
    index_buffer.emplace_back(vertex_index++);
  }

  vertex_buffer->Unlock();
}

void VertexBufferAllocationTests::Initialize() {
  TestSuite::Initialize();
  srand(0x12345678);
}

void VertexBufferAllocationTests::Deinitialize() {
  host_.ClearVertexBuffer();
  TestSuite::Deinitialize();
}

void VertexBufferAllocationTests::TestMixedSizes(const std::string &name,
                                                 VertexBufferAllocationTests::DrawMode draw_mode) {
  auto shader = std::make_shared<PassthroughVertexShader>();
  host_.SetVertexShaderProgram(shader);

  static constexpr uint32_t kBackgroundColor = 0xFF444444;
  host_.PrepareDraw(kBackgroundColor);

  static constexpr auto kPrimitive = TestHost::PRIMITIVE_QUADS;

  TestHost::ProfileResults results{};
  switch (draw_mode) {
    case DRAW_ARRAYS:
      results = Profile(name, 10, [this] {
        for (unsigned int kMixedVertexCount : kMixedVertexBufferSizes) {
          std::vector<uint32_t> index_buffer;
          CreateGeometry(host_, index_buffer, kMixedVertexCount);
          host_.DrawArrays(kVertexAttributes, kPrimitive);
          host_.ClearVertexBuffer();
          index_buffer.clear();
        }
      });
      break;

    case DRAW_INLINE_BUFFERS:
      results = Profile(name, 10, [this] {
        for (unsigned int kMixedVertexCount : kMixedVertexBufferSizes) {
          std::vector<uint32_t> index_buffer;
          CreateGeometry(host_, index_buffer, kMixedVertexCount);
          host_.DrawInlineBuffer(kVertexAttributes, kPrimitive);
          host_.ClearVertexBuffer();
          index_buffer.clear();
        }
      });
      break;

    case DRAW_INLINE_ELEMENTS:
      results = Profile(name, 10, [this] {
        for (unsigned int kMixedVertexCount : kMixedVertexBufferSizes) {
          std::vector<uint32_t> index_buffer;
          CreateGeometry(host_, index_buffer, kMixedVertexCount);
          host_.DrawInlineElements16(index_buffer, kVertexAttributes, kPrimitive);
          host_.ClearVertexBuffer();
          index_buffer.clear();
        }
      });
      break;

    case DRAW_INLINE_ARRAYS:
      results = Profile(name, 10, [this] {
        for (unsigned int kMixedVertexCount : kMixedVertexBufferSizes) {
          std::vector<uint32_t> index_buffer;
          CreateGeometry(host_, index_buffer, kMixedVertexCount);
          host_.DrawInlineArray(kVertexAttributes, kPrimitive);
          host_.ClearVertexBuffer();
          index_buffer.clear();
        }
      });
      break;
  }

  host_.FinishDraw(suite_name_, name, results);
}

void VertexBufferAllocationTests::TestTinyAllocations(const std::string &name,
                                                      VertexBufferAllocationTests::DrawMode draw_mode) {
  auto shader = std::make_shared<PassthroughVertexShader>();
  host_.SetVertexShaderProgram(shader);

  static constexpr uint32_t kNumProfilingRuns = 10;
  static constexpr uint32_t kNumDraws = 500;
  static constexpr uint32_t kBackgroundColor = 0xFF333333;
  host_.PrepareDraw(kBackgroundColor);

  static constexpr auto kPrimitive = TestHost::PRIMITIVE_QUADS;

  TestHost::ProfileResults results{};
  switch (draw_mode) {
    case DRAW_ARRAYS:
      results = Profile(name, kNumProfilingRuns, [this] {
        std::vector<uint32_t> index_buffer;
        for (auto i = 0; i < kNumDraws; ++i) {
          CreateGeometry(host_, index_buffer, kSmallestVertexBufferSize);
          host_.DrawArrays(kVertexAttributes, kPrimitive);
          host_.ClearVertexBuffer();
          index_buffer.clear();
        }
      });
      break;

    case DRAW_INLINE_BUFFERS:
      results = Profile(name, kNumProfilingRuns, [this] {
        std::vector<uint32_t> index_buffer;
        for (auto i = 0; i < kNumDraws; ++i) {
          CreateGeometry(host_, index_buffer, kSmallestVertexBufferSize);
          host_.DrawInlineBuffer(kVertexAttributes, kPrimitive);
          host_.ClearVertexBuffer();
          index_buffer.clear();
        }
      });
      break;

    case DRAW_INLINE_ELEMENTS:
      results = Profile(name, kNumProfilingRuns, [this] {
        std::vector<uint32_t> index_buffer;
        for (auto i = 0; i < kNumDraws; ++i) {
          CreateGeometry(host_, index_buffer, kSmallestVertexBufferSize);
          host_.DrawInlineElements16(index_buffer, kVertexAttributes, kPrimitive);
          host_.ClearVertexBuffer();
          index_buffer.clear();
        }
      });
      break;

    case DRAW_INLINE_ARRAYS:
      results = Profile(name, kNumProfilingRuns, [this] {
        std::vector<uint32_t> index_buffer;
        for (auto i = 0; i < kNumDraws; ++i) {
          CreateGeometry(host_, index_buffer, kSmallestVertexBufferSize);
          host_.DrawInlineArray(kVertexAttributes, kPrimitive);
          host_.ClearVertexBuffer();
          index_buffer.clear();
        }
      });
      break;
  }

  host_.FinishDraw(suite_name_, name, results);
}
