xemu-perf-tests
====

Provides tests intended to be used to detect performance improvements/degradation in
the [xemu](xemu.app) project.

# Usage

Tests will be executed automatically if no gamepad input is given within an initial timeout.

Individual tests may be executed via the menu.

# Controls

DPAD:

* Up - Move the menu cursor up. Inside a test, go to the previous test in the active suite.
* Down - Move the menu cursor down. Inside a test, go to the previous test in the active suite.
* Left - Move the menu cursor up by half a page.
* Right - Move the menu cursor down by half a page.
* A - Enter a submenu or test. Inside a test, re-run the test.
* B - Go up one menu or leave a test. If pressed on the root menu, exit the application.
* X - Run all tests for the current suite.
* Y - Toggle between one shot and continuous test mode. In continuous mode, tests are designed to run near the edge of
  60 FPS on 1.0 Xbox devkit hardware, provided that the application is built in release mode and XBDM notifications are
  not attached.
* Start - Enter a submenu or test.
* Back - Go up one menu or leave a test. If pressed on the root menu, exit the application.
* Black - Exit the application.

# Test configuration

Behavior can optionally be determined via a JSON configuration file loaded from the XBOX.

```json
{
  "settings": {
    "disable_autorun": false,
    "enable_autorun_immediately": false,
    "enable_shutdown_on_completion": false,
    "skip_tests_by_default": false,
    "delay_milliseconds_between_tests": 0,
    "output_directory_path": "e:/xemu_perf_tests",
    "run_mode": "single",
    "warmup_frames": 30,
    "measure_frames": 240,
    "calibration_probe_frames": 120,
    "calibration_target_busy_p95_us": 15000
  }
}
```

In the default release build, the program will look for this file in the `output_directory_path` (
`e:/xemu_perf_tests/xemu_perf_tests_config.json`) and `d:\xemu_perf_tests_config.json`, taking whichever is found
first.

When building from source, the `sample-config.json` file in the `resources` directory can be copied to
`resources/xemu_perf_tests_config.json` and modified in order to change the default behavior of the final xiso.

#### Sustained mode

Setting `"run_mode": "sustained"` makes the non-interactive "run all" path run each test in continuous (60 FPS budget)
mode. Each test runs for `warmup_frames` unrecorded frames and then `measure_frames` recorded frames. Each test writes
one record to `results.txt` with:

* `busy_us`: time from the start of the frame (just after the vblank wait and clear in `PrepareDraw`) until the GPU is
  idle. This is the primary metric. GPU completion is detected with a `NV097_BACK_END_WRITE_SEMAPHORE_RELEASE` fence
  that the GPU writes to RAM, rather than by polling GPU registers with `pb_busy()`. Under xemu, register polling is
  MMIO that contends for the same locks the benchmarks measure. It is not quantized by vsync, so improvements show up
  even when they don't change the frame rate.
* `submit_us`: time from the start of the frame until the test's frame body returns. This includes `reset_wait_us`,
  and anything else the body does besides pushing, such as spinning on reports, reading surfaces or writing the
  framebuffer.
* `reset_wait_us`: the part of `submit_us` spent in pushbuffer resets. The pushbuffer is reset whenever it fills up,
  and each reset waits for the GPU to finish everything submitted so far.
* `submit_minus_reset_us`: `submit_us` minus `reset_wait_us`. For tests that only push commands this is the CPU time
  spent building the pushbuffer, but for the others it also includes the work listed under `submit_us`.
* `resets`: the number of pushbuffer resets per frame.

Most tests submit work to the GPU while they are still pushing more, so the GPU runs in parallel with the CPU.
`busy_us - submit_us` is therefore only the time the GPU needs to finish after the body returns, not the GPU's total
work for the frame. When it is small, the GPU kept up with submission.
* `frame_us`: time from the end of one recorded frame to the end of the next. For tests that skip recording some frames
  (`HalfRatePresent`), this spans the skipped frames too.
