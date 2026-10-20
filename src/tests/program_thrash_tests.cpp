#include "program_thrash_tests.h"

#include <pbkit/pbkit.h>

#include "pushbuffer.h"
#include "shaders/passthrough_vertex_shader.h"
#include "shaders/vertex_shader_program.h"
#include "test_host.h"

static constexpr char kReuploadTest[] = "Reupload";
static constexpr char kReuploadLongTest[] = "ReuploadLong";
static constexpr char kResidentTest[] = "Resident";

static constexpr uint32_t kIterations = 10;
static constexpr uint32_t kNumDrawsSingleFrame = 1000;
// Measured using a 1.0 devkit, slightly below the 60 fps limit. DPAD left/right may be used in continuous mode to
// find the largest value that maintains 60 fps.
static constexpr uint32_t kNumDrawsReupload = 325;
static constexpr uint32_t kNumDrawsReuploadLong = 160;
static constexpr uint32_t kNumDrawsResident = 355;
static constexpr uint32_t kNumDrawsAdjustmentStep = 5;

// Instruction slot at which the second program is loaded in the resident test. Must be past the end of the
// passthrough program loaded at slot 0.
static constexpr uint32_t kSecondProgramSlot = 64;

static uint32_t kVertexAttributes = TestHost::POSITION | TestHost::DIFFUSE;
static TestHost::DrawPrimitive kPrimitive = TestHost::PRIMITIVE_TRIANGLES;

// clang-format off
static const uint32_t kLongPassthroughShader[] = {
#include "long_passthrough.vshinc"
};

static const uint32_t kDiffuseFromUniformShader[] = {
#include "diffuse_from_uniform.vshinc"
};
// clang-format on

//! Loads the given vertex program into transform program memory starting at the given slot without changing the
//! program start address.
static void LoadProgramAt(uint32_t slot, const uint32_t *shader, uint32_t shader_size) {
  PBKitPlusPlus::Pushbuffer::Begin();
  PBKitPlusPlus::Pushbuffer::Push(NV097_SET_TRANSFORM_PROGRAM_LOAD, slot);
  for (auto i = 0; i < shader_size / 16; ++i) {
    auto *value = reinterpret_cast<const DWORD *>(shader + (i * 4));
    PBKitPlusPlus::Pushbuffer::Push4(NV097_SET_TRANSFORM_PROGRAM, value);
  }
  PBKitPlusPlus::Pushbuffer::End();
}

ProgramThrashTests::ProgramThrashTests(TestHost &host, std::string output_dir, const Config &config)
    : TestSuite(host, std::move(output_dir), "ProgramThrash", config) {
  tests_[kReuploadTest] = [this]() { TestReupload(kReuploadTest, false); };
  tests_[kReuploadLongTest] = [this]() { TestReupload(kReuploadLongTest, true); };
  tests_[kResidentTest] = [this]() { TestResident(kResidentTest); };

  num_draws_multi_frame_[kReuploadTest] = kNumDrawsReupload;
  num_draws_multi_frame_[kReuploadLongTest] = kNumDrawsReuploadLong;
  num_draws_multi_frame_[kResidentTest] = kNumDrawsResident;
}

/**
 * Initializes the test suite and creates test cases.
 *
 * @tc Reupload
 *   Renders hundreds of tiny triangles using individual DRAW_ARRAYS calls, uploading a short passthrough vertex
 *   program before each draw.
 *
 * @tc ReuploadLong
 *   Renders hundreds of tiny triangles using individual DRAW_ARRAYS calls, uploading a passthrough vertex program
 *   that fills all 136 instruction slots before each draw.
 *
 * @tc Resident
 *   Renders hundreds of tiny triangles using individual DRAW_ARRAYS calls, alternating between two vertex programs
 *   that are both resident in transform program memory by changing the program start address before each draw.
 *   All draws overlap, so the visible triangle is solid green if the number of draws is even and vertex colored if it
 *   is odd.
 */
