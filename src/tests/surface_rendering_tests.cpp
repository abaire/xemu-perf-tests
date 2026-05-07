#include "surface_rendering_tests.h"

#include <texture_generator.h>

#include "test_host.h"

static constexpr char kTestName[] = "SurfaceRendering";
static constexpr uint32_t kIterations = 10;
static constexpr uint32_t kNumDrawsSingleFrame = 80;
static constexpr uint32_t kNumDrawsMultiframe60FPS = 40;

static constexpr uint32_t kTextureWidth = 128;
static constexpr uint32_t kTextureHeight = 128;

SurfaceRenderingTests::SurfaceRenderingTests(TestHost &host, std::string output_dir, const Config &config)
    : TestSuite(host, std::move(output_dir), "SurfaceRendering", config) {
  tests_[kTestName] = [this]() { Test(); };
}

/**
 * Initializes the test suite and creates test cases.
 *
 * @tc SurfaceRendering
 *  Renders to various offscreen buffers before blitting to the screen.
 */
void SurfaceRenderingTests::Initialize() {
  TestSuite::Initialize();

  PBKitPlusPlus::GenerateSwizzledRGBRadialGradient(host_.GetTextureMemoryForStage(2), kTextureWidth, kTextureHeight);
  PBKitPlusPlus::GenerateSwizzledRGBMaxContrastNoisePattern(host_.GetTextureMemoryForStage(3), kTextureWidth,
                                                            kTextureHeight);

  host_.SetBlend(false);
  host_.SetVertexShaderProgram(nullptr);

  // Zeta writes are disabled to allow the zeta buffer to be assigned to arbitrary memory in order to avoid a GPU
  // exception when setting the offscreen render target with swizzling enabled.
  PBKitPlusPlus::Pushbuffer::Begin();
  PBKitPlusPlus::Pushbuffer::Push(NV097_SET_DEPTH_MASK, false);
  PBKitPlusPlus::Pushbuffer::Push(NV097_SET_STENCIL_MASK, false);
  PBKitPlusPlus::Pushbuffer::End();

  host_.SetFinalCombiner1Just(PBKitPlusPlus::NV2AState::SRC_ZERO, true, true);
}

static void DrawBiTri(TestHost &host, float left, float top, float span_x, float span_y,
                      PBKitPlusPlus::NV2AState::CombinerSource tex_top,
                      PBKitPlusPlus::NV2AState::CombinerSource tex_bottom) {
  static constexpr float kZ = 1.f;

  host.SetFinalCombiner0Just(tex_top);
  host.Begin(PBKitPlusPlus::NV2AState::PRIMITIVE_TRIANGLES);
  host.SetTexCoord0(0.f, 0.f);
  host.SetTexCoord2(0.f, 0.f);
  host.SetVertex(left, top, kZ);
  host.SetTexCoord0(1.f, 0.f);
  host.SetTexCoord2(1.f, 0.f);
  host.SetVertex(left + span_x, top, kZ);
  host.SetTexCoord0(0.f, 1.f);
  host.SetTexCoord2(0.f, 1.f);
  host.SetVertex(left, top + span_y, kZ);
  host.End();

  host.SetFinalCombiner0Just(tex_bottom);
  host.Begin(PBKitPlusPlus::NV2AState::PRIMITIVE_TRIANGLES);
  host.SetTexCoord1(0.f, 1.f);
  host.SetTexCoord3(0.f, 1.f);
  host.SetVertex(left, top + span_y, kZ);
  host.SetTexCoord1(1.f, 0.f);
  host.SetTexCoord3(1.f, 0.f);
  host.SetVertex(left + span_x, top, kZ);
  host.SetTexCoord1(1.f, 1.f);
  host.SetTexCoord3(1.f, 1.f);
  host.SetVertex(left + span_x, top + span_y, kZ);
  host.End();
}

