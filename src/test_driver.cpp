#include "test_driver.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wignored-attributes"
#include <hal/debug.h>
#pragma clang diagnostic pop

#include <pbkit/pbkit.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmacro-redefined"
#include <windows.h>
#pragma clang diagnostic pop

#include <fstream>

#include "debug_output.h"
#include "menu_item.h"

static constexpr auto kButtonRepeatMilliseconds = 150;

TestDriver::TestDriver(TestHost &host, const std::vector<std::shared_ptr<TestSuite>> &test_suites,
                       uint32_t framebuffer_width, uint32_t framebuffer_height, bool show_options_menu,
                       bool disable_autorun, bool autorun_immediately)
    : test_host_(host), test_suites_(test_suites) {
  auto on_run_all = [this]() { RunAllTestsNonInteractive(); };
  auto on_exit = [this]() { running_ = false; };
  root_menu_ = std::make_shared<MenuItemRoot>(test_suites, on_run_all, on_exit, framebuffer_width, framebuffer_height,
                                              disable_autorun, autorun_immediately);

  if (show_options_menu) {
    auto on_options_exit = [this]() { active_menu_ = root_menu_; };
    options_menu_ =
        std::make_shared<MenuItemOptions>(test_suites, on_options_exit, framebuffer_width, framebuffer_height);
    active_menu_ = options_menu_;
  } else {
    active_menu_ = root_menu_;
  }
}

TestDriver::~TestDriver() {
  for (auto handle : gamepads_) {
    if (handle) {
      SDL_GameControllerClose(handle);
    }
  }
}

void TestDriver::Run() {
  std::map<SDL_GameControllerButton, std::chrono::time_point<std::chrono::steady_clock>> button_repeat_map;

  while (running_) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_CONTROLLERDEVICEADDED:
          OnControllerAdded(event.cdevice);
          break;

        case SDL_CONTROLLERDEVICEREMOVED:
          OnControllerRemoved(event.cdevice);
          break;

        case SDL_CONTROLLERBUTTONDOWN: {
          auto now = std::chrono::high_resolution_clock::now();
          button_repeat_map[static_cast<SDL_GameControllerButton>(event.cbutton.button)] = now;
        } break;

        case SDL_CONTROLLERBUTTONUP:
          button_repeat_map.erase(static_cast<SDL_GameControllerButton>(event.cbutton.button));
          OnControllerButtonEvent(event.cbutton);
          break;

        default:
          break;
      }
    }

    auto now = std::chrono::high_resolution_clock::now();
    for (const auto &pair : button_repeat_map) {
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - pair.second).count();
      if (elapsed > kButtonRepeatMilliseconds) {
        OnButtonActivated(pair.first, true);
        button_repeat_map[pair.first] = std::chrono::high_resolution_clock::now();
      }
    }

    switch (MenuItemTest::GetRunMode()) {
      case MenuItemTest::RunMode::SINGLE_FRAME:
        active_menu_->SetBackgroundColor(0xFF114411);
        break;

      case MenuItemTest::RunMode::CONTINUOUS:
        active_menu_->SetBackgroundColor(0xFF551111);
        break;
    }

    active_menu_->Draw();

    if (test_host_.GetSaveResults()) {
      Sleep(10);
    }
  }
}

void TestDriver::RunAllTestsNonInteractive() {
  if (calibration_mode_) {
    RunCalibration();
    running_ = false;
    return;
  }

  for (auto &suite : test_suites_) {
    suite->Initialize();
    if (sustained_mode_) {
      for (const auto &test_name : suite->TestNames()) {
        suite->RunSustained(test_name, sustained_warmup_frames_, sustained_measure_frames_);
      }
    } else {
      suite->RunAll();
    }
    suite->Deinitialize();
  }
  running_ = false;
}

void TestDriver::OnControllerAdded(const SDL_ControllerDeviceEvent &event) {
  SDL_GameController *controller = SDL_GameControllerOpen(event.which);
  if (!controller) {
    debugPrint("Failed to handle controller add event.");
    debugPrint("%s", SDL_GetError());
    ShowDebugMessageAndExit();
    return;
  }

  auto player_index = SDL_GameControllerGetPlayerIndex(controller);

  if (player_index < 0 || player_index >= kMaxGamepads) {
    debugPrint("Player index %d is out of range (max %d).", player_index, kMaxGamepads);
    ShowDebugMessageAndExit();
    return;
  }

  gamepads_[player_index] = controller;
}