* `missed_vblanks`: the number of extra vblanks that elapsed during measured frames, including frames that were
  skipped from recording. Each frame is expected to take exactly one vblank.
* `fence_timeouts`: the number of frames whose semaphore fence did not complete within 2 seconds. The wait falls back
  to `pb_busy()` for those frames. This should always be 0.
* the raw per-frame `submit_us`, `reset_wait_us`, `busy_us` and vblank counts.

The distributions report mean, min, p50, p95, p99 and max, all in microseconds.

A test can expose a work-scaling knob through `GetLoad(test_name, default)`, so it can be tuned to the 60 FPS budget on
hardware without recompiling. Set the knob in a top-level `"loads"` object, keyed by `"Suite::Test"`. It is kept
separate from `"test_suites"` so that tools that rewrite the test filter don't discard it:

```json
"loads": {
  "BusyPfifo::PFIFOSaturation": 7
}
```

#### Calibration mode

Setting `"run_mode": "calibrate"` makes the non-interactive "run all" path search for each test's load on the
hardware it runs on. A load passes if busy p95 is at most `calibration_target_busy_p95_us` (default 15000, 90% of a
60Hz frame) with no missed vblanks and no fence timeouts.

For each test, the search works like this:

1. Probe the test's default load for `calibration_probe_frames` measured frames (default 120), after `warmup_frames`.
2. Double the load until a probe fails, or halve it until a probe passes.
3. Bisect between the passing and failing loads to within about 2%.
4. Confirm the result with a full `measure_frames` run, stepping the load down if that run fails.

The results are written to `calibration.json` in the output directory. Each test gets an entry in `"results"`, whose
`status` is one of:

* `ok`: the test was calibrated.
* `no_load`: the test has no load knob.
* `over_budget`: the test fails even at a load of 1.
* `hit_cap`: the test still passes at the largest load searched.
* `unconfirmed`: the search found a passing load, but the confirmation runs failed even after stepping the load
  down. This usually means the measurements are too noisy. `load` is the last load confirmation tried.
* `excluded`: the test has a load knob but opts out of calibration (e.g. `PushCost`, whose tests are compared at equal
  loads).

Each result also reports `max_resets`, the most pushbuffer resets in any frame of the final run. It should be 0. A
non-zero value means the load no longer fits in the pushbuffer (`kPushbufferBytes` in `main.cpp`), so frames are
interrupted by waits for the GPU to go idle; the pushbuffer should be enlarged and the test recalibrated.

The `"loads"` object starts from the existing baseline (see below), and replaces the load of every test whose status is
`ok` or `hit_cap`. Tests that were filtered out of the run, or failed to calibrate, keep their previous load.

A run that fails only because of missed vblanks, while its busy p95 is within the target, is retried once at the same load, since a one-off stall can cause it. Each result's `history` lists every run in order,
including retries, with its load, kind (`probe` or `confirm`), whether it passed, and its summary.

Each run stops early, and fails, once two frames in a row (not counting the first) are each busy for more than four
times the target, so loads far over budget don't take long to rule out. A single stalled frame does not stop a run.
Runs that stop early are not retried. Only the confirmation runs are written to `results.txt`, each
with a `"calibration"` field of `"confirmed"` or `"rejected"`. A record whose run stopped early has `"aborted": true`.

Calibration should only be performed on hardware. Results from xemu depend on the host and are not meaningful
baselines.

#### Calibration baseline

[resources/calibration.json](resources/calibration.json) is bundled with the xiso and loaded at startup. Its `"loads"`
object provides each test's default load, so benchmark runs need no config to run at the hardware baseline. Any
`"loads"` in the runtime config override it, and tests missing from it fall back to their uncalibrated placeholder
loads. Calibrate mode starts each test's search from its baseline load.

To recalibrate, run calibrate mode on hardware and replace `resources/calibration.json` with the `calibration.json` it
writes.

