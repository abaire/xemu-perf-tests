#include "pfifo_contention_tests.h"

#include <pbkit/pbkit.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmacro-redefined"
#include <windows.h>
#pragma clang diagnostic pop

#include <cstring>

#include "debug_output.h"
#include "pushbuffer.h"
#include "test_host.h"

using namespace PBKitPlusPlus;

#define SET_MASK(mask, val) (((val) << (__builtin_ffs(mask) - 1)) & (mask))

static constexpr char kControlTest[] = "Control";
static constexpr char kLongPushbufferTest[] = "LongPushbuffer";
static constexpr char kMethodChurnTest[] = "MethodChurn";
static constexpr char kTinyKicksTest[] = "TinyKicks";
static constexpr char kHalfRatePresentTest[] = "HalfRatePresent";
static constexpr char kSurfaceReadbackTest[] = "SurfaceReadback";
static constexpr char kOcclusionSpinTest[] = "OcclusionSpin";
static constexpr char kCpuFramebufferWriteTest[] = "CpuFramebufferWrite";
static constexpr char kFillBoundTest[] = "FillBound";

static constexpr uint32_t kIterations = 10;

// Placeholder loads, used only for tests that have no entry in the calibration baseline and no runtime config override.
// These are not calibrated.
static constexpr uint32_t kDefaultControlDraws = 250;
static constexpr uint32_t kDefaultLongPushbufferDraws = 250;
static constexpr uint32_t kDefaultMethodChurnMethods = 50000;
static constexpr uint32_t kDefaultTinyKicksDraws = 200;
static constexpr uint32_t kDefaultHalfRatePresentDraws = 250;
static constexpr uint32_t kDefaultSurfaceReadbackDraws = 40;
static constexpr uint32_t kDefaultOcclusionSpinQueries = 20;
static constexpr uint32_t kDefaultCpuFramebufferWriteDwords = 4096;
static constexpr uint32_t kDefaultFillBoundQuads = 20;

// Number of draws per frame in MethodChurn. The load scales the number of state methods interleaved between them.
static constexpr uint32_t kMethodChurnDraws = 16;

// Single-DWORD state methods that have no effect on rendering (the values are overwritten before any clear) and are
// written to distinct registers, so each one is processed individually.
static constexpr uint32_t kChurnMethods[] = {
    NV097_SET_COLOR_CLEAR_VALUE,
    NV097_SET_ZSTENCIL_CLEAR_VALUE,
    NV097_SET_CLEAR_RECT_HORIZONTAL,
    NV097_SET_CLEAR_RECT_VERTICAL,
};
static constexpr uint32_t kNumChurnMethods = sizeof(kChurnMethods) / sizeof(kChurnMethods[0]);

// Size of the grid cell in which each tiny triangle is drawn.
static constexpr uint32_t kCellSize = 4;

// Dimensions of the offscreen render target used by HalfRatePresent and SurfaceReadback.
static constexpr uint32_t kOffscreenWidth = 256;
static constexpr uint32_t kOffscreenHeight = 256;

// Number of draws made to the screen on the presenting frames of HalfRatePresent.
static constexpr uint32_t kHalfRatePresentScreenDraws = 16;

// Number of times per frame that SurfaceReadback reads back from a freshly rendered surface.
static constexpr uint32_t kReadbacksPerFrame = 4;

// Size of the quad drawn for each OcclusionSpin query.
static constexpr float kOcclusionQuadSize = 16.f;
// Placed in the report before each query; larger than any possible pixel count.
static constexpr uint32_t kReportPending = 0xFFFFFFFF;

// Upper bound on how long OcclusionSpin waits for a single report before giving up.
static constexpr DWORD kReportTimeoutMilliseconds = 2000;

// pbkit binds channel 12 as the default DMA_REPORT context object.
static constexpr uint32_t kDefaultReportContextChannel = 12;
static constexpr uint32_t kReportContextChannel = TestHost::kFirstTestContextChannel;

//! Layout of a NV097_GET_REPORT result.
struct ZPassReport {
  uint64_t timestamp;
  uint32_t value;
  uint32_t reserved;
};