void SurfaceRenderingTests::Test() {
  host_.SetupFixedFunctionPassthrough();
  host_.PrepareDraw(0xFF444444);

  TestHost::ProfileResults results{};

  auto fill_surface = [this]() {
    host_.SetTextureStageEnabled(0, false);
    host_.SetTextureStageEnabled(1, false);
    auto format = PBKitPlusPlus::GetTextureFormatInfo(NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8B8G8R8);
    for (auto i = 2; i < 4; ++i) {
      auto &texture_stage = host_.GetTextureStage(i);
      texture_stage.SetFormat(format);
      texture_stage.SetTextureDimensions(kTextureWidth, kTextureHeight);
      texture_stage.SetEnabled(true);
    }
    host_.SetupTextureStages();
    host_.SetShaderStageProgram(TestHost::STAGE_NONE, TestHost::STAGE_NONE, TestHost::STAGE_2D_PROJECTIVE,
                                TestHost::STAGE_2D_PROJECTIVE);

    DrawBiTri(host_, 0.f, 0.f, kTextureWidth, kTextureHeight, PBKitPlusPlus::NV2AState::SRC_TEX2,
              PBKitPlusPlus::NV2AState::SRC_TEX3);

    host_.SetTextureStageEnabled(2, false);
    host_.SetTextureStageEnabled(3, false);
    host_.SetupTextureStages();
    host_.SetShaderStageProgram(TestHost::STAGE_NONE);
  };

  const float screen_w = host_.GetFramebufferWidthF();
  const float screen_h = host_.GetFramebufferHeightF();
  const float center_x = screen_w * 0.5f;
  const float center_y = screen_h * 0.5f;

  const float span_x = screen_w * 0.25f;
  const float span_y = screen_h * 0.25f;
  const float left = center_x - (span_x * 0.5f);
  const float top = center_y - (span_y * 0.5f);

  const uint32_t num_draws = host_.GetSaveResults() ? kNumDrawsSingleFrame : kNumDrawsMultiframe60FPS;
  results = Profile(kTestName, kIterations, [this, num_draws, &fill_surface, left, top, span_x, span_y] {
    for (auto i = 0; i < num_draws; ++i) {
      host_.RenderToSurfaceStart(host_.GetTextureMemoryForStage(0), PBKitPlusPlus::NV2AState::SCF_A8R8G8B8,
                                 host_.GetTextureMemoryForStage(1), PBKitPlusPlus::NV2AState::SZF_Z24S8, kTextureWidth,
                                 kTextureHeight, true);
      fill_surface();
      host_.RenderToSurfaceEnd();

      host_.RenderToSurfaceStart(host_.GetTextureMemoryForStage(1), PBKitPlusPlus::NV2AState::SCF_R5G6B5,
                                 host_.GetTextureMemoryForStage(0), PBKitPlusPlus::NV2AState::SZF_Z16, kTextureWidth,
                                 kTextureHeight, true);
      fill_surface();
      host_.RenderToSurfaceEnd();

      {
        auto &texture_stage = host_.GetTextureStage(0);
        texture_stage.SetFormat(PBKitPlusPlus::GetTextureFormatInfo(NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8R8G8B8));
        texture_stage.SetTextureDimensions(kTextureWidth, kTextureHeight);
        texture_stage.SetEnabled(true);
      }
      {
        auto &texture_stage = host_.GetTextureStage(1);
        texture_stage.SetFormat(PBKitPlusPlus::GetTextureFormatInfo(NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R5G6B5));
        texture_stage.SetTextureDimensions(kTextureWidth, kTextureHeight);
        texture_stage.SetEnabled(true);
      }
      host_.SetupTextureStages();
      host_.SetShaderStageProgram(TestHost::STAGE_2D_PROJECTIVE, TestHost::STAGE_2D_PROJECTIVE);

      DrawBiTri(host_, left, top, span_x, span_y, PBKitPlusPlus::NV2AState::SRC_TEX0,
                PBKitPlusPlus::NV2AState::SRC_TEX1);

      host_.SetTextureStageEnabled(0, false);
      host_.SetTextureStageEnabled(1, false);
      host_.SetupTextureStages();
      host_.SetShaderStageProgram(TestHost::STAGE_NONE);
    }
  });

  host_.FinishDraw(suite_name_, kTestName, results);
}