#### Filtering test suites/cases

The `"test_suites"` section may be used to filter the set of tests.

For example, suppose the program contains four test suites: `Default suite`, `Unlisted suite`, `Skipped suite`, and
`Suite with skipped test`. Each test suite contains two tests, `TestOne` and `TestTwo`. Without any filtering, the menu
tree would be something like:

```
Run all and exit
Default suite
  TestOne
  TestTwo
Unlisted suite
  TestOne
  TestTwo
Skipped suite
  TestOne
  TestTwo
Suite with skipped test
  TestOne
  TestTwo
```

___Note___: The names used within `"test_suites"` and its children are the same as the names that appear in the
`xemu_perf_tests` menu.

By default, any descendant of the `"test_suites"` object that contains a `"skipped": true` will be omitted.

For example, the following config will disable all tests except

```json
{
  "settings": {},
  "test_suites": {
    "Skipped suite": {
      "skipped": true,
      "TestOne": {}
    },
    "Suite with skipped test": {
      "TestTwo": {
        "skipped": true
      }
    },
    "Default suite": {
      "TestOne": {}
    }
  }
}
```

Resulting in

```
Run all and exit
Default suite
  TestOne
  TestTwo
Unlisted suite
  TestOne
  TestTwo
Suite with skipped test
  TestOne
```

This behavior may be modified via the `"skip_tests_by_default"` setting, allowing only entries with an explicit
`"skipped": false` to be retained. For example, the following config will disable all tests except the `TestOne` case
under `Default suite`.

```json
{
  "settings": {
    "skip_tests_by_default": true
  },
  "test_suites": {
    "Default suite": {
      "TestOne": {
        "skipped": true
      }
    }
  }
}
```

# Building

## Prerequisites

This project uses https://pypi.org/project/nv2a-vsh to build vertex shaders. It may be
installed by running `pip install -r requirements.txt` from this directory.

## CLion

### Building

Under Settings > Build, Execution, Deployment > CMake

- Provide the path to the nxdk toolchain file under CMake Options

  `-DCMAKE_TOOLCHAIN_FILE=$CMakeProjectDir$/third_party/nxdk/share/toolchain-nxdk.cmake`

- Set the `NXDK_DIR` environment variable to the absolute path of the `third_party/nxdk` subdirectory.

- Optionally modify the `PATH` variable to make sure the path to your chosen version of Clang comes first.

### Debugging

#### Using [xemu](xemu.app)

1. Create a new `Embedded GDB Server` target
1. Set the Target to the `<your project name>_xiso` target
1. Set the Executable to the `<your project name>` binary (it should be the only thing in the dropdown)
1. Set `Upload executable` to `Never`
1. Set `'target remote' args` to `127.0.0.1:1234`
1. Set `GDB Server` to the path to the xemu binary
1. Set `GDB Server args` to
   `-s -S -dvd_path "$CMakeCurrentBuildDir$/xiso/xemu-perf-tests_xiso/xemu-perf-tests_xiso.iso"` (the `-S` is
   optional and will cause xemu to wait for the debugger to connnect)
1. Under `Advanced GDB Server Options`
1. Set "Working directory" to `$ProjectFileDir$`
1. On macOS, set "Environment variables"
   to
   `DYLD_FALLBACK_LIBRARY_PATH=/<the full path to your xemu.app bundle>/Contents/Libraries/<the architecture for your platform, e.g., arm64>`
1. Set "Reset command" to `Never`

#### Using [xbdm_gdb_bridge](https://github.com/abaire/xbdm_gdb_bridge) and a devkit/XBDM-enabled Xbox