PfifoContentionTests::PfifoContentionTests(TestHost &host, std::string output_dir, const Config &config)
    : TestSuite(host, std::move(output_dir), "PfifoContention", config) {
  tests_[kControlTest] = [this]() { TestControl(); };
  tests_[kLongPushbufferTest] = [this]() { TestLongPushbuffer(); };
  tests_[kMethodChurnTest] = [this]() { TestMethodChurn(); };
  tests_[kTinyKicksTest] = [this]() { TestTinyKicks(); };
  tests_[kHalfRatePresentTest] = [this]() { TestHalfRatePresent(); };
  tests_[kSurfaceReadbackTest] = [this]() { TestSurfaceReadback(); };
  tests_[kOcclusionSpinTest] = [this]() { TestOcclusionSpin(); };
  tests_[kCpuFramebufferWriteTest] = [this]() { TestCpuFramebufferWrite(); };
  tests_[kFillBoundTest] = [this]() { TestFillBound(); };
}

/**
 * Initializes the test suite and creates test cases.
 *
 * @tc Control
 *  Draws `load` tiny triangles, submitted to the GPU in a few large chunks per frame. Hits none of the targeted
 *  pathologies and serves as the baseline for the other tests.
 *
 * @tc LongPushbuffer
 *  Draws `load` tiny triangles, submitted to the GPU all at once at the end of the frame, so the GPU processes one long
 *  uninterrupted run of commands.
 *
 * @tc MethodChurn
 *  Draws a fixed number of tiny triangles with `load` single-DWORD state methods interleaved between them. Each method
 *  is processed individually, so the cost scales with the number of methods rather than the amount of data.
 *
 * @tc TinyKicks
 *  Draws `load` tiny triangles, submitting each one to the GPU separately and reading a PGRAPH register from the CPU
 *  after each submission.
 *
 * @tc HalfRatePresent
 *  Presents every other frame (30fps). The frames in between draw `load` tiny triangles to an offscreen surface without
 *  presenting, so the displayed image does not change; only those frames are recorded.
 *
 * @tc SurfaceReadback
 *  Several times per frame, renders to an offscreen surface, queues `load` tiny triangles to the screen behind it, then
 *  reads a pixel of the offscreen surface from the CPU before queueing `load` more.
 *
 * @tc OcclusionSpin
 *  Issues `load` zpass pixel count queries per frame, each covering a small quad, and spins on the CPU until each
 *  report has been written before issuing the next.
 *
 * @tc CpuFramebufferWrite
 *  Draws a few tiny triangles to the back buffer, then writes `load` DWORDs directly into the back buffer from the CPU
 *  before presenting it.
 *
 * @tc FillBound
 *  Draws `load` alpha blended quads covering the whole back buffer. Each quad is a handful of DWORDs but many pixels,
 *  so the frame time should be dominated by the GPU rather than by submission. Validates that the measurements can
 *  tell the two apart.
 */
void PfifoContentionTests::Initialize() {
  TestSuite::Initialize();

  host_.SetFinalCombiner0Just(TestHost::SRC_DIFFUSE);
  host_.SetFinalCombiner1Just(TestHost::SRC_DIFFUSE, true);

  // Offscreen render targets are bound with the framebuffer's zeta buffer, so depth and stencil writes are disabled to
  // keep them from writing outside of it.
  Pushbuffer::Begin();
  Pushbuffer::Push(NV097_SET_DEPTH_TEST_ENABLE, false);
  Pushbuffer::Push(NV097_SET_DEPTH_MASK, false);
  Pushbuffer::Push(NV097_SET_STENCIL_MASK, false);
  Pushbuffer::End();

  if (!report_memory_) {
    // The DMA context cannot be destroyed, so the report memory is never freed.
    auto memory = MmAllocateContiguousMemoryEx(sizeof(ZPassReport), 0, MAXRAM, 0, PAGE_READWRITE);
    ASSERT(memory && "Failed to allocate report memory");
    memset(memory, 0, sizeof(ZPassReport));
    report_memory_ = memory;
    pb_create_dma_ctx(kReportContextChannel, DMA_CLASS_3D, reinterpret_cast<DWORD>(memory), sizeof(ZPassReport) - 1,
                      &report_dma_ctx_);
    pb_bind_channel(&report_dma_ctx_);
  }

  half_rate_frame_ = 0;
}

