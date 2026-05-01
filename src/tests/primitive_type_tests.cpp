#include "primitive_type_tests.h"

#include <shaders/passthrough_vertex_shader.h>

#include "test_host.h"

static constexpr char kTestName[] = "PrimitiveType";

static constexpr uint32_t kIterations = 10;
static constexpr uint32_t kNumPrimitives = 1000;
static uint32_t kVertexAttributes = TestHost::POSITION | TestHost::DIFFUSE;

static std::string MakeTestName(const std::string &prefix, TestHost::DrawPrimitive primitive, bool use_vsh) {
  std::string ret = prefix;

  switch (primitive) {
    case TestHost::PRIMITIVE_POINTS:
      ret += "-Points";
      break;
    case TestHost::PRIMITIVE_LINES:
      ret += "-Lines";
      break;
    case TestHost::PRIMITIVE_LINE_LOOP:
      ret += "-LLoop";
      break;
    case TestHost::PRIMITIVE_LINE_STRIP:
      ret += "-LStrip";
      break;
    case TestHost::PRIMITIVE_TRIANGLES:
      ret += "-Tris";
      break;
    case TestHost::PRIMITIVE_TRIANGLE_STRIP:
      ret += "-TriStrip";
      break;
    case TestHost::PRIMITIVE_TRIANGLE_FAN:
      ret += "-TriFan";
      break;
    case TestHost::PRIMITIVE_QUADS:
      ret += "-Quads";
      break;
    case TestHost::PRIMITIVE_QUAD_STRIP:
      ret += "-QuadStrip";
      break;
    case TestHost::PRIMITIVE_POLYGON:
      ret += "-Poly";
      break;
  }

  if (use_vsh) {
    ret += "-vsh";
  }

  return ret;
}

PrimitiveTypeTests::PrimitiveTypeTests(TestHost &host, std::string output_dir, const Config &config)
    : TestSuite(host, std::move(output_dir), "PrimitiveType", config) {
  for (auto primitive : {
           TestHost::PRIMITIVE_POINTS,
           TestHost::PRIMITIVE_LINES,
           TestHost::PRIMITIVE_LINE_LOOP,
           TestHost::PRIMITIVE_LINE_STRIP,
           TestHost::PRIMITIVE_TRIANGLES,
           TestHost::PRIMITIVE_TRIANGLE_STRIP,
           TestHost::PRIMITIVE_TRIANGLE_FAN,
           TestHost::PRIMITIVE_QUADS,
           TestHost::PRIMITIVE_QUAD_STRIP,
           TestHost::PRIMITIVE_POLYGON,
       }) {
    for (auto use_vsh : {false, true}) {
      std::string name = MakeTestName(kTestName, primitive, use_vsh);
      tests_[name] = [this, name, primitive, use_vsh]() { Test(name, primitive, use_vsh); };
    }
  }
}

/**
 * Initializes the test suite and creates test cases.
 *
 * @tc PrimitiveType-Points
 * @tc PrimitiveType-Lines
 * @tc PrimitiveType-LLoop
 * @tc PrimitiveType-LStrip
 * @tc PrimitiveType-Tris
 * @tc PrimitiveType-TriStrip
 * @tc PrimitiveType-TriFan
 * @tc PrimitiveType-Quads
 * @tc PrimitiveType-QuadStrip
 * @tc PrimitiveType-Poly
 */
void PrimitiveTypeTests::Initialize() { TestSuite::Initialize(); }

static void SetVertexColor(PBKitPlusPlus::Vertex &vertex, uint32_t index) {
  // Use a golden ratio offset (~0.618) to maximize hue difference between adjacent indices
  // Multiplying by 360 gives us a hue in degrees
  float hue = fmodf(index * 222.5f, 360.0f);

  // Simple HSV to RGB conversion (S=1.0, V=1.0)
  float c = 1.0f;
  float x = c * (1.0f - fabsf(fmodf(hue / 60.0f, 2.0f) - 1.0f));
  float r = 0, g = 0, b = 0;

  if (hue < 60) {
    r = c;
    g = x;
    b = 0;
  } else if (hue < 120) {
    r = x;
    g = c;
    b = 0;
  } else if (hue < 180) {
    r = 0;
    g = c;
    b = x;
  } else if (hue < 240) {
    r = 0;
    g = x;
    b = c;
  } else if (hue < 300) {
    r = x;
    g = 0;
    b = c;
  } else {
    r = c;
    g = 0;
    b = x;
  }

  vertex.SetDiffuse(r, g, b);
}

