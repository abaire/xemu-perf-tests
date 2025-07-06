#ifndef XBOX
#error Must be built with nxdk
#endif

#include <SDL.h>
#include <SDL_image.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wignored-attributes"
#include <hal/debug.h>
#pragma clang diagnostic pop
#include <hal/fileio.h>
#include <hal/video.h>
#include <nxdk/mount.h>
#include <pbkit/pbkit.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmacro-redefined"
#include <windows.h>
#pragma clang diagnostic pop

#include "debug_output.h"
#include "logger.h"
#include "runtime_config.h"
#include "test_driver.h"
#include "test_host.h"
#include "tests/high_vertex_count_tests.h"

static constexpr const char* kLogFileName = "results.txt";

static const int kFramebufferWidth = 640;
static const int kFramebufferHeight = 480;
static const int kBitsPerPixel = 32;

static constexpr int kDelayOnFailureMilliseconds = 4000;

const UCHAR kSMCSlaveAddress = 0x20;
const UCHAR kSMCRegisterPower = 0x02;
const UCHAR kSMCPowerShutdown = 0x80;

static bool EnsureDriveMounted(char drive_letter);
static bool LoadConfig(RuntimeConfig& config, std::vector<std::string>& errors);
static void RunTests(RuntimeConfig& config, TestHost& host, std::vector<std::shared_ptr<TestSuite>>& test_suites);
static void RegisterSuites(TestHost& host, RuntimeConfig& config, std::vector<std::shared_ptr<TestSuite>>& test_suites,
                           const std::string& output_directory);
static void Shutdown();

extern "C" __cdecl int automount_d_drive(void);

int main() {
  automount_d_drive();
  debugPrint("Set video mode\n");
  if (!XVideoSetMode(kFramebufferWidth, kFramebufferHeight, kBitsPerPixel, REFRESH_DEFAULT)) {
    debugPrint("Failed to set video mode\n");
    Sleep(kDelayOnFailureMilliseconds);
    return 1;
  }

  int status = pb_init();
  if (status) {
    debugPrint("pb_init Error %d\n", status);
    Sleep(kDelayOnFailureMilliseconds);
    return 1;
  }

  debugPrint("Initializing...\n");
  pb_show_debug_screen();

  if (SDL_Init(SDL_INIT_GAMECONTROLLER)) {
    debugPrint("Failed to initialize SDL_GAMECONTROLLER.\n");
    debugPrint("%s\n", SDL_GetError());
    pb_show_debug_screen();
    Sleep(kDelayOnFailureMilliseconds);
    return 1;
  }

  if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
    debugPrint("Failed to initialize SDL_image PNG mode.\n");
    pb_show_debug_screen();
    Sleep(kDelayOnFailureMilliseconds);
    pb_kill();
    return 1;
  }

  RuntimeConfig config;
  {
    std::vector<std::string> errors;
    if (!LoadConfig(config, errors)) {
      debugPrint("Failed to load config, using default values.\n");
      for (auto& err : errors) {
        debugPrint("%s\n", err.c_str());
      }
      pb_show_debug_screen();
    }
  }

  if (!EnsureDriveMounted(config.output_directory_path().front())) {
    debugPrint("Failed to mount %s, please make sure output directory is on a writable drive.\n",
               config.output_directory_path().c_str());
    pb_show_debug_screen();
    Sleep(kDelayOnFailureMilliseconds);
    pb_kill();
    return 1;
  };

  TestHost::EnsureFolderExists(config.output_directory_path());

  std::vector<std::shared_ptr<TestSuite>> test_suites;
  TestHost host(kFramebufferWidth, kFramebufferHeight);
  RegisterSuites(host, config, test_suites, config.output_directory_path());

  {
    std::vector<std::string> errors;
    if (!config.ApplyConfig(test_suites, errors)) {
      debugClearScreen();
      debugPrint("Failed to apply runtime config:\n");
      for (auto& err : errors) {
        debugPrint("%s\n", err.c_str());
      }
      Sleep(kDelayOnFailureMilliseconds);
      pb_kill();
      return 1;
    }
  }

  pb_show_front_screen();
  debugClearScreen();
  RunTests(config, host, test_suites);

  pb_kill();
  return 0;
}

static bool EnsureDriveMounted(char drive_letter) {
  if (nxIsDriveMounted(drive_letter)) {
    return true;
  }

  char dos_path[4] = "x:\\";
  dos_path[0] = drive_letter;
  char device_path[256] = {0};
  if (XConvertDOSFilenameToXBOX(dos_path, device_path) != STATUS_SUCCESS) {
    return false;
  }

  if (!strstr(device_path, R"(\Device\Harddisk0\Partition)")) {
    return false;
  }
  device_path[28] = 0;

  return nxMountDrive(drive_letter, device_path);
}

static bool LoadConfig(RuntimeConfig& config, std::vector<std::string>& errors) {
#ifdef RUNTIME_CONFIG_PATH
  if (!EnsureDriveMounted(RUNTIME_CONFIG_PATH[0])) {
    debugPrint("Ignoring missing config at %s\n", RUNTIME_CONFIG_PATH);
  } else {
    if (config.LoadConfig(RUNTIME_CONFIG_PATH, errors)) {
      return true;
    } else {
      debugPrint("Failed to load config at %s\n", RUNTIME_CONFIG_PATH);
    }
  }
#endif

  return config.LoadConfig("d:\\xemu_perf_tests_config.json", errors);
}

static void Shutdown() {
  // TODO: HalInitiateShutdown doesn't seem to cause Xemu to actually close.
  // This never sends the SMC command indicating that a shutdown should occur (at least, it never makes it to
  // `smc_write_data` to be processed).
  // HalInitiateShutdown();

  HalWriteSMBusValue(kSMCSlaveAddress, kSMCRegisterPower, FALSE, kSMCPowerShutdown);
  while (true) {
    Sleep(30000);
  }
}

static void RunTests(RuntimeConfig& config, TestHost& host, std::vector<std::shared_ptr<TestSuite>>& test_suites) {
  std::string log_file = config.output_directory_path() + "\\" + kLogFileName;
  DeleteFile(log_file.c_str());
  Logger::Initialize(log_file, true);

  TestDriver driver(host, test_suites, kFramebufferWidth, kFramebufferHeight, false, config.disable_autorun(),
                    config.enable_autorun_immediately());

  Logger::Log() << "Starting tests." << std::endl;
  Logger::Log() << "[" << std::endl;
  driver.Run();
  Logger::Log() << "]" << std::endl;
  PrintMsg("Test loop completed normally\n");
  Logger::Log() << "Testing completed normally, closing log." << std::endl;
  Logger::Log().close();

  if (config.enable_shutdown_on_completion()) {
    debugPrint("Results written to %s\n\nShutting down in 10 seconds...\n", config.output_directory_path().c_str());
    pb_show_debug_screen();
    Sleep(10000);

    Shutdown();
  } else {
    debugPrint("Results written to %s\n\nRebooting in 10 seconds...\n", config.output_directory_path().c_str());
    pb_show_debug_screen();
    Sleep(10000);
  }
}

static void RegisterSuites(TestHost& host, RuntimeConfig& runtime_config,
                           std::vector<std::shared_ptr<TestSuite>>& test_suites, const std::string& output_directory) {
  auto config = TestSuite::Config{};

#define REG_TEST(CLASS_NAME)                                                   \
  {                                                                            \
    auto suite = std::make_shared<CLASS_NAME>(host, output_directory, config); \
    test_suites.push_back(suite);                                              \
  }

  REG_TEST(HighVertexCountTests)

#undef REG_TEST
}