void PfifoContentionTests::RunFrame(const char *test_name, const std::function<void()> &body) {
  host_.SetVertexShaderProgram(nullptr);
  host_.SetupFixedFunctionPassthrough();

  host_.PrepareDraw(0xFF222222);

  auto results = Profile(test_name, kIterations, body);

  host_.FinishDraw(suite_name_, test_name, results);
}

void PfifoContentionTests::DrawTinyTriangle(uint32_t index) const {
  DrawTinyTriangle(index, host_.GetFramebufferWidth(), host_.GetFramebufferHeight());
}

void PfifoContentionTests::DrawTinyTriangle(uint32_t index, uint32_t target_width, uint32_t target_height) const {
  static constexpr float kZ = 1.f;
  static constexpr float kW = 1.f;

  const uint32_t columns = target_width / kCellSize;
  const uint32_t rows = target_height / kCellSize;
  const uint32_t cell = index % (columns * rows);
  const auto left = static_cast<float>((cell % columns) * kCellSize);
  const auto top = static_cast<float>((cell / columns) * kCellSize);
  const auto size = static_cast<float>(kCellSize - 1);

  // Pushed as one block, rather than through the NV2AState helpers that each open their own, to keep the CPU cost
  // per draw low. The methods are the same.
  Pushbuffer::Begin();
  Pushbuffer::Push(NV097_SET_BEGIN_END, TestHost::PRIMITIVE_TRIANGLES);
  Pushbuffer::Push(NV097_SET_DIFFUSE_COLOR4I, 0xFF000000 | (index * 0x00102030));
  Pushbuffer::PushF(NV097_SET_VERTEX4F, left, top, kZ, kW);
  Pushbuffer::PushF(NV097_SET_VERTEX4F, left + size, top, kZ, kW);
  Pushbuffer::PushF(NV097_SET_VERTEX4F, left, top + size, kZ, kW);
  Pushbuffer::Push(NV097_SET_BEGIN_END, NV097_SET_BEGIN_END_OP_END);
  Pushbuffer::End();
}

void PfifoContentionTests::TestControl() {
  const uint32_t num_draws = GetLoad(kControlTest, kDefaultControlDraws);
  RunFrame(kControlTest, [this, num_draws]() {
    Pushbuffer::DeferredSubmitScope deferred;
    for (uint32_t i = 0; i < num_draws; ++i) {
      DrawTinyTriangle(i);
    }
  });
}

void PfifoContentionTests::TestLongPushbuffer() {
  const uint32_t num_draws = GetLoad(kLongPushbufferTest, kDefaultLongPushbufferDraws);
  RunFrame(kLongPushbufferTest, [this, num_draws]() {
    // Submit the whole frame at once, unless it exceeds the pushbuffer capacity and forces a reset.
    Pushbuffer::DeferredSubmitScope deferred(Pushbuffer::CapacityDwords());
    for (uint32_t i = 0; i < num_draws; ++i) {
      DrawTinyTriangle(i);
    }
  });
}

void PfifoContentionTests::TestMethodChurn() {
  const uint32_t num_methods = GetLoad(kMethodChurnTest, kDefaultMethodChurnMethods);
  RunFrame(kMethodChurnTest, [this, num_methods]() {
    Pushbuffer::DeferredSubmitScope deferred;
    const uint32_t methods_per_draw = num_methods / kMethodChurnDraws;
    for (uint32_t draw = 0; draw < kMethodChurnDraws; ++draw) {
      Pushbuffer::Begin();
      for (uint32_t i = 0; i < methods_per_draw; ++i) {
        Pushbuffer::Push(kChurnMethods[i % kNumChurnMethods], 0);
      }
      Pushbuffer::End();

      DrawTinyTriangle(draw);
    }
  });
}

void PfifoContentionTests::TestTinyKicks() {
  const uint32_t num_draws = GetLoad(kTinyKicksTest, kDefaultTinyKicksDraws);
  RunFrame(kTinyKicksTest, [this, num_draws]() {
    volatile uint32_t pgraph_status;
    for (uint32_t i = 0; i < num_draws; ++i) {
      {
        Pushbuffer::DeferredSubmitScope deferred;
        DrawTinyTriangle(i);
      }
      pgraph_status = VIDEOREG(NV_PGRAPH_STATUS);
    }
    (void)pgraph_status;
  });
}

