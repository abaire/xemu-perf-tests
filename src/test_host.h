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
};

void pb_print_with_floats(const char *format, ...);

#endif  // XEMU_PERF_TESTS_TEST_HOST_H
