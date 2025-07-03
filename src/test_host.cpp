#include "test_host.h"

#include <SDL.h>
#include <SDL_image.h>
#include <strings.h>

#include <cmath>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmacro-redefined"
#include <windows.h>
#pragma clang diagnostic pop

#include <texture_generator.h>
#include <xboxkrnl/xboxkrnl.h>

#include <algorithm>
#include <utility>

#include "debug_output.h"
#include "logger.h"
#include "nxdk_ext.h"
#include "pushbuffer.h"
#include "shaders/vertex_shader_program.h"
#include "vertex_buffer.h"
#include "xbox_math_d3d.h"
#include "xbox_math_matrix.h"
#include "xbox_math_types.h"
#include "xbox_math_util.h"

using namespace XboxMath;

static constexpr uint32_t kResultsOverlayColor = 0x88000000;

#define MAX_FILE_PATH_SIZE 248
#define MAX_FILENAME_SIZE 42

TestHost::TestHost(uint32_t framebuffer_width, uint32_t framebuffer_height, uint32_t max_texture_width,
                   uint32_t max_texture_height, uint32_t max_texture_depth)
    : NV2AState(framebuffer_width, framebuffer_height, max_texture_width, max_texture_height, max_texture_depth) {}

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

void TestHost::FinishDraw(const std::string &suite_name, const std::string &test_name,
                          const TestHost::ProfileResults &results) {
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
  pb_print("  %lu iterations\n", results.iterations);
  pb_print_with_floats("  Total: %f ms\n", micro_to_milliseconds(results.total_time_microseconds));
  pb_print_with_floats("  Avg: %f ms\n", micro_to_milliseconds(results.average_time_microseconds));
  pb_print_with_floats("  Min: %f ms\n", micro_to_milliseconds(results.minimum_time_microseconds));
  pb_print_with_floats("  Max: %f ms\n", micro_to_milliseconds(results.maximum_time_microseconds));

  pb_draw_text_screen();

  PBKitPlusPlus::NV2AState::FinishDraw();

  Logger::Log() << "  {" << std::endl;
  Logger::Log() << "    'name': '" << suite_name << "::" << test_name << "'," << std::endl;
  Logger::Log() << "    'iterations': " << results.iterations << "," << std::endl;
  Logger::Log() << "    'total_us': " << results.total_time_microseconds << "," << std::endl;
  Logger::Log() << "    'average_us': " << results.average_time_microseconds << "," << std::endl;
  Logger::Log() << "    'min_us': " << results.minimum_time_microseconds << "," << std::endl;
  Logger::Log() << "    'max_us': " << results.maximum_time_microseconds << std::endl;
  Logger::Log() << "  }," << std::endl;
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