void PfifoContentionTests::TestHalfRatePresent() {
  const uint32_t num_draws = GetLoad(kHalfRatePresentTest, kDefaultHalfRatePresentDraws);
  // Single-frame runs always draw and report the measured (offscreen) frame.
  const bool present = !host_.GetSaveResults() && (half_rate_frame_++ & 1) != 0;

  host_.SetVertexShaderProgram(nullptr);
  host_.SetupFixedFunctionPassthrough();
  host_.PrepareDraw(0xFF222222);

  if (present) {
    host_.SkipRecordingCurrentFrame();
    {
      Pushbuffer::DeferredSubmitScope deferred;
      for (uint32_t i = 0; i < kHalfRatePresentScreenDraws; ++i) {
        DrawTinyTriangle(i);
      }
    }
    host_.FinishDraw(suite_name_, kHalfRatePresentTest, {});
    return;
  }

  auto results = Profile(kHalfRatePresentTest, kIterations, [this, num_draws]() {
    host_.RenderToSurfaceStart(host_.GetTextureMemoryForStage(0), TestHost::SCF_A8R8G8B8, kOffscreenWidth,
                               kOffscreenHeight);
    {
      Pushbuffer::DeferredSubmitScope deferred;
      for (uint32_t i = 0; i < num_draws; ++i) {
        DrawTinyTriangle(i, kOffscreenWidth, kOffscreenHeight);
      }
    }
    host_.RenderToSurfaceEnd();
  });

  if (host_.IsSustainedCaptureActive()) {
    host_.FinishDrawWithoutPresent();
  } else {
    host_.FinishDraw(suite_name_, kHalfRatePresentTest, results);
  }
}

void PfifoContentionTests::TestSurfaceReadback() {
  const uint32_t num_draws = GetLoad(kSurfaceReadbackTest, kDefaultSurfaceReadbackDraws);
  RunFrame(kSurfaceReadbackTest, [this, num_draws]() {
    auto surface = reinterpret_cast<volatile uint32_t *>(host_.GetTextureMemoryForStage(0));
    volatile uint32_t pixel;
    uint32_t index = 0;
    for (uint32_t readback = 0; readback < kReadbacksPerFrame; ++readback) {
      host_.RenderToSurfaceStart(host_.GetTextureMemoryForStage(0), TestHost::SCF_A8R8G8B8, kOffscreenWidth,
                                 kOffscreenHeight);
      DrawTinyTriangle(readback, kOffscreenWidth, kOffscreenHeight);
      host_.RenderToSurfaceEnd();

      {
        Pushbuffer::DeferredSubmitScope deferred;
        for (uint32_t i = 0; i < num_draws; ++i) {
          DrawTinyTriangle(index++);
        }
      }

      // Read a pixel covered by the triangle just drawn to the offscreen surface.
      const uint32_t columns = kOffscreenWidth / kCellSize;
      pixel = surface[(readback / columns) * kCellSize * kOffscreenWidth + (readback % columns) * kCellSize];

      {
        Pushbuffer::DeferredSubmitScope deferred;
        for (uint32_t i = 0; i < num_draws; ++i) {
          DrawTinyTriangle(index++);
        }
      }
    }
    (void)pixel;
  });
}