1. Create a new `Embedded GDB Server` target
1. Set the Target to the `<your project name>_xiso` target
1. Set the Executable to the `<your project name>` binary (it should be the only thing in the dropdown)
1. Set `Upload executable` to `Never`
1. Set `'target remote' args` to `127.0.0.1:1999`
1. Set `GDB Server` to the path to the xbdm_gdb_bridge binary (e.g., `<some_path>/xbdm_gdb_bridge`)
1. Set `GDB Server args` to `<your_xbox_ip> -v3 -s -- gdb :1999 e:\$CMakeCurrentTargetName$`
1. Under `Advanced GDB Server Options`, set `Reset command` to `Never`

To perform automatic deployment after builds:

1. Under the `Before launch` section, click the `+` button and add a new `Run external tool` entry.
1. Set the `Description` field to something like `Sync program and resources to the XBOX`
1. Set the `Program` field to the path to the xbdm_gdb_bridge binary (same as above)
1. Set `Arguments` to
   `<your_xbox_ip> -- mkdir e:\$CMakeCurrentTargetName$ && %syncdir $CMakeCurrentBuildDir$/xbe/xbe_file e:\$CMakeCurrentTargetName$ -f`
1. Set `Working directory` to `$ProjectFileDir$`

## Adding new tests

The `utils` directory has a script that may be used to generate a skeletal test suite.

### Documentation

Autogenerated documentation may be accessed
at [https://abaire.github.io/xemu-perf-tests](https://abaire.github.io/xemu-perf-tests/)

Some special Doxygen comments are used to describe test results on the website:

1. A detailed description attached to the `TestSuite` subclass (see `high_vertex_count_tests.h`)
2. Ideally a list of `@tc` custom commands in the documentation for the constructor method, explaining what each test is
   doing and/or is expected to display (see `high_vertex_count_tests.cpp`).

---

**NOTE:** For tests whose class name is not equal to the lower case version of the `suite_name` string passed to the
`TestSuite` constructor, a `kSuiteName` `private static constexpr const char *` should be created, containing the
`suite_name` value passed to the `TestSuite`. The documentation generation script will pick this up to allow downstream
scripts to discover the test metadata.

---

### Using nv2a log events from [xemu](https://xemu.app/)

1. Enable tracing of nv2a log events as normal (see xemu documentation) and exercise the event of interest within the
   game.
1. Duplicate an existing test as a skeleton.
1. Add the duplicated test to `CMakeLists.txt` and `main.cpp` (please preserve alphabetical ordering if possible).
1. Use [nv2a_to_pbkit](https://github.com/abaire/nv2a_to_pbkit) to get a rough set of pbkit invocations duplicating the
   behavior from the log. Take the interesting portions of the converted output and put them into the body of the test.
   You may wish to utilize some of the helper methods from `TestHost`   and similar classes rather than using the raw
   output to improve readability.

### Writing nv2a vertex shaders in assembly

See [the README in the nv2a_vsh_asm repository](https://github.com/abaire/nv2a_vsh_asm) for an overview.

`*.vsh` files are assembed via the `generate_nv2a_vshinc_files` function in CMakeLists.txt. Each vsh file produces a
corresponding `.vshinc` file that contains a C-style list of 32-bit integers containing the vertex shader operations.
This file is intended to be included from test source files to initialize a constant array which may then be used to
populate a `VertexShaderProgram` via `SetShader`.

For example:

```c++
// Note that the VertexShaderProgram does not copy this data, so it is important that it remain in scope throughout
// the use of the VertexShaderProgram object.
static const uint32_t kPassthroughVsh[] = {
#include "passthrough.vshinc"
};

// ...

  auto shader = std::make_shared<VertexShaderProgram>();
  shader->SetShader(kPassthroughVsh, sizeof(kPassthroughVsh));
  host_.SetVertexShaderProgram(shader);

```

Uniform values may then be set via the `VertexShaderProgram::SetUniform*` series of functions. By default, the 0th
uniform will be available in the shader as `c96`.

For example, a shader that takes a 4-element vertex might have the code:

```asm
#vertex vector 96
```

which would be populated via

```c++
  shader->shader->SetUniformF(0, -1.f, 1.5f, 0.f, 1.f);
```