static void CreateGeometry(TestHost &host_, TestHost::DrawPrimitive primitive) {
  static constexpr float kZ = 1.f;
  static constexpr float kW = 1.f;

  const float screen_w = host_.GetFramebufferWidthF();
  const float screen_h = host_.GetFramebufferHeightF();
  const float center_x = screen_w * 0.5f;
  const float center_y = screen_h * 0.5f;

  const float span_x = screen_w * 0.6f;
  const float span_y = screen_h * 0.6f;
  const float left = center_x - (span_x * 0.5f);
  const float top = center_y - (span_y * 0.5f);

  std::shared_ptr<PBKitPlusPlus::VertexBuffer> vbuf;

  uint32_t vertex_index = 0;
  auto setvtx = [&vertex_index](PBKitPlusPlus::Vertex *&vtx, float x, float y) {
    vtx->SetPosition(x, y, kZ, kW);
    SetVertexColor(*vtx, vertex_index++);
    ++vtx;
  };

  switch (primitive) {
    case TestHost::PRIMITIVE_POINTS: {
      vbuf = host_.AllocateVertexBuffer(kNumPrimitives);
      auto vertex = vbuf->Lock();

      const float aspect = span_x / span_y;
      const uint32_t cols = static_cast<uint32_t>(fmaxf(1.f, roundf(sqrtf(kNumPrimitives * aspect))));
      const uint32_t rows = (kNumPrimitives + cols - 1) / cols;

      for (uint32_t i = 0; i < kNumPrimitives; ++i) {
        uint32_t col = i % cols;
        uint32_t row = i / cols;

        float x = left + (span_x * col / (cols - 1));
        float y = top + (span_y * row / (rows - 1));

        setvtx(vertex, x, y);
      }
      vbuf->Unlock();
    } break;

    case TestHost::PRIMITIVE_LINES: {
      vbuf = host_.AllocateVertexBuffer(2 * kNumPrimitives);
      auto vertex = vbuf->Lock();
      for (uint32_t i = 0; i < kNumPrimitives; ++i) {
        float x = left + (span_x * i / (kNumPrimitives - 1));
        setvtx(vertex, x, top);
        setvtx(vertex, x, top + span_y);
      }
      vbuf->Unlock();
    } break;

    case TestHost::PRIMITIVE_LINE_LOOP:
    case TestHost::PRIMITIVE_LINE_STRIP: {
      // Both require kNumPrimitives + 1 vertices for a continuous chain
      vbuf = host_.AllocateVertexBuffer(kNumPrimitives + 1);
      auto vertex = vbuf->Lock();
      for (uint32_t i = 0; i <= kNumPrimitives; ++i) {
        float x = left + (span_x * i / kNumPrimitives);
        float y = top + (i % 2 == 0 ? 0 : span_y);
        setvtx(vertex, x, y);
      }
      vbuf->Unlock();
    } break;

    case TestHost::PRIMITIVE_TRIANGLES: {
      vbuf = host_.AllocateVertexBuffer(kNumPrimitives * 3);
      auto vertex = vbuf->Lock();
      for (uint32_t i = 0; i < kNumPrimitives; ++i) {
        float x_off = left + (span_x * i / kNumPrimitives);
        setvtx(vertex, x_off, top + span_y);
        setvtx(vertex, x_off + (span_x / kNumPrimitives / 2), top);
        setvtx(vertex, x_off + (span_x / kNumPrimitives), top + span_y);
      }
      vbuf->Unlock();
    } break;

    case TestHost::PRIMITIVE_TRIANGLE_STRIP: {
      vbuf = host_.AllocateVertexBuffer(kNumPrimitives + 2);
      auto vertex = vbuf->Lock();
      for (uint32_t i = 0; i < kNumPrimitives + 2; ++i) {
        float x = left + (span_x * (i / 2) / (kNumPrimitives / 2.0f));
        float y = (i % 2 == 0) ? top + span_y : top;
        setvtx(vertex, x, y);
      }
      vbuf->Unlock();
    } break;

    case TestHost::PRIMITIVE_TRIANGLE_FAN: {
      vbuf = host_.AllocateVertexBuffer(kNumPrimitives + 2);
      auto vertex = vbuf->Lock();
      setvtx(vertex, center_x, center_y);  // Center hub

      for (uint32_t i = 0; i <= kNumPrimitives; ++i) {
        float angle = (2.f * M_PI * i) / kNumPrimitives;
        setvtx(vertex, center_x + cosf(angle) * (span_x * 0.5f), center_y + sinf(angle) * (span_y * 0.5f));
      }
      vbuf->Unlock();
    } break;

    case TestHost::PRIMITIVE_QUADS: {
      vbuf = host_.AllocateVertexBuffer(kNumPrimitives * 4);
      auto vertex = vbuf->Lock();
      for (uint32_t i = 0; i < kNumPrimitives; ++i) {
        float x_start = left + (span_x * i / kNumPrimitives);
        float x_end = x_start + (span_x / kNumPrimitives) * 0.8f;
        setvtx(vertex, x_start, top);
        setvtx(vertex, x_end, top);
        setvtx(vertex, x_end, top + span_y);
        setvtx(vertex, x_start, top + span_y);
      }
      vbuf->Unlock();
    } break;

    case TestHost::PRIMITIVE_QUAD_STRIP: {
      vbuf = host_.AllocateVertexBuffer(2 * kNumPrimitives + 2);
      auto vertex = vbuf->Lock();
      for (uint32_t i = 0; i <= kNumPrimitives; ++i) {
        float x = left + (span_x * i / kNumPrimitives);
        setvtx(vertex, x, top + span_y);
        setvtx(vertex, x, top);
      }
      vbuf->Unlock();
    } break;

    case TestHost::PRIMITIVE_POLYGON: {
      vbuf = host_.AllocateVertexBuffer(kNumPrimitives);
      auto vertex = vbuf->Lock();
      for (uint32_t i = 0; i < kNumPrimitives; ++i) {
        float angle = (2.f * M_PI * i) / kNumPrimitives;
        setvtx(vertex, center_x + cosf(angle) * (span_x * 0.5f), center_y + sinf(angle) * (span_y * 0.5f));
      }
      vbuf->Unlock();
    } break;
  }
}

void PrimitiveTypeTests::Test(const std::string &name, TestHost::DrawPrimitive primitive, bool use_vsh) {
  host_.PrepareDraw(0xFF222222);

  if (use_vsh) {
    auto shader = std::make_shared<PBKitPlusPlus::PassthroughVertexShader>();
    host_.SetVertexShaderProgram(shader);
  } else {
    host_.SetVertexShaderProgram(nullptr);
    host_.SetupFixedFunctionPassthrough();
  }

  TestHost::ProfileResults results{};

  CreateGeometry(host_, primitive);

  results = Profile(kTestName, kIterations, [this, primitive] { host_.DrawInlineArray(kVertexAttributes, primitive); });

  host_.ClearVertexBuffer();

  host_.FinishDraw(suite_name_, name, results);
}