void ProgramThrashTests::Initialize() {
  TestSuite::Initialize();

  host_.SetFinalCombiner0Just(TestHost::SRC_DIFFUSE);
  host_.SetFinalCombiner1Just(TestHost::SRC_ZERO, true, true);

  vertex_buffer_ = host_.AllocateVertexBuffer(3);
  auto vertex = vertex_buffer_->Lock();

  vertex->SetPosition(300.f, 200.f, 1.f);
  vertex->SetDiffuse(1.f, 0.f, 0.f);
  ++vertex;

  vertex->SetPosition(304.f, 200.f, 1.f);
  vertex->SetDiffuse(0.f, 1.f, 0.f);
  ++vertex;

  vertex->SetPosition(302.f, 204.f, 1.f);
  vertex->SetDiffuse(0.f, 0.f, 1.f);
  ++vertex;

  vertex_buffer_->Unlock();
}

void ProgramThrashTests::Deinitialize() {
  host_.ClearVertexBuffer();
  TestSuite::Deinitialize();
}

void ProgramThrashTests::UpdateUserContext(int direction) {
  auto it = num_draws_multi_frame_.find(active_test_);
  if (it == num_draws_multi_frame_.end()) {
    return;
  }

  if (direction < 0) {
    if (it->second > kNumDrawsAdjustmentStep) {
      it->second -= kNumDrawsAdjustmentStep;
    }
  } else {
    it->second += kNumDrawsAdjustmentStep;
  }
}

uint32_t ProgramThrashTests::GetNumDraws(const std::string &test_name) {
  active_test_ = test_name;
  return host_.GetSaveResults() ? kNumDrawsSingleFrame : num_draws_multi_frame_[test_name];
}

void ProgramThrashTests::PrintNumDraws(const std::string &test_name) {
  if (!host_.GetSaveResults()) {
    pb_print("Draws: %lu (DPAD left/right to adjust)\n", num_draws_multi_frame_[test_name]);
  }
}

void ProgramThrashTests::TestReupload(const std::string &test_name, bool long_shader) {
  host_.PrepareDraw(0xFF333333);

  std::shared_ptr<PBKitPlusPlus::VertexShaderProgram> shader;
  if (long_shader) {
    shader =
        std::make_shared<PBKitPlusPlus::VertexShaderProgram>(kLongPassthroughShader, sizeof(kLongPassthroughShader));
    // See long_passthrough.vsh.
    shader->SetUniformF(0, 0.f, 0.5f, 0.25f, 1.f);
  } else {
    shader = std::make_shared<PBKitPlusPlus::PassthroughVertexShader>();
  }
  host_.SetVertexShaderProgram(shader);

  const uint32_t num_draws = GetNumDraws(test_name);
  auto results = Profile(test_name, kIterations, [this, shader, num_draws] {
    for (auto i = 0; i < num_draws; ++i) {
      shader->Activate();
      host_.DrawArrays(kVertexAttributes, kPrimitive);
    }
  });

  PrintNumDraws(test_name);
  host_.FinishDraw(suite_name_, test_name, results);

  host_.SetVertexShaderProgram(nullptr);
}

void ProgramThrashTests::TestResident(const std::string &test_name) {
  host_.PrepareDraw(0xFF333333);

  host_.SetVertexShaderProgram(std::make_shared<PBKitPlusPlus::PassthroughVertexShader>());

  // Load a second program after the passthrough program and upload its uniforms directly, as it will never be the
  // active pbkit program.
  LoadProgramAt(kSecondProgramSlot, kDiffuseFromUniformShader, sizeof(kDiffuseFromUniformShader));
  PBKitPlusPlus::VertexShaderProgram diffuse_from_uniform;
  diffuse_from_uniform.SetUniformF(0, 0.f, 1.f, 0.f, 1.f);
  diffuse_from_uniform.PrepareDraw();

  const uint32_t num_draws = GetNumDraws(test_name);
  auto results = Profile(test_name, kIterations, [this, num_draws] {
    for (auto i = 0; i < num_draws; ++i) {
      PBKitPlusPlus::Pushbuffer::Begin();
      PBKitPlusPlus::Pushbuffer::Push(NV097_SET_TRANSFORM_PROGRAM_START, (i & 1) ? kSecondProgramSlot : 0);
      PBKitPlusPlus::Pushbuffer::End();
      host_.DrawArrays(kVertexAttributes, kPrimitive);
    }
  });

  PrintNumDraws(test_name);
  host_.FinishDraw(suite_name_, test_name, results);

  host_.SetVertexShaderProgram(nullptr);
}
