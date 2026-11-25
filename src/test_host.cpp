#include "test_host.h"

#include <SDL.h>
#include <strings.h>

#include <algorithm>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmacro-redefined"
#include <windows.h>
#pragma clang diagnostic pop

#include <texture_generator.h>
#include <xboxkrnl/xboxkrnl.h>

#include "debug_output.h"
#include "logger.h"
#include "pushbuffer.h"
#include "shaders/vertex_shader_program.h"
#include "xbox_math_matrix.h"
#include "xbox_math_types.h"

using namespace XboxMath;

static constexpr uint32_t kResultsOverlayColor = 0x88000000;

// DMA context channel used for the frame fence semaphore.
static constexpr uint32_t kSemaphoreContextChannel = PBKitPlusPlus::kNextContextChannel;
static constexpr uint32_t kSemaphoreObjectSize = 32;
// Upper bound on how long WaitForGPUIdle will spin before giving up and falling back to pb_busy().
static constexpr DWORD kFenceTimeoutMilliseconds = 2000;

#define MAX_FILE_PATH_SIZE 248
#define MAX_FILENAME_SIZE 42

TestHost::TestHost(uint32_t framebuffer_width, uint32_t framebuffer_height, uint32_t max_texture_width,
                   uint32_t max_texture_height, uint32_t max_texture_depth)
    : NV2AState(framebuffer_width, framebuffer_height, max_texture_width, max_texture_height, max_texture_depth) {
  LARGE_INTEGER frequency;
  QueryPerformanceFrequency(&frequency);
  perf_counter_frequency_ = static_cast<double>(frequency.QuadPart);
  QueryPerformanceCounter(&last_frame_time_);
}

