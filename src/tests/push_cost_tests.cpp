#include "push_cost_tests.h"

#include <pbkit/pbkit.h>

#include <algorithm>

#include "pushbuffer.h"
#include "test_host.h"

using namespace PBKitPlusPlus;

static constexpr char kRamWriteTest[] = "RamWrite";
static constexpr char kPushbufferWriteTest[] = "PushbufferWrite";
static constexpr char kPushbufferSubmitTest[] = "PushbufferSubmit";
static constexpr char kPushbufferAPITest[] = "PushbufferAPI";

static constexpr uint32_t kIterations = 10;

// Roughly the number of DWORDs that PfifoContention::Control pushes per frame at its calibrated load.
static constexpr uint32_t kDefaultDwords = 200 * 1024;

// Writes that are never submitted cycle through a window this many DWORDs long. It fits in the pushbuffer and is larger
// than the CPU's L2 cache.
static constexpr uint32_t kWindowDwords = 64 * 1024;

// PushbufferSubmit submits every this many DWORDs, matching Pushbuffer::kDefaultSubmitIntervalDwords.
static constexpr uint32_t kSubmitIntervalDwords = Pushbuffer::kDefaultSubmitIntervalDwords;

// Header of a single-parameter NV097_NO_OPERATION method on the 3D subchannel.
static constexpr uint32_t kNopHeader = (1 << 18) | (SUBCH_3D << 13) | NV097_NO_OPERATION;

//! Returns the number of DWORDs a test should write: its load, rounded down to a whole number of 2-DWORD methods.
static uint32_t MethodDwords(uint32_t load) { return load & ~1u; }

//! Writes `num_dwords` (which must be even) DWORDs of NOP methods to `dest`, wrapping around a window of
//! `window_dwords`.
static void WriteNops(volatile uint32_t *dest, uint32_t num_dwords, uint32_t window_dwords) {
  uint32_t offset = 0;
  for (uint32_t methods = num_dwords / 2; methods; --methods) {
    dest[offset] = kNopHeader;
    dest[offset + 1] = 0;
    offset += 2;
    if (offset >= window_dwords) {
      offset = 0;
    }
  }
}

PushCostTests::PushCostTests(TestHost &host, std::string output_dir, const Config &config)
    : TestSuite(host, std::move(output_dir), "PushCost", config) {
  tests_[kRamWriteTest] = [this]() { TestRamWrite(); };
  tests_[kPushbufferWriteTest] = [this]() { TestPushbufferWrite(); };
  tests_[kPushbufferSubmitTest] = [this]() { TestPushbufferSubmit(); };
  tests_[kPushbufferAPITest] = [this]() { TestPushbufferAPI(); };

  // The tests are compared with each other at equal loads, so calibrating each to the frame budget would defeat them.
  for (auto &[name, _] : tests_) {
    uncalibrated_tests_.insert(name);
  }
}

/**
 * Initializes the test suite and creates test cases.
 *
 * @tc RamWrite
 *  Writes `load` DWORDs of NOP methods into ordinary cached RAM. Baseline for the CPU's store throughput.
 *
 * @tc PushbufferWrite
 *  Writes `load` DWORDs of NOP methods into the pushbuffer's memory without submitting them, so the GPU never reads
 *  them. Compared to RamWrite, shows the cost of writing to the pushbuffer's memory itself.
 *
 * @tc PushbufferSubmit
 *  Writes `load` DWORDs of NOP methods directly into the pushbuffer and submits them with pbkit every 8K DWORDs,
 *  resetting the pushbuffer at the same points as Pushbuffer does. Compared to PushbufferWrite, shows the cost of the
 *  GPU consuming commands while the CPU is writing them.
 *
 * @tc PushbufferAPI
 *  Pushes `load` DWORDs of NOP methods through Pushbuffer::Push with deferred submission, which submits and resets at
 *  the same points as PushbufferSubmit. Compared to PushbufferSubmit, shows the overhead of the Pushbuffer API.
 *
 * Every test starts its frame with an empty pushbuffer, and any reset needed to return the pushbuffer to Pushbuffer's
 * control happens after the frame's measurements.
 */
