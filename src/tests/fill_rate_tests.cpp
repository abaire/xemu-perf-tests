#include "fill_rate_tests.h"

#include <SDL.h>
#include <pbkit/pbkit.h>

#include "test_host.h"
#include "texture_format.h"
#include "texture_generator.h"

static constexpr uint32_t kIterations = 10;
static constexpr uint32_t kNumDraws = 500;
static uint32_t kVertexAttributes = TestHost::POSITION | TestHost::DIFFUSE | TestHost::TEXCOORD0 | TestHost::TEXCOORD1 |
                                    TestHost::TEXCOORD2 | TestHost::TEXCOORD3;
static constexpr auto kTextureWidth = 128;
static constexpr auto kTextureHeight = 128;

FillRateTests::FillRateTests(TestHost &host, std::string output_dir, const Config &config)
    : TestSuite(host, std::move(output_dir), "FillRate", config) {
  tests_["FillRate-Solid"] = [this]() { TestFillRate(false); };
  tests_["FillRate-Textured"] = [this]() { TestFillRate(true); };
}

void FillRateTests::Initialize() {
  TestSuite::Initialize();

  host_.SetFinalCombiner0Just(TestHost::SRC_DIFFUSE);
  host_.SetFinalCombiner1Just(TestHost::SRC_ZERO, true, true);

  PBKitPlusPlus::GenerateSwizzledRGBTestPattern(host_.GetTextureMemoryForStage(0), kTextureWidth, kTextureHeight);
  PBKitPlusPlus::GenerateRGBRadialATestPattern(host_.GetTextureMemoryForStage(1), kTextureWidth, kTextureHeight);
  PBKitPlusPlus::GenerateSwizzledRGBRadialGradient(host_.GetTextureMemoryForStage(2), kTextureWidth, kTextureHeight);
  PBKitPlusPlus::GenerateSwizzledRGBMaxContrastNoisePattern(host_.GetTextureMemoryForStage(3), kTextureWidth,
                                                            kTextureHeight);

  vertex_buffer_ = host_.AllocateVertexBuffer(4);
  auto vertex = vertex_buffer_->Lock();

  float width = host_.GetFramebufferWidthF();
  float height = host_.GetFramebufferHeightF();

  vertex->SetPosition(0.0f, 0.0f, 1.0f);
  vertex->SetDiffuse(1.0f, 1.0f, 1.0f);
  vertex->SetTexCoord0(0.0f, 0.0f);
  vertex->SetTexCoord1(0.0f, 0.0f);
  vertex->SetTexCoord2(0.0f, 0.0f);
  vertex->SetTexCoord3(0.0f, 0.0f);
  ++vertex;

  vertex->SetPosition(width, 0.0f, 1.0f);
  vertex->SetDiffuse(0.0f, 1.0f, 1.0f);
  vertex->SetTexCoord0(1.0f, 0.0f);
  vertex->SetTexCoord1(1.0f, 0.0f);
  vertex->SetTexCoord2(1.0f, 0.0f);
  vertex->SetTexCoord3(1.0f, 0.0f);
  ++vertex;

  vertex->SetPosition(0.0f, height, 1.0f);
  vertex->SetDiffuse(1.0f, 0.0f, 1.0f);
  vertex->SetTexCoord0(0.0f, 1.0f);
  vertex->SetTexCoord1(0.0f, 1.0f);
  vertex->SetTexCoord2(0.0f, 1.0f);
  vertex->SetTexCoord3(0.0f, 1.0f);
  ++vertex;

  vertex->SetPosition(width, height, 1.0f);
  vertex->SetDiffuse(1.0f, 1.0f, 0.0f);
  vertex->SetTexCoord0(1.0f, 1.0f);
  vertex->SetTexCoord1(1.0f, 1.0f);
  vertex->SetTexCoord2(1.0f, 1.0f);
  vertex->SetTexCoord3(1.0f, 1.0f);
  ++vertex;

  vertex_buffer_->Unlock();
}

void FillRateTests::Deinitialize() {
  host_.ClearVertexBuffer();
  TestSuite::Deinitialize();
}