void TestHost::EnsureFolderExists(const std::string &folder_path) {
  if (folder_path.length() > MAX_FILE_PATH_SIZE) {
    ASSERT(!"Folder Path is too long.");
  }

  char buffer[MAX_FILE_PATH_SIZE + 1] = {0};
  const char *path_start = folder_path.c_str();
  const char *slash = strchr(path_start, '\\');
  slash = strchr(slash + 1, '\\');

  while (slash) {
    strncpy(buffer, path_start, slash - path_start);
    if (!CreateDirectory(buffer, nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
      ASSERT(!"Failed to create output directory.");
    }

    slash = strchr(slash + 1, '\\');
  }

  // Handle case where there was no trailing slash.
  if (!CreateDirectory(path_start, nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
    ASSERT(!"Failed to create output directory.");
  }
}

void TestHost::PrepareDraw(uint32_t argb, uint32_t depth_value, uint8_t stencil_value) {
  NV2AState::PrepareDraw(argb, depth_value, stencil_value);
  if (sustained_active_ && sustained_awaiting_prepare_) {
    // NV2AState::PrepareDraw waits for vblank and then clears, so the frame's timed region starts here, roughly aligned
    // to the vblank that began the frame.
    QueryPerformanceCounter(&sustained_frame_start_);
    sustained_awaiting_prepare_ = false;
  }
}

void TestHost::FinishDraw(const std::string &suite_name, const std::string &test_name, const ProfileResults &results) {
  if (sustained_active_) {
    FinishSustainedFrame();
    return;
  }

  SetVertexShaderProgram(nullptr);
  SetXDKDefaultViewportAndFixedFunctionMatrices();

  SetBlend();
  SetFinalCombiner0Just(SRC_DIFFUSE);
  SetFinalCombiner1Just(SRC_DIFFUSE, true);

  Begin(TestHost::PRIMITIVE_QUADS);
  SetDiffuse(kResultsOverlayColor);
  SetScreenVertex(0.f, 0.f);
  SetScreenVertex(GetFramebufferWidthF(), 0.f);
  SetScreenVertex(GetFramebufferWidthF(), GetFramebufferHeightF());
  SetScreenVertex(0.f, GetFramebufferHeightF());
  End();

  auto micro_to_milliseconds = [](uint32_t microseconds) -> double {
    return static_cast<double>(microseconds) / 1000.0;
  };

  pb_print("%s::%s\n", suite_name.c_str(), test_name.c_str());
  if (save_results_) {
    pb_print("  %lu iterations\n", results.iterations);
    pb_print_with_floats("  Total: %f ms\n", micro_to_milliseconds(results.total_time_microseconds));
    pb_print_with_floats("  Avg: %f ms\n", micro_to_milliseconds(results.average_time_microseconds));
    pb_print_with_floats("  Min: %f ms\n", micro_to_milliseconds(results.minimum_time_microseconds));
    pb_print_with_floats("  Max: %f ms\n", micro_to_milliseconds(results.maximum_time_microseconds));
  } else {
    pb_print("Continuous mode: saving disabled\n");
    pb_print_with_floats("Average FPS: %f\n", average_frame_rate_);
    pb_print_with_floats("Average MSPF: %f\n", average_mspf_);
  }

  pb_draw_text_screen();

  NV2AState::FinishDraw();

  if (save_results_) {
    Logger::Log() << "  {" << std::endl;
    Logger::Log() << R"(    "name": ")" << suite_name << "::" << test_name << "\"," << std::endl;
    Logger::Log() << "    \"iterations\": " << results.iterations << "," << std::endl;
    Logger::Log() << "    \"total_us\": " << results.total_time_microseconds << "," << std::endl;
    Logger::Log() << "    \"average_us\": " << results.average_time_microseconds << "," << std::endl;
    Logger::Log() << "    \"min_us\": " << results.minimum_time_microseconds << "," << std::endl;
    Logger::Log() << "    \"max_us\": " << results.maximum_time_microseconds << "," << std::endl;
    Logger::Log() << "    \"raw_results\": [";
    std::string separator;
    for (auto val : results.raw_results) {
      Logger::Log() << separator << std::endl;
      separator = ",";
      Logger::Log() << "      " << val;
    }
    Logger::Log() << std::endl;
    Logger::Log() << "    ]" << std::endl;
    Logger::Log() << "  }," << std::endl;
  } else {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    auto delta =
        static_cast<float>(static_cast<double>(now.QuadPart - last_frame_time_.QuadPart) / perf_counter_frequency_);
    last_frame_time_ = now;

    frame_times_[current_frame_index_] = delta;
    current_frame_index_ = (current_frame_index_ + 1) % kFrameTimeWindow;

    if (!current_frame_index_) {
      float sum = 0;
      for (float frame_time : frame_times_) {
        sum += frame_time;
      }

      float avg_delta = sum / kFrameTimeWindow;
      average_mspf_ = avg_delta * 1000.f;
      average_frame_rate_ = 1.0f / avg_delta;
    }
  }
}

void TestHost::SetupFixedFunctionPassthrough() {
  SetVertexShaderProgram(nullptr);
  SetWindowClip(GetFramebufferWidth(), GetFramebufferHeight());
  SetViewportOffset(0, 0, 0, 0);
  SetViewportScale(1.f, 1.f, 1.f, 1.f);

  matrix4_t matrix;
  MatrixSetIdentity(matrix);
  SetFixedFunctionModelViewMatrix(matrix);
  SetFixedFunctionProjectionMatrix(matrix);
}

void pb_print_with_floats(const char *format, ...) {
  char buffer[512];

  va_list argList;
  va_start(argList, format);
  vsnprintf_(buffer, 512, format, argList);
  va_end(argList);

  char *str = buffer;
  while (*str != 0) {
    pb_print_char(*str++);
  }
}

[[nodiscard]] uint32_t TestHost::GetMicrosecondsSince(const LARGE_INTEGER &previous) const {
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);

  double delta = (double)(now.QuadPart - previous.QuadPart) / perf_counter_frequency_;
  return static_cast<uint32_t>(delta * 1000000.f);
  ;
}