void TestDriver::OnControllerRemoved(const SDL_ControllerDeviceEvent &event) {
  SDL_GameController *controller = SDL_GameControllerFromInstanceID(event.which);

  for (auto i = 0; i < kMaxGamepads; ++i) {
    SDL_GameController *handle = gamepads_[i];
    if (!handle) {
      continue;
    }

    SDL_JoystickID id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(handle));
    if (id == event.which) {
      SDL_GameControllerClose(controller);
      gamepads_[i] = nullptr;
      return;
    }
  }

  // Handle case where the controller was never mapped for some reason.
  SDL_GameControllerClose(controller);
}

void TestDriver::OnControllerButtonEvent(const SDL_ControllerButtonEvent &event) {
  if (event.state != SDL_RELEASED) {
    return;
  }

  OnButtonActivated(static_cast<SDL_GameControllerButton>(event.button), false);
}

void TestDriver::OnButtonActivated(SDL_GameControllerButton button, bool is_repeat) {
  switch (button) {
    case SDL_CONTROLLER_BUTTON_BACK:
      OnBack(is_repeat);
      break;

    case SDL_CONTROLLER_BUTTON_START:
      OnStart(is_repeat);
      break;

    case SDL_CONTROLLER_BUTTON_DPAD_UP:
      OnUp(is_repeat);
      break;

    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
      OnDown(is_repeat);
      break;

    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
      OnLeft(is_repeat);
      break;

    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
      OnRight(is_repeat);
      break;

    case SDL_CONTROLLER_BUTTON_A:
      OnA(is_repeat);
      break;

    case SDL_CONTROLLER_BUTTON_B:
      OnB(is_repeat);
      break;

    case SDL_CONTROLLER_BUTTON_X:
      OnX(is_repeat);
      break;

    case SDL_CONTROLLER_BUTTON_Y:
      OnY(is_repeat);
      break;

    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
      OnBlack(is_repeat);
      break;

    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
      OnWhite(is_repeat);
      break;

    default:
      break;
  }
}

void TestDriver::ShowDebugMessageAndExit() {
  pb_show_debug_screen();
  Sleep(2000);
  running_ = false;
}

void TestDriver::OnBack(bool is_repeat) {
  if (is_repeat) {
    return;
  }
  active_menu_->Deactivate();
}

void TestDriver::OnStart(bool is_repeat) {
  if (is_repeat) {
    return;
  }
  active_menu_->Activate();
}

void TestDriver::OnBlack(bool is_repeat) {
  if (is_repeat) {
    return;
  }
  running_ = false;
}

void TestDriver::OnWhite(bool is_repeat) {}

void TestDriver::OnA(bool is_repeat) {
  if (is_repeat) {
    return;
  }
  active_menu_->Activate();
}

void TestDriver::OnB(bool is_repeat) {
  if (is_repeat) {
    return;
  }
  active_menu_->Deactivate();
}

void TestDriver::OnX(bool is_repeat) {
  if (is_repeat) {
    return;
  }
  active_menu_->ActivateCurrentSuite();
}

void TestDriver::OnY(bool is_repeat) {
  if (is_repeat) {
    return;
  }

  switch (MenuItemTest::GetRunMode()) {
    case MenuItemTest::RunMode::SINGLE_FRAME:
      MenuItemTest::SetRunMode(MenuItemTest::RunMode::CONTINUOUS);
      test_host_.SetSaveResults(false);
      break;

    case MenuItemTest::RunMode::CONTINUOUS:
      MenuItemTest::SetRunMode(MenuItemTest::RunMode::SINGLE_FRAME);
      test_host_.SetSaveResults(true);
  }
}

void TestDriver::OnUp(bool is_repeat) { active_menu_->CursorUp(is_repeat); }

void TestDriver::OnDown(bool is_repeat) { active_menu_->CursorDown(is_repeat); }

void TestDriver::OnLeft(bool is_repeat) { active_menu_->CursorLeft(is_repeat); }

void TestDriver::OnRight(bool is_repeat) { active_menu_->CursorRight(is_repeat); }