void FillRateTests::TestFillRate(bool use_texture) {
  host_.PrepareDraw(0xFF222222);

  host_.SetVertexShaderProgram(nullptr);
  host_.SetupFixedFunctionPassthrough();

  if (use_texture) {
    for (auto i = 0; i < 4; ++i) {
      auto &texture_stage = host_.GetTextureStage(i);
      texture_stage.SetFormat(PBKitPlusPlus::GetTextureFormatInfo(NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8B8G8R8));
      texture_stage.SetTextureDimensions(kTextureWidth, kTextureHeight);
      texture_stage.SetUWrap(PBKitPlusPlus::TextureStage::WRAP_REPEAT, false);
      texture_stage.SetVWrap(PBKitPlusPlus::TextureStage::WRAP_MIRROR, false);
      texture_stage.SetEnabled(true);
    }
    host_.SetupTextureStages();

    host_.SetCombinerControl(3, true);
    host_.SetCombinerFactorC0(0, 0.25f, 0.25f, 0.25f, 0.25f);
    host_.SetInputColorCombiner(0, PBKitPlusPlus::NV2AState::SRC_TEX0, false, PBKitPlusPlus::NV2AState::MAP_UNSIGNED_IDENTITY,
      PBKitPlusPlus::NV2AState::SRC_C0, false, PBKitPlusPlus::NV2AState::MAP_UNSIGNED_IDENTITY,
      PBKitPlusPlus::NV2AState::SRC_TEX1, false, PBKitPlusPlus::NV2AState::MAP_UNSIGNED_IDENTITY,
      PBKitPlusPlus::NV2AState::SRC_C0);
    host_.SetOutputColorCombiner(0, PBKitPlusPlus::NV2AState::DST_DISCARD, PBKitPlusPlus::NV2AState::DST_DISCARD, PBKitPlusPlus::NV2AState::DST_R0);
    host_.SetInputColorCombiner(1, PBKitPlusPlus::NV2AState::SRC_TEX2, false, PBKitPlusPlus::NV2AState::MAP_UNSIGNED_IDENTITY,
      PBKitPlusPlus::NV2AState::SRC_C0, false, PBKitPlusPlus::NV2AState::MAP_UNSIGNED_IDENTITY,
      PBKitPlusPlus::NV2AState::SRC_TEX3, false, PBKitPlusPlus::NV2AState::MAP_UNSIGNED_IDENTITY,
      PBKitPlusPlus::NV2AState::SRC_C0);
    host_.SetOutputColorCombiner(1, PBKitPlusPlus::NV2AState::DST_DISCARD, PBKitPlusPlus::NV2AState::DST_DISCARD, PBKitPlusPlus::NV2AState::DST_R1);

    host_.SetInputColorCombiner(2, PBKitPlusPlus::NV2AState::SRC_R0, false, PBKitPlusPlus::NV2AState::MAP_UNSIGNED_IDENTITY,
      PBKitPlusPlus::NV2AState::SRC_ZERO, false, PBKitPlusPlus::NV2AState::MAP_UNSIGNED_INVERT,
      PBKitPlusPlus::NV2AState::SRC_R1, false, PBKitPlusPlus::NV2AState::MAP_UNSIGNED_IDENTITY,
      PBKitPlusPlus::NV2AState::SRC_ZERO, false, PBKitPlusPlus::NV2AState::MAP_UNSIGNED_INVERT);
    host_.SetOutputColorCombiner(2, PBKitPlusPlus::NV2AState::DST_DISCARD, PBKitPlusPlus::NV2AState::DST_DISCARD, PBKitPlusPlus::NV2AState::DST_R0);

    host_.SetFinalCombiner0Just(TestHost::SRC_R0);

    host_.SetShaderStageProgram(TestHost::STAGE_2D_PROJECTIVE, TestHost::STAGE_2D_PROJECTIVE,
                                TestHost::STAGE_2D_PROJECTIVE, TestHost::STAGE_2D_PROJECTIVE);

  } else {
    host_.SetFinalCombiner0Just(TestHost::SRC_DIFFUSE);
  }

  TestHost::ProfileResults results{};

  std::string test_name = use_texture ? "FillRate-Textured" : "FillRate-Solid";

  results = Profile(test_name, kIterations, [this] {
    for (auto i = 0; i < kNumDraws; ++i) {
      host_.DrawArrays(kVertexAttributes, TestHost::PRIMITIVE_TRIANGLE_STRIP);
    }
  });

  host_.SetShaderStageProgram(TestHost::STAGE_NONE);
  host_.SetTextureStageEnabled(0, false);
  host_.SetTextureStageEnabled(1, false);
  host_.SetTextureStageEnabled(2, false);
  host_.SetTextureStageEnabled(3, false);

  host_.FinishDraw(suite_name_, test_name, results);
  host_.SetCombinerControl();
}
