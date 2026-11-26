#ifndef XEMU_PERF_TESTS_TEST_HOST_H
#define XEMU_PERF_TESTS_TEST_HOST_H

#include <pbkit/pbkit.h>

#include <cstdint>
#include <string>
#include <vector>

#include "nv2astate.h"
#include "pushbuffer.h"

/**
 * Provides utility methods for use by TestSuite subclasses.
 */
class TestHost : public PBKitPlusPlus::NV2AState {
 public:
  struct ProfileResults {
    uint32_t iterations;
    uint32_t total_time_microseconds;
    uint32_t average_time_microseconds;
    uint32_t maximum_time_microseconds;
    uint32_t minimum_time_microseconds;
    std::vector<uint32_t> raw_results;
  };

 public:
  TestHost(uint32_t framebuffer_width, uint32_t framebuffer_height, uint32_t max_texture_width = 256,
           uint32_t max_texture_height = 256, uint32_t max_texture_depth = 4);

  //! Creates the given directory if it does not already exist.
  static void EnsureFolderExists(const std::string &folder_path);

  //! Waits for vblank, clears the framebuffer, and (in sustained mode) marks the start of the frame's timed region.
  //! Hides NV2AState::PrepareDraw so that all tests get sustained-mode timing without modification.
  void PrepareDraw(uint32_t argb = 0xFF000000, uint32_t depth_value = 0xFFFFFFFF, uint8_t stencil_value = 0x00);

  //! Renders test results and swaps back buffer.
  void FinishDraw(const std::string &suite_name, const std::string &test_name, const ProfileResults &results);

  //! Waits for the GPU to finish the frame's work without presenting it, leaving the displayed image unchanged. In
  //! sustained mode the frame is timed like any other.
  void FinishDrawWithoutPresent();

  //! Excludes the current sustained-mode frame from the recorded results.
  void SkipRecordingCurrentFrame() { sustained_skip_current_frame_ = true; }

  //! First DMA context channel available for use by test suites.
  static constexpr int32_t kFirstTestContextChannel = PBKitPlusPlus::kNextContextChannel + 1;

  //! Sets up the projection matrix for passthrough operation / direct addressing of pixels.
  void SetupFixedFunctionPassthrough();

  [[nodiscard]] bool GetSaveResults() const { return save_results_; }
  void SetSaveResults(bool enable = true) { save_results_ = enable; }

  [[nodiscard]] const double &GetPerformanceCounterFrequency() const { return perf_counter_frequency_; }
  [[nodiscard]] uint32_t GetMicrosecondsSince(const LARGE_INTEGER &previous) const;

  void PreTest() {
    current_frame_index_ = 0;
    last_frame_time_.QuadPart = 0;
    average_frame_rate_ = 0.f;
    average_mspf_ = 0.f;
  }

  //! Begins collecting per-frame timing for a sustained (multi-frame, non-interactive) run.
  //! While active, FinishDraw skips the results overlay and records timing for each frame.
  void BeginSustainedCapture(uint32_t expected_frames);

  //! Marks the start of a frame in sustained mode. Must be called before the test body issues any work.
  void BeginSustainedFrame();

  //! Enables/disables recording of frames passed to FinishDraw (used to discard warmup frames).
  void SetSustainedRecording(bool enable) { sustained_recording_ = enable; }

  //! Summary of a sustained capture.
  struct SustainedSummary {
    uint32_t frames;
    uint32_t busy_p50_us;
    uint32_t busy_p95_us;
    uint32_t missed_vblanks;
    uint32_t fence_timeouts;
    //! Largest number of pushbuffer resets in any recorded frame. Non-zero means the load no longer fits in the
    //! pushbuffer, so frames are interrupted by waits for the GPU to go idle.
    uint32_t max_resets;
    //! True if the capture was stopped early, in which case it does not represent the requested number of frames.
    bool aborted;
  };

  //! Ends sustained capture and returns a summary of it. The recorded frames are kept until the next capture begins, so
  //! they may be logged with LogSustainedCapture().
  SustainedSummary EndSustainedCapture(const std::string &suite_name, const std::string &test_name, uint32_t load,
                                       bool aborted);