void PfifoContentionTests::TestOcclusionSpin() {
  const uint32_t num_queries = GetLoad(kOcclusionSpinTest, kDefaultOcclusionSpinQueries);
  RunFrame(kOcclusionSpinTest, [this, num_queries]() {
    auto report = static_cast<volatile ZPassReport *>(report_memory_);

    Pushbuffer::Begin();
    Pushbuffer::Push(NV097_SET_CONTEXT_DMA_REPORT, report_dma_ctx_.ChannelID);
    Pushbuffer::Push(NV097_SET_DEPTH_TEST_ENABLE, true);
    Pushbuffer::Push(NV097_SET_DEPTH_FUNC, NV097_SET_DEPTH_FUNC_V_ALWAYS);
    Pushbuffer::End();

    const uint32_t columns = host_.GetFramebufferWidth() / static_cast<uint32_t>(kOcclusionQuadSize);
    const uint32_t rows = host_.GetFramebufferHeight() / static_cast<uint32_t>(kOcclusionQuadSize);
    for (uint32_t query = 0; query < num_queries; ++query) {
      // A count of 0 is a valid result, so the report is marked with a value the GPU will never write.
      report->value = kReportPending;

      Pushbuffer::Begin();
      Pushbuffer::Push(NV097_CLEAR_REPORT_VALUE, NV097_CLEAR_REPORT_VALUE_TYPE_ZPASS_PIXEL_CNT);
      Pushbuffer::Push(NV097_SET_ZPASS_PIXEL_COUNT_ENABLE, true);
      Pushbuffer::End();

      // Wrap so that every quad is on screen.
      const uint32_t cell = query % (columns * rows);
      const float left = static_cast<float>(cell % columns) * kOcclusionQuadSize;
      const float top = static_cast<float>(cell / columns) * kOcclusionQuadSize;
      host_.SetDiffuse(0xFF000000 | (query * 0x00203040));
      host_.DrawScreenQuad(left, top, left + kOcclusionQuadSize, top + kOcclusionQuadSize, 0.f);

      Pushbuffer::Begin();
      Pushbuffer::Push(NV097_SET_ZPASS_PIXEL_COUNT_ENABLE, false);
      Pushbuffer::Push(NV097_GET_REPORT, SET_MASK(NV097_GET_REPORT_TYPE, NV097_GET_REPORT_TYPE_ZPASS_PIXEL_CNT));
      Pushbuffer::End();

      const DWORD start = GetTickCount();
      while (report->value == kReportPending) {
        if (GetTickCount() - start > kReportTimeoutMilliseconds) {
          PrintMsg("OcclusionSpin: report %lu timed out\n", static_cast<unsigned long>(query));
          break;
        }
      }
    }

    Pushbuffer::Begin();
    Pushbuffer::Push(NV097_SET_DEPTH_TEST_ENABLE, false);
    Pushbuffer::Push(NV097_SET_CONTEXT_DMA_REPORT, kDefaultReportContextChannel);
    Pushbuffer::End();
  });
}

void PfifoContentionTests::TestCpuFramebufferWrite() {
  const uint32_t num_dwords = GetLoad(kCpuFramebufferWriteTest, kDefaultCpuFramebufferWriteDwords);
  RunFrame(kCpuFramebufferWriteTest, [this, num_dwords]() {
    {
      Pushbuffer::DeferredSubmitScope deferred;
      for (uint32_t i = 0; i < kHalfRatePresentScreenDraws; ++i) {
        DrawTinyTriangle(i);
      }
    }

    const uint32_t framebuffer_dwords = host_.GetFramebufferWidth() * host_.GetFramebufferHeight();
    auto framebuffer = reinterpret_cast<volatile uint32_t *>(pb_back_buffer());
    for (uint32_t i = 0; i < num_dwords; ++i) {
      framebuffer[i % framebuffer_dwords] = 0xFF4080C0;
    }
  });
}

void PfifoContentionTests::TestFillBound() {
  const uint32_t num_quads = GetLoad(kFillBoundTest, kDefaultFillBoundQuads);
  RunFrame(kFillBoundTest, [this, num_quads]() {
    const auto width = host_.GetFramebufferWidthF();
    const auto height = host_.GetFramebufferHeightF();
    static constexpr float kZ = 1.f;
    static constexpr float kW = 1.f;

    host_.SetBlend();
    {
      Pushbuffer::DeferredSubmitScope deferred;
      for (uint32_t i = 0; i < num_quads; ++i) {
        host_.Begin(TestHost::PRIMITIVE_QUADS);
        host_.SetDiffuse(0x20000000 | (i * 0x00102030 & 0x00FFFFFF));
        host_.SetVertex(0.f, 0.f, kZ, kW);
        host_.SetVertex(width, 0.f, kZ, kW);
        host_.SetVertex(width, height, kZ, kW);
        host_.SetVertex(0.f, height, kZ, kW);
        host_.End();
      }
    }
    host_.SetBlend(false);
  });
}
