#include "uniform_thrash_tests.h"

#include <shaders/vertex_shader_program.h>

#include "test_host.h"

static constexpr char kTestName[] = "UniformThrash";

static constexpr uint32_t kIterations = 10;
static constexpr uint32_t kNumDrawsSingleFrame = 100;
static constexpr uint32_t kNumDrawsMultiFrame = 830;

// clang-format off
static const uint32_t kShader[] = {
#include "diffuse_from_uniform.vshinc"
};
// clang-format on

UniformThrashTests::UniformThrashTests(TestHost &host, std::string output_dir, const Config &config)
    : TestSuite(host, std::move(output_dir), "UniformThrash", config) {
  tests_[kTestName] = [this]() { Test(); };
}

/**
 * Initializes the test suite and creates test cases.
 *
 * @tc UniformThrash
 *  Renders a moderate number of quads, changing the uniform constants between each quad.
 */
void UniformThrashTests::Initialize() { TestSuite::Initialize(); }

static void SetVertexColor(vector_t &ret, uint32_t index) {
  // Use a golden ratio offset (~0.618) to maximize hue difference between adjacent indices
  // Multiplying by 360 gives us a hue in degrees
  float hue = fmodf(index * 222.5f, 360.0f);

  // Simple HSV to RGB conversion (S=1.0, V=1.0)
  float c = 1.0f;
  float x = c * (1.0f - fabsf(fmodf(hue / 60.0f, 2.0f) - 1.0f));
  ret[3] = 1.f;

  if (hue < 60) {
    ret[0] = c;
    ret[1] = x;
    ret[2] = 0;
  } else if (hue < 120) {
    ret[0] = x;
    ret[1] = c;
    ret[2] = 0;
  } else if (hue < 180) {
    ret[0] = 0;
    ret[1] = c;
    ret[2] = x;
  } else if (hue < 240) {
    ret[0] = 0;
    ret[1] = x;
    ret[2] = c;
  } else if (hue < 300) {
    ret[0] = x;
    ret[1] = 0;
    ret[2] = c;
  } else {
    ret[0] = c;
    ret[1] = 0;
    ret[2] = x;
  }
}

void UniformThrashTests::Test() {
  host_.PrepareDraw();

  auto shader = std::make_shared<PBKitPlusPlus::VertexShaderProgram>();
  shader->SetShader(kShader, sizeof(kShader));
  host_.SetVertexShaderProgram(shader);

  TestHost::ProfileResults results{};

  static constexpr float kTopMargin = 96.f;
  const float kQuadWidth = ceilf(host_.GetFramebufferWidthF() / 10.f);
  const uint32_t num_draws = host_.GetSaveResults() ? kNumDrawsSingleFrame : kNumDrawsMultiFrame;
  const float kQuadHeight = ceilf(host_.GetFramebufferHeightF() - kTopMargin) / (static_cast<float>(num_draws) / 10.f);

  static constexpr float kZ = 1.f;

  results = Profile(kTestName, kIterations, [this, shader, kQuadWidth, kQuadHeight, num_draws] {
    XboxMath::vector_t diffuse;
    for (auto i = 0; i < num_draws; ++i) {
      SetVertexColor(diffuse, i);
      shader->SetUniform4F(96 - PBKitPlusPlus::VertexShaderProgram::kShaderUserConstantOffset, diffuse);
      shader->PrepareDraw();

      host_.Begin(TestHost::PRIMITIVE_QUADS);
      float left = static_cast<float>(i % 10) * kQuadWidth;
      float top = kTopMargin + floorf(static_cast<float>(i) / 10.f) * kQuadHeight;
      host_.SetVertex(left, top, kZ);
      host_.SetVertex(left + kQuadWidth, top, kZ);
      host_.SetVertex(left + kQuadWidth, top + kQuadHeight, kZ);
      host_.SetVertex(left, top + kQuadHeight, kZ);
      host_.End();
    }
  });

  host_.FinishDraw(suite_name_, kTestName, results);

  host_.SetVertexShaderProgram(nullptr);
}