void TestDriver::RunCalibration() {
  struct Entry {
    std::string name;
    TestSuite::CalibrationResult result;
  };
  std::vector<Entry> entries;

  for (auto &suite : test_suites_) {
    suite->Initialize();
    for (const auto &test_name : suite->TestNames()) {
      entries.push_back({suite->Name() + "::" + test_name, suite->Calibrate(test_name, calibration_settings_)});
    }
    suite->Deinitialize();
  }

  DeleteFile(calibration_file_path_.c_str());
  std::ofstream out(calibration_file_path_.c_str());
  if (!out) {
    debugPrint("Failed to open %s\n", calibration_file_path_.c_str());
    return;
  }

  out << "{" << std::endl;
  out << "  \"hardware\": {" << std::endl;
  out << "    \"flags\": " << XboxHardwareInfo.Flags << "," << std::endl;
  out << "    \"devkit_kernel\": " << ((XboxHardwareInfo.Flags & XBOX_HW_FLAG_DEVKIT_KERNEL) ? "true" : "false") << ","
      << std::endl;
  out << "    \"gpu_revision\": " << static_cast<uint32_t>(XboxHardwareInfo.GpuRevision) << "," << std::endl;
  out << "    \"mcp_revision\": " << static_cast<uint32_t>(XboxHardwareInfo.McpRevision) << std::endl;
  out << "  }," << std::endl;
  out << "  \"calibration\": {" << std::endl;
  out << "    \"target_busy_p95_us\": " << calibration_settings_.target_busy_p95_us << "," << std::endl;
  out << "    \"warmup_frames\": " << calibration_settings_.warmup_frames << "," << std::endl;
  out << "    \"probe_frames\": " << calibration_settings_.probe_frames << "," << std::endl;
  out << "    \"confirm_frames\": " << calibration_settings_.confirm_frames << std::endl;
  out << "  }," << std::endl;

  // Start from the existing baseline so that tests which were filtered out of this run keep their loads, then replace
  // the loads of tests that met the target. Only loads that met the target are suitable for use in benchmark runs, so
  // a test that was recalibrated but failed keeps its previous load (its failure is reported in "results").
  auto loads = calibration_baseline_loads_;
  for (const auto &entry : entries) {
    if (entry.result.status == "ok" || entry.result.status == "hit_cap") {
      loads[entry.name] = entry.result.load;
    }
  }
  out << "  \"loads\": {";
  const char *separator = "";
  for (const auto &[name, load] : loads) {
    out << separator << std::endl << "    \"" << name << "\": " << load;
    separator = ",";
  }
  out << std::endl << "  }," << std::endl;

  out << "  \"results\": [";
  separator = "";
  for (const auto &entry : entries) {
    const auto &r = entry.result;
    out << separator << std::endl;
    out << "    {\"name\": \"" << entry.name << "\", \"status\": \"" << r.status << "\", \"load\": " << r.load
        << ", \"runs\": " << r.runs << ", \"busy_p50_us\": " << r.summary.busy_p50_us
        << ", \"busy_p95_us\": " << r.summary.busy_p95_us << ", \"missed_vblanks\": " << r.summary.missed_vblanks
        << ", \"fence_timeouts\": " << r.summary.fence_timeouts << ", \"max_resets\": " << r.summary.max_resets
        << ", \"history\": [";
    const char *run_separator = "";
    for (const auto &run : r.history) {
      out << run_separator << std::endl;
      out << "      {\"load\": " << run.load << ", \"kind\": \"" << (run.confirm ? "confirm" : "probe")
          << "\", \"passed\": " << (run.passed ? "true" : "false") << ", \"frames\": " << run.summary.frames
          << ", \"aborted\": " << (run.summary.aborted ? "true" : "false")
          << ", \"busy_p50_us\": " << run.summary.busy_p50_us << ", \"busy_p95_us\": " << run.summary.busy_p95_us
          << ", \"missed_vblanks\": " << run.summary.missed_vblanks
          << ", \"fence_timeouts\": " << run.summary.fence_timeouts << ", \"max_resets\": " << run.summary.max_resets
          << "}";
      run_separator = ",";
    }
    out << (r.history.empty() ? "" : "\n    ") << "]}";
    separator = ",";
  }
  out << std::endl << "  ]" << std::endl;
  out << "}" << std::endl;

  PrintMsg("Calibration written to %s\n", calibration_file_path_.c_str());
}
