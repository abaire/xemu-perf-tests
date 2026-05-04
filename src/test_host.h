#ifndef XEMU_PERF_TESTS_TEST_HOST_H
#define XEMU_PERF_TESTS_TEST_HOST_H

#include <cstdint>
#include <string>

#include "nv2astate.h"

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

  //! Renders test results and swaps back buffer.
  void FinishDraw(const std::string &suite_name, const std::string &test_name, const ProfileResults &results);

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

 private:
  bool save_results_{true};

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