  //! Writes a results record for the most recently ended capture. If `calibration_label` is not null it is recorded as
  //! the record's "calibration" field.
  void LogSustainedCapture(const std::string &suite_name, const std::string &test_name, uint32_t load,
                           uint32_t warmup_frames, const char *calibration_label = nullptr);

  //! Returns the busy time of the most recently finished sustained frame, whether or not it was recorded.
  [[nodiscard]] uint32_t GetLastSustainedFrameBusyUs() const { return sustained_last_busy_us_; }

  [[nodiscard]] bool IsSustainedCaptureActive() const { return sustained_active_; }

  //! Blocks until the GPU has finished all work submitted so far.
  //!
  //! Pushes a NV097_BACK_END_WRITE_SEMAPHORE_RELEASE and spins on the RAM value the GPU writes, rather than polling GPU
  //! registers via pb_busy(). Under xemu each register read is an MMIO access that contends for the same locks the
  //! benchmarks are trying to measure, so polling would perturb the results.
  //!
  //! Returns false if the fence timed out, in which case this falls back to pb_busy().
  bool WaitForGPUIdle();

 private:
  void FinishSustainedFrame(bool present);

  //! Marks the start of the current frame's timed region.
  void StampSustainedFrameStart();
  void EnsureSemaphoreInitialized();

 private:
  bool save_results_{true};

  //! Timing for a single frame in sustained mode.
  struct SustainedFrame {
    //! Time from frame start until all work has been submitted to the pushbuffer. Includes `reset_wait_us`.
    uint32_t submit_us;
    //! Portion of `submit_us` spent in pushbuffer resets, each of which waits for the GPU to finish all submitted work.
    uint32_t reset_wait_us;
    //! Number of pushbuffer resets during submission.
    uint32_t resets;
    //! Time from frame start until the GPU reports idle. Primary metric; not quantized by vsync.
    uint32_t busy_us;
    //! Time from the end of the previous recorded frame (after its flip, if any) to the end of this one. 0 for the
    //! first recorded frame.
    uint32_t frame_us;
    //! Number of vblanks between the end of the previous frame and the end of this one.
    uint32_t vblanks;
  };

  bool sustained_active_{false};
  bool sustained_recording_{false};
  //! True until the first PrepareDraw of the current frame re-stamps the frame start.
  bool sustained_awaiting_prepare_{false};
  //! True if the current frame should not be recorded.
  bool sustained_skip_current_frame_{false};
  LARGE_INTEGER sustained_frame_start_{};
  //! Pushbuffer reset statistics at `sustained_frame_start_`.
  PBKitPlusPlus::Pushbuffer::ResetStats sustained_frame_start_resets_{};
  //! End of the most recent recorded frame, or 0 if none has been recorded yet.
  LARGE_INTEGER sustained_last_recorded_end_{};
  //! Missed vblanks during frames that were not recorded because of SkipRecordingCurrentFrame().
  uint32_t sustained_skipped_missed_vblanks_{0};
  uint32_t sustained_last_busy_us_{0};
  bool sustained_aborted_{false};
  SustainedSummary sustained_summary_{};
  DWORD sustained_last_vbl_{0};
  std::vector<SustainedFrame> sustained_frames_;
  uint32_t sustained_fence_timeouts_{0};

  //! RAM written by the GPU on NV097_BACK_END_WRITE_SEMAPHORE_RELEASE.
  volatile uint32_t *semaphore_memory_{nullptr};
  struct s_CtxDma semaphore_dma_ctx_{};
  uint32_t semaphore_value_{0};

  static constexpr auto kFrameTimeWindow = 10;
  double perf_counter_frequency_;
  LARGE_INTEGER last_frame_time_;
  uint32_t current_frame_index_ = 0;
  float frame_times_[kFrameTimeWindow] = {0.f};
  float average_frame_rate_ = 0.f;
  float average_mspf_ = 0.f;
};

void pb_print_with_floats(const char *format, ...);

#endif  // XEMU_PERF_TESTS_TEST_HOST_H