void TestHost::EnsureSemaphoreInitialized() {
  if (semaphore_memory_) {
    return;
  }

  auto memory =
      static_cast<uint32_t *>(MmAllocateContiguousMemoryEx(kSemaphoreObjectSize, 0, MAXRAM, 0, PAGE_READWRITE));
  ASSERT(memory && "Failed to allocate semaphore memory");
  memset(memory, 0, kSemaphoreObjectSize);
  semaphore_memory_ = memory;

  pb_create_dma_ctx(kSemaphoreContextChannel, DMA_CLASS_3D, reinterpret_cast<DWORD>(memory), kSemaphoreObjectSize - 1,
                    &semaphore_dma_ctx_);
  pb_bind_channel(&semaphore_dma_ctx_);

  PBKitPlusPlus::Pushbuffer::Begin();
  PBKitPlusPlus::Pushbuffer::Push(NV097_SET_CONTEXT_DMA_SEMAPHORE, semaphore_dma_ctx_.ChannelID);
  PBKitPlusPlus::Pushbuffer::Push(NV097_SET_SEMAPHORE_OFFSET, 0);
  PBKitPlusPlus::Pushbuffer::End();
}

bool TestHost::WaitForGPUIdle() {
  EnsureSemaphoreInitialized();

  // Never release 0 so a freshly zeroed semaphore can't be mistaken for a completed fence.
  if (!++semaphore_value_) {
    ++semaphore_value_;
  }
  const uint32_t release_value = semaphore_value_;

  PBKitPlusPlus::Pushbuffer::Begin();
  PBKitPlusPlus::Pushbuffer::Push(NV097_BACK_END_WRITE_SEMAPHORE_RELEASE, release_value);
  // Do not flush: Pushbuffer::End(true) would poll pb_busy(), defeating the purpose of the fence.
  PBKitPlusPlus::Pushbuffer::End();
  // Ensure the release reaches the GPU even if submission is deferred.
  PBKitPlusPlus::Pushbuffer::Submit();

  const DWORD start = GetTickCount();
  while (*semaphore_memory_ != release_value) {
    if (GetTickCount() - start > kFenceTimeoutMilliseconds) {
      PrintMsg("WaitForGPUIdle: semaphore fence timed out (expected 0x%08lX got 0x%08lX), falling back to pb_busy\n",
               static_cast<unsigned long>(release_value), static_cast<unsigned long>(*semaphore_memory_));
      PBKitBusyWait();
      return false;
    }
  }
  return true;
}

void TestHost::BeginSustainedCapture(uint32_t expected_frames) {
  EnsureSemaphoreInitialized();
  sustained_fence_timeouts_ = 0;
  sustained_frames_.clear();
  sustained_frames_.reserve(expected_frames);
  sustained_active_ = true;
  sustained_recording_ = false;
  sustained_last_flip_.QuadPart = 0;
  sustained_last_vbl_ = pb_get_vbl_counter();
  BeginSustainedFrame();
}

void TestHost::BeginSustainedFrame() {
  QueryPerformanceCounter(&sustained_frame_start_);
  sustained_awaiting_prepare_ = true;
}

void TestHost::FinishSustainedFrame() {
  SustainedFrame frame{};
  frame.submit_us = GetMicrosecondsSince(sustained_frame_start_);

  // Waiting for the GPU to go idle is what exposes emulator-side PFIFO/PGRAPH costs to the guest.
  if (!WaitForGPUIdle()) {
    ++sustained_fence_timeouts_;
  }
  frame.busy_us = GetMicrosecondsSince(sustained_frame_start_);

  while (pb_finished()) {
    /* Not ready to swap yet */
  }

  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  if (sustained_last_flip_.QuadPart) {
    frame.frame_us = static_cast<uint32_t>(static_cast<double>(now.QuadPart - sustained_last_flip_.QuadPart) *
                                           1000000.0 / perf_counter_frequency_);
  }
  sustained_last_flip_ = now;

  auto vbl = pb_get_vbl_counter();
  frame.vblanks = vbl - sustained_last_vbl_;
  sustained_last_vbl_ = vbl;

  if (sustained_recording_) {
    sustained_frames_.push_back(frame);
  }
}

