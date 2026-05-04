#include "tiny_draw_tests.h"

#include "shaders/passthrough_vertex_shader.h"
#include "test_host.h"

static constexpr char kTinyDrawTest[] = "TinyDraw";

static constexpr uint32_t kIterations = 10;
static constexpr uint32_t kNumDrawsSingleFrame = 1000;

// Measured using a 1.0 devkit, close to the 60 fps limit.
static constexpr uint32_t GetNumDrawsForMode(TinyDrawTests::DrawMode mode) {
  switch (mode) {
    case TinyDrawTests::DrawMode::DRAW_ARRAYS:
      return 380;
    case TinyDrawTests::DrawMode::DRAW_INLINE_BUFFERS:
      return 420;
    case TinyDrawTests::DrawMode::DRAW_INLINE_ARRAYS:
      return 290;
    case TinyDrawTests::DrawMode::DRAW_INLINE_ELEMENTS:
      return 370;
  }
  return 0;
};

static uint32_t kVertexAttributes = TestHost::POSITION | TestHost::DIFFUSE;
static TestHost::DrawPrimitive kPrimitive = TestHost::PRIMITIVE_TRIANGLES;

static std::string MakeTestName(const std::string &prefix, TinyDrawTests::DrawMode draw_mode, bool use_vsh) {
  std::string ret = prefix;

  switch (draw_mode) {
    case TinyDrawTests::DrawMode::DRAW_ARRAYS:
      ret += "-arrays";
      break;
    case TinyDrawTests::DrawMode::DRAW_INLINE_BUFFERS:
      ret += "-inlinebuffers";
      break;
    case TinyDrawTests::DrawMode::DRAW_INLINE_ARRAYS:
      ret += "-inlinearrays";
      break;
    case TinyDrawTests::DrawMode::DRAW_INLINE_ELEMENTS:
      ret += "-inlineelements";
      break;
  }

  if (use_vsh) {
    ret += "-vsh";
  }

  return ret;
}

TinyDrawTests::TinyDrawTests(TestHost &host, std::string output_dir, const Config &config)
    : TestSuite(host, std::move(output_dir), "TinyDraw", config) {
  for (auto draw_mode : {DrawMode::DRAW_ARRAYS, DrawMode::DRAW_INLINE_BUFFERS, DrawMode::DRAW_INLINE_ARRAYS,
                         DrawMode::DRAW_INLINE_ELEMENTS}) {
    for (auto use_vsh : {false, true}) {
      std::string name = MakeTestName(kTinyDrawTest, draw_mode, use_vsh);
      tests_[name] = [this, name, draw_mode, use_vsh]() { Test(name, draw_mode, use_vsh); };
    }
  }
}

/**
 * Initializes the test suite and creates test cases.
 *
 * @tc TinyDraw-arrays
 *   Renders hundreds of tiny triangles using individual DRAW_ARRAYS calls. Fixed function pipeline.
 *
 * @tc TinyDraw-arrays-vsh
 *   Renders hundreds of tiny triangles using individual DRAW_ARRAYS calls. Custom vertex shader.
 *
 * @tc TinyDraw-inlinebuffers
 *   Renders hundreds of tiny triangles using individual DRAW_INLINE_BUFFERS calls. Fixed function pipeline.
 *
 * @tc TinyDraw-inlinebuffers-vsh
 *   Renders hundreds of tiny triangles using individual DRAW_INLINE_BUFFERS calls. Custom vertex shader.
 *
 * @tc TinyDraw-inlinearrays
 *   Renders hundreds of tiny triangles using individual DRAW_INLINE_ARRAYS calls. Fixed function pipeline.
 *
 * @tc TinyDraw-inlinearrays-vsh
 *   Renders hundreds of tiny triangles using individual DRAW_INLINE_ARRAYS calls. Custom vertex shader.
 *
 * @tc TinyDraw-inlineelements
 *   Renders hundreds of tiny triangles using individual DRAW_INLINE_ELEMENTS calls. Fixed function pipeline.
 *
 * @tc TinyDraw-inlineelements-vsh
 *   Renders hundreds of tiny triangles using individual DRAW_INLINE_ELEMENTS calls. Custom vertex shader.
 *
 */
void TinyDrawTests::Initialize() {
  TestSuite::Initialize();

  host_.SetFinalCombiner0Just(TestHost::SRC_DIFFUSE);
  host_.SetFinalCombiner1Just(TestHost::SRC_ZERO, true, true);

  vertex_buffer_ = host_.AllocateVertexBuffer(3);
  auto vertex = vertex_buffer_->Lock();

  vertex->SetPosition(300.f, 200.f, 1.f);
  vertex->SetDiffuse(1.f, 0.f, 0.f);
  index_buffer_.emplace_back(0);
  ++vertex;

  vertex->SetPosition(304.f, 200.f, 1.f);
  vertex->SetDiffuse(0.f, 1.f, 0.f);
  index_buffer_.emplace_back(1);
  ++vertex;

  vertex->SetPosition(302.f, 204.f, 1.f);
  vertex->SetDiffuse(0.f, 0.f, 1.f);
  index_buffer_.emplace_back(2);
  ++vertex;

  vertex_buffer_->Unlock();
}

void TinyDrawTests::Deinitialize() {
  host_.ClearVertexBuffer();
  TestSuite::Deinitialize();
}

void TinyDrawTests::Test(const std::string &test_name, DrawMode draw_mode, bool use_vsh) {
  host_.PrepareDraw(0xFF333333);

  if (use_vsh) {
    auto shader = std::make_shared<PBKitPlusPlus::PassthroughVertexShader>();
    host_.SetVertexShaderProgram(shader);
  } else {
    host_.SetVertexShaderProgram(nullptr);
    host_.SetupFixedFunctionPassthrough();
  }

  TestHost::ProfileResults results{};
  const uint32_t num_draws = host_.GetSaveResults() ? kNumDrawsSingleFrame : GetNumDrawsForMode(draw_mode);
  switch (draw_mode) {
    case DrawMode::DRAW_ARRAYS:
      results = Profile(test_name, kIterations, [this, num_draws] {
        for (auto i = 0; i < num_draws; ++i) {
          host_.DrawArrays(kVertexAttributes, kPrimitive);
        }
      });
      break;

    case DrawMode::DRAW_INLINE_BUFFERS:
      results = Profile(test_name, kIterations, [this, num_draws] {
        for (auto i = 0; i < num_draws; ++i) {
          host_.DrawInlineBuffer(kVertexAttributes, kPrimitive);
        }
      });
      break;

    case DrawMode::DRAW_INLINE_ELEMENTS:
      results = Profile(test_name, kIterations, [this, num_draws] {
        for (auto i = 0; i < num_draws; ++i) {
          host_.DrawInlineElements16(index_buffer_, kVertexAttributes, kPrimitive);
        }
      });
      break;

    case DrawMode::DRAW_INLINE_ARRAYS:
      results = Profile(test_name, kIterations, [this, num_draws] {
        for (auto i = 0; i < num_draws; ++i) {
          host_.DrawInlineArray(kVertexAttributes, kPrimitive);
        }
      });
      break;
  }

  host_.FinishDraw(suite_name_, test_name, results);
}
