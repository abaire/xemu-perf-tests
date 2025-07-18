#include "high_vertex_count_tests.h"

#include <pbkit/pbkit.h>

#include "debug_output.h"
// #include "pbkit_ext.h"
#include "shaders/passthrough_vertex_shader.h"
#include "test_host.h"
#include "vertex_buffer.h"

using namespace PBKitPlusPlus;

static constexpr char kHighVertexCountTest[] = "HighVtxCount";

static constexpr uint32_t kMaxVertexCount = 0x07FFFF;

static std::string MakeTestName(const std::string &prefix, HighVertexCountTests::DrawMode draw_mode) {
  std::string ret = prefix;

  switch (draw_mode) {
    case HighVertexCountTests::DRAW_ARRAYS:
      ret += "-arrays";
      break;
    case HighVertexCountTests::DRAW_INLINE_BUFFERS:
      ret += "-inlinebuffers";
      break;
    case HighVertexCountTests::DRAW_INLINE_ARRAYS:
      ret += "-inlinearrays";
      break;
    case HighVertexCountTests::DRAW_INLINE_ELEMENTS:
      ret += "-inlineelements";
      break;
  }

  return ret;
}

/**
 * Initializes the test suite and creates test cases.
 *
 * @tc HighVtxCount-arrays
 *  Tests NV097_DRAW_ARRAYS with a very large number of vertices.
 *
 * @tc HighVtxCount-inlinebuffers
 *  Tests immediate mode (e.g., NV097_SET_VERTEX3F) with a very large number of vertices.
 *
 * @tc HighVtxCount-inlinearrays
 *  Tests NV097_INLINE_ARRAY with a very large number of vertices.
 *
 * @tc HighVtxCount-inlineelements
 *  Tests NV097_ARRAY_ELEMENT16 / NV097_ARRAY_ELEMENT32 with a very large number of vertices.
 *
 */
HighVertexCountTests::HighVertexCountTests(TestHost &host, std::string output_dir, const Config &config)
    : TestSuite(host, std::move(output_dir), "High vertex count", config) {
  for (auto draw_mode : {DRAW_ARRAYS, DRAW_INLINE_BUFFERS, DRAW_INLINE_ARRAYS, DRAW_INLINE_ELEMENTS}) {
    std::string name = MakeTestName(kHighVertexCountTest, draw_mode);
    tests_[name] = [this, name, draw_mode]() { Test(name, draw_mode); };
  }
}

static void CreateGeometry(TestHost &host, std::shared_ptr<VertexBuffer> &vertex_buffer,
                           std::vector<uint32_t> &index_buffer) {
  vertex_buffer.reset();
  index_buffer.clear();

  static constexpr float kInset = 2.f;
  static constexpr float kTop = 48.f;
  static constexpr float kQuadSize = 6.f;
  static constexpr float kQuadZ = 0.f;
  const float framebuffer_width = host.GetFramebufferWidthF();
  const float framebuffer_height = host.GetFramebufferHeightF();

  // Array entries per vertex in test = (4 position, 1 weight, 4 diffuse, 4 specular, 2 texcoord0)
  static constexpr uint32_t kArrayEntriesPerVertex = 15;

  // From King of Fighters 2003, Noah Sky 2 level, at least 0x410FA = 266490 are used, each vertex using 15 entries.
  // static constexpr uint32_t kTargetArrayEntries = 0x410FA;

  // From xemu NV2A_MAX_BATCH_LENGTH = 0x1FFFF = 131071
  // static constexpr uint32_t kTargetArrayEntries = 0x1FFFF - kArrayEntriesPerVertex;

  // Confirmed on Xbox 1.0 that 0x0FFFFF works for draw methods other than DRAW_ARRAYS.
  // static constexpr uint32_t kTargetArrayEntries = 0x0FFFFF;

  // In practice, King of Fighters 2003 seems to be the only game that uses a very large number, to speed up the tests
  // an arbitrary cap higher than KoF2k3 is selected.
  //  static constexpr uint32_t kTargetArrayEntries = 0x07FFFF;

  // Note, this value will also be under the separate DrawArrays limit of 0xFFFF vertices. That may be a general
  // hardware limit.
  const uint32_t target_quads = kMaxVertexCount / (4 * kArrayEntriesPerVertex);

  const auto quads_per_row = static_cast<int>((framebuffer_width - (kInset * 2.f)) / kQuadSize);
  const float bottom_row_y = (framebuffer_height - kQuadSize);

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

  vertex_buffer = host.AllocateVertexBuffer(target_quads * 4);
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

  uint32_t quad_count = 0;
  uint32_t vertex_index = 0;
  float top = kTop;
  float alpha = 1.f;
  while (quad_count < target_quads) {
    float left = kInset;
    for (auto x = 0; x < quads_per_row && quad_count < target_quads; ++x, left += kQuadSize) {
      add_quad(left, top, alpha);
      ++quad_count;
      index_buffer.emplace_back(vertex_index++);
      index_buffer.emplace_back(vertex_index++);
      index_buffer.emplace_back(vertex_index++);
      index_buffer.emplace_back(vertex_index++);
    }

    top += kQuadSize;
    if (top > bottom_row_y) {
      top = kTop;
      red = 1.f;
      green = 0.f;
      blue = 1.f;
      alpha *= 0.75f;
    }
  }

  vertex_buffer->Unlock();
}

void HighVertexCountTests::Initialize() {
  TestSuite::Initialize();

  CreateGeometry(host_, vertex_buffer_, index_buffer_);
}

void HighVertexCountTests::Deinitialize() {
  host_.ClearVertexBuffer();
  vertex_buffer_.reset();
  index_buffer_.clear();
  TestSuite::Deinitialize();
}

//! Test the arbitrary maximum number of vertices per draw.
void HighVertexCountTests::Test(const std::string &name, DrawMode draw_mode) {
  auto shader = std::make_shared<PassthroughVertexShader>();
  host_.SetVertexShaderProgram(shader);

  host_.SetVertexBuffer(vertex_buffer_);

  static constexpr uint32_t kBackgroundColor = 0xFF444444;
  host_.PrepareDraw(kBackgroundColor);

  static constexpr auto kPrimitive = TestHost::PRIMITIVE_QUADS;

  uint32_t attributes =
      TestHost::POSITION | TestHost::DIFFUSE | TestHost::SPECULAR | TestHost::WEIGHT | TestHost::TEXCOORD0;

  TestHost::ProfileResults results{};
  switch (draw_mode) {
    case DRAW_ARRAYS:
      results = Profile(name, 100, [this, &attributes] { host_.DrawArrays(attributes, kPrimitive); });
      break;

    case DRAW_INLINE_BUFFERS:
      results = Profile(name, 5, [this, &attributes] { host_.DrawInlineBuffer(attributes, kPrimitive); });
      break;

    case DRAW_INLINE_ELEMENTS:
      results =
          Profile(name, 50, [this, &attributes] { host_.DrawInlineElements16(index_buffer_, attributes, kPrimitive); });
      break;

    case DRAW_INLINE_ARRAYS:
      results = Profile(name, 10, [this, &attributes] { host_.DrawInlineArray(attributes, kPrimitive); });
      break;
  }

  host_.FinishDraw(suite_name_, name, results);
}