static uint32_t Percentile(std::vector<uint32_t> &sorted_values, uint32_t percentile) {
  if (sorted_values.empty()) {
    return 0;
  }
  auto index = (sorted_values.size() - 1) * percentile / 100;
  return sorted_values[index];
}

static void LogDistribution(const char *name, std::vector<uint32_t> values) {
  std::sort(values.begin(), values.end());
  uint64_t sum = 0;
  for (auto v : values) {
    sum += v;
  }
  auto mean = values.empty() ? 0 : static_cast<uint32_t>(sum / values.size());

  Logger::Log() << "    \"" << name << "\": {"
                << "\"mean\": " << mean << ", "
                << "\"min\": " << (values.empty() ? 0 : values.front()) << ", "
                << "\"p50\": " << Percentile(values, 50) << ", "
                << "\"p95\": " << Percentile(values, 95) << ", "
                << "\"p99\": " << Percentile(values, 99) << ", "
                << "\"max\": " << (values.empty() ? 0 : values.back()) << "}," << std::endl;
}

static void LogRawArray(const char *name, const std::vector<uint32_t> &values, bool trailing_comma) {
  // Logger::Log() reopens the log file on every call, so build the line in memory and write it once.
  std::string line = "    \"";
  line += name;
  line += "\": [";
  const char *separator = "";
  for (auto v : values) {
    line += separator;
    line += std::to_string(v);
    separator = ",";
  }
  line += "]";
  if (trailing_comma) {
    line += ",";
  }
  Logger::Log() << line << std::endl;
}

void TestHost::EndSustainedCapture(const std::string &suite_name, const std::string &test_name, uint32_t load,
                                   uint32_t warmup_frames) {
  sustained_active_ = false;
  sustained_recording_ = false;

  std::vector<uint32_t> submit;
  std::vector<uint32_t> busy;
  std::vector<uint32_t> frame;
  std::vector<uint32_t> vblanks;
  uint32_t missed_vblanks = 0;
  submit.reserve(sustained_frames_.size());
  busy.reserve(sustained_frames_.size());
  frame.reserve(sustained_frames_.size());
  vblanks.reserve(sustained_frames_.size());

  for (auto &f : sustained_frames_) {
    submit.push_back(f.submit_us);
    busy.push_back(f.busy_us);
    // The first frame of a capture has no previous flip to measure against.
    if (f.frame_us) {
      frame.push_back(f.frame_us);
    }
    vblanks.push_back(f.vblanks);
    if (f.vblanks > 1) {
      missed_vblanks += f.vblanks - 1;
    }
  }

  PrintMsg("  Sustained '%s::%s' load %lu: %lu frames, %lu missed vblanks\n", suite_name.c_str(), test_name.c_str(),
           static_cast<unsigned long>(load), static_cast<unsigned long>(sustained_frames_.size()),
           static_cast<unsigned long>(missed_vblanks));

  Logger::Log() << "  {" << std::endl;
  Logger::Log() << R"(    "name": ")" << suite_name << "::" << test_name << "\"," << std::endl;
  Logger::Log() << R"(    "mode": "sustained",)" << std::endl;
  Logger::Log() << "    \"load\": " << load << "," << std::endl;
  Logger::Log() << "    \"warmup_frames\": " << warmup_frames << "," << std::endl;
  Logger::Log() << "    \"frames\": " << sustained_frames_.size() << "," << std::endl;
  Logger::Log() << "    \"missed_vblanks\": " << missed_vblanks << "," << std::endl;
  Logger::Log() << "    \"fence_timeouts\": " << sustained_fence_timeouts_ << "," << std::endl;
  LogDistribution("submit_us", submit);
  LogDistribution("busy_us", busy);
  LogDistribution("frame_us", frame);
  LogRawArray("raw_busy_us", busy, true);
  LogRawArray("raw_vblanks", vblanks, false);
  Logger::Log() << "  }," << std::endl;

  sustained_frames_.clear();
  sustained_frames_.shrink_to_fit();
}
