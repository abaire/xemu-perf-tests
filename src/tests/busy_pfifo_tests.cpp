#include "busy_pfifo_tests.h"

#include "test_host.h"

static constexpr char kTestName[] = "PFIFOSaturation";
static constexpr uint32_t kNumBloatCommandsPerDraw = 1900;
static constexpr uint32_t kNumDrawsSingleFrame = 25;
static constexpr uint32_t kNumDrawsMultiFrame = 7;

BusyPfifoTests::BusyPfifoTests(TestHost &host, std::string output_dir, const Config &config)
    : TestSuite(host, std::move(output_dir), "BusyPfifo", config) {
  tests_[kTestName] = [this]() { Test(); };
}

/**
 * Initializes the test suite and creates test cases.
 *
 * @tc PFIFOSaturation
 *  Saturates the PFIFO queue and renders simple draws.
 */
void BusyPfifoTests::Initialize() {
  TestSuite::Initialize();

  host_.SetFinalCombiner0Just(PBKitPlusPlus::NV2AState::SRC_DIFFUSE);
  host_.SetFinalCombiner1Just(PBKitPlusPlus::NV2AState::SRC_DIFFUSE, true);
}

static void FillPFIFO() {
  PBKitPlusPlus::Pushbuffer::Begin();
  for (auto i = 0; i < kNumBloatCommandsPerDraw; ++i) {
    PBKitPlusPlus::Pushbuffer::Push(NV097_SET_TEXGEN_Q, 0);
    PBKitPlusPlus::Pushbuffer::Push(NV097_SET_TEXGEN_R, 0);
    PBKitPlusPlus::Pushbuffer::Push(NV097_SET_TEXGEN_S, 0);
    PBKitPlusPlus::Pushbuffer::Push(NV097_SET_TEXGEN_T, 0);
    PBKitPlusPlus::Pushbuffer::Push(NV097_SET_ALPHA_REF, 0);
    PBKitPlusPlus::Pushbuffer::Push(NV097_SET_COLOR_MATERIAL, 0);
  }
  PBKitPlusPlus::Pushbuffer::End();
}

static void SetVertexColor(TestHost &host, uint32_t index) {
  float hue = fmodf(index * 222.5f, 360.0f);

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

  host.SetDiffuse(r, g, b);
}

void BusyPfifoTests::Test() {
  host_.SetVertexShaderProgram(nullptr);
  host_.SetupFixedFunctionPassthrough();

  host_.PrepareDraw(0xFF222222);

  TestHost::ProfileResults results{};

  const uint32_t num_draws = host_.GetSaveResults() ? kNumDrawsSingleFrame : kNumDrawsMultiFrame;
  results = Profile(kTestName, 10, [this, num_draws] {
    static constexpr float kZ = 1.f;
    static constexpr float kW = 1.f;
    static constexpr uint32_t kNumPrimitives = 10;

    const float screen_w = host_.GetFramebufferWidthF();
    const float screen_h = host_.GetFramebufferHeightF();
    const float center_x = screen_w * 0.5f;
    const float center_y = screen_h * 0.5f;

    const float span_x = screen_w * 0.6f;
    const float span_y = screen_h * 0.6f;
    const float left = center_x - (span_x * 0.5f);
    const float top = center_y - (span_y * 0.5f);

    for (auto draw = 0; draw < num_draws; ++draw) {
      FillPFIFO();

      host_.Begin(TestHost::PRIMITIVE_TRIANGLE_STRIP);
      for (uint32_t i = 0; i < kNumPrimitives + 2; ++i) {
        float x = left + (span_x * (i / 2) / (kNumPrimitives / 2.0f));
        float y = (i % 2 == 0) ? top + span_y : top;
        SetVertexColor(host_, i);
        host_.SetVertex(x, y, kZ, kW);
      }
      host_.End();
    }
  });

  host_.FinishDraw(suite_name_, kTestName, results);
}
