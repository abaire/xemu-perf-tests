#ifndef XEMU_PERF_TESTS_PFIFO_CONTENTION_TESTS_H
#define XEMU_PERF_TESTS_PFIFO_CONTENTION_TESTS_H

#include <pbkit/pbkit.h>

#include <functional>
#include <string>

#include "test_suite.h"

/**
 * Scenarios that stress the interaction between the guest and the emulator's PFIFO / PGRAPH processing: how work is
 * submitted, CPU access to GPU-rendered memory, waits on GPU reports, and presenting at less than the display rate.
 * Each test is built from the same tiny triangles, so Control serves as the baseline for draw cost.
 *
 * In sustained mode each test's work per frame is scaled by its "load" knob.
 */
class PfifoContentionTests : public TestSuite {
 public:
  PfifoContentionTests(TestHost &host, std::string output_dir, const Config &config);

  void Initialize() override;

 private:
  void TestControl();
  void TestLongPushbuffer();
  void TestMethodChurn();
  void TestTinyKicks();
  void TestHalfRatePresent();
  void TestSurfaceReadback();
  void TestOcclusionSpin();
  void TestCpuFramebufferWrite();
  void TestFillBound();

  //! Runs `body` with the standard frame setup and reports results under `test_name`.
  void RunFrame(const char *test_name, const std::function<void()> &body);

  //! Draws a tiny triangle at a grid position derived from `index`.
  void DrawTinyTriangle(uint32_t index) const;

  //! Draws a tiny triangle at a grid position derived from `index` within a render target of the given size.
  void DrawTinyTriangle(uint32_t index, uint32_t target_width, uint32_t target_height) const;

  //! Memory written by NV097_GET_REPORT.
  void *report_memory_{nullptr};
  struct s_CtxDma report_dma_ctx_{};

  //! Counts the frames of HalfRatePresent, which alternates between presenting and not presenting.
  uint32_t half_rate_frame_{0};
};

#endif  // XEMU_PERF_TESTS_PFIFO_CONTENTION_TESTS_H