void PushCostTests::Initialize() {
  TestSuite::Initialize();
  ram_buffer_ = std::make_unique<uint32_t[]>(kWindowDwords);
}

void PushCostTests::Deinitialize() {
  ram_buffer_.reset();
  TestSuite::Deinitialize();
}

void PushCostTests::RunFrame(const char *test_name, const std::function<void()> &body) {
  host_.SetVertexShaderProgram(nullptr);
  host_.SetupFixedFunctionPassthrough();

  host_.PrepareDraw(0xFF222222);

  // Start from an empty pushbuffer so every test performs its resets at the same points. This is inside the measured
  // frame, but it is the same for every test, its wait (for the clear) is reported in reset_wait_us, and it is short.
  Pushbuffer::Flush();

  auto results = Profile(test_name, kIterations, body);

  host_.FinishDraw(suite_name_, test_name, results);

  // PushbufferWrite and PushbufferSubmit write to the pushbuffer behind Pushbuffer's back, so it no longer knows how
  // much of the pushbuffer is in use. Resetting returns it to a known state; the GPU is idle after FinishDraw.
  Pushbuffer::Flush();
}

void PushCostTests::TestRamWrite() {
  const uint32_t num_dwords = MethodDwords(GetLoad(kRamWriteTest, kDefaultDwords));
  RunFrame(kRamWriteTest, [this, num_dwords]() { WriteNops(ram_buffer_.get(), num_dwords, kWindowDwords); });
}

void PushCostTests::TestPushbufferWrite() {
  const uint32_t num_dwords = MethodDwords(GetLoad(kPushbufferWriteTest, kDefaultDwords));
  RunFrame(kPushbufferWriteTest, [num_dwords]() {
    // The pushbuffer is empty, so the window fits. Ending the block at its start submits nothing, so the GPU never
    // reads the data and later pushes overwrite it.
    auto p = pb_begin();
    WriteNops(p, num_dwords, kWindowDwords);
    pb_end(p);
  });
}

void PushCostTests::TestPushbufferSubmit() {
  const uint32_t num_dwords = MethodDwords(GetLoad(kPushbufferSubmitTest, kDefaultDwords));
  // Counts DWORDs pushed since the pushbuffer was last reset. It starts at zero because RunFrame flushes before
  // profiling, and it carries over between the body's iterations because the pushbuffer is only reset here.
  uint32_t since_reset = 0;
  RunFrame(kPushbufferSubmitTest, [num_dwords, &since_reset]() {
    // Reset at the same point that Pushbuffer does automatically, rounded down to a whole number of 2-DWORD methods,
    // so that this and PushbufferAPI perform the same number of resets.
    const uint32_t dwords_per_reset = Pushbuffer::CapacityDwords() & ~1u;
    uint32_t remaining = num_dwords;
    while (remaining) {
      uint32_t block = std::min(remaining, kSubmitIntervalDwords);
      if (since_reset + block > dwords_per_reset) {
        if (since_reset < dwords_per_reset) {
          // Fill up to the reset point first, as Pushbuffer would.
          block = dwords_per_reset - since_reset;
        } else {
          // Resets go through Pushbuffer so that their wait is reported in reset_wait_us.
          Pushbuffer::Flush();
          since_reset = 0;
          continue;
        }
      }

      auto p = pb_begin();
      WriteNops(p, block, block);
      pb_end(p + block);
      remaining -= block;
      since_reset += block;
    }
  });
}

void PushCostTests::TestPushbufferAPI() {
  const uint32_t num_dwords = MethodDwords(GetLoad(kPushbufferAPITest, kDefaultDwords));
  RunFrame(kPushbufferAPITest, [num_dwords]() {
    Pushbuffer::DeferredSubmitScope deferred;
    for (uint32_t methods = num_dwords / 2; methods; --methods) {
      Pushbuffer::Begin();
      Pushbuffer::Push(NV097_NO_OPERATION, 0);
      Pushbuffer::End();
    }
  });
}
