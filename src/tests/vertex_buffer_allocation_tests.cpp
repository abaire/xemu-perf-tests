#include "vertex_buffer_allocation_tests.h"

#include <pbkit/pbkit.h>

#include "debug_output.h"
// #include "pbkit_ext.h"
#include "shaders/passthrough_vertex_shader.h"
#include "test_host.h"
#include "vertex_buffer.h"

using namespace PBKitPlusPlus;

static constexpr char kMixedVertexCountTest[] = "MixedVtxAlloc";

static constexpr uint32_t kMixedVertexCounts[] = {
    0x2a12, 0x17cdc, 0xcb43,  0x91f5,  0x15225, 0x14a0f, 0x12921, 0x12327,
    0x3c,   0x1bde6, 0x1b31e, 0x1a2e3, 0x1d001, 0x1FFE0, 0x12a7a, 0x9ef7,
};

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

VertexBufferAllocationTests::VertexBufferAllocationTests(TestHost &host, std::string output_dir, const Config &config)
    : TestSuite(host, std::move(output_dir), "Vertex buffer allocation", config) {
  for (auto draw_mode : {DRAW_ARRAYS, DRAW_INLINE_BUFFERS, DRAW_INLINE_ARRAYS, DRAW_INLINE_ELEMENTS}) {
    auto name = MakeTestName(kMixedVertexCountTest, draw_mode);
    tests_[name] = [this, name, draw_mode]() { TestMixedSizes(name, draw_mode); };
  }
}

static void CreateGeometry(TestHost &host, std::vector<uint32_t> &index_buffer, uint32_t target_array_entries) {
  index_buffer.clear();

  static constexpr float kInset = 2.f;
  static constexpr float kTop = 48.f;
  static constexpr float kQuadSize = 16.f;
  static constexpr float kQuadZ = 0.f;
  const float framebuffer_width = host.GetFramebufferWidthF();
  const float framebuffer_height = host.GetFramebufferHeightF();

  // Array entries per vertex in test = (4 position, 1 weight, 4 diffuse, 4 specular, 2 texcoord0)
  static constexpr uint32_t kArrayEntriesPerVertex = 15;
  const uint32_t target_quads = target_array_entries / (4 * kArrayEntriesPerVertex);
  ASSERT(target_quads > 0);

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

void VertexBufferAllocationTests::Initialize() { TestSuite::Initialize(); }

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

  uint32_t attributes =
      TestHost::POSITION | TestHost::DIFFUSE | TestHost::SPECULAR | TestHost::WEIGHT | TestHost::TEXCOORD0;

  TestHost::ProfileResults results{};
  switch (draw_mode) {
    case DRAW_ARRAYS:
      results = Profile(name, 10, [this, &attributes] {
        for (unsigned int kMixedVertexCount : kMixedVertexCounts) {
          std::vector<uint32_t> index_buffer;
          CreateGeometry(host_, index_buffer, kMixedVertexCount);
          host_.DrawArrays(attributes, kPrimitive);
          host_.ClearVertexBuffer();
          index_buffer.clear();
        }
      });
      break;

    case DRAW_INLINE_BUFFERS:
      results = Profile(name, 10, [this, &attributes] {
        for (unsigned int kMixedVertexCount : kMixedVertexCounts) {
          std::vector<uint32_t> index_buffer;
          CreateGeometry(host_, index_buffer, kMixedVertexCount);
          host_.DrawInlineBuffer(attributes, kPrimitive);
          host_.ClearVertexBuffer();
          index_buffer.clear();
        }
      });
      break;

    case DRAW_INLINE_ELEMENTS:
      results = Profile(name, 10, [this, &attributes] {
        for (unsigned int kMixedVertexCount : kMixedVertexCounts) {
          std::vector<uint32_t> index_buffer;
          CreateGeometry(host_, index_buffer, kMixedVertexCount);
          host_.DrawInlineElements16(index_buffer, attributes, kPrimitive);
          host_.ClearVertexBuffer();
          index_buffer.clear();
        }
      });
      break;

    case DRAW_INLINE_ARRAYS:
      results = Profile(name, 10, [this, &attributes] {
        for (unsigned int kMixedVertexCount : kMixedVertexCounts) {
          std::vector<uint32_t> index_buffer;
          CreateGeometry(host_, index_buffer, kMixedVertexCount);
          host_.DrawInlineArray(attributes, kPrimitive);
          host_.ClearVertexBuffer();
          index_buffer.clear();
        }
      });
      break;
  }

  host_.FinishDraw(suite_name_, name, results);
}
