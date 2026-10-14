/*
 * Host repro for the OTG-left-enabled-on-exit finding in Sobe (sacriphanius/sobe),
 * a Flipper Zero hidden-IR-camera detector.
 *
 * Models the exact lifecycle around sobe_worker_set_tx_output() as transcribed from
 * ir_worker.c and sobe.c. A mock furi_hal_power_* pair tracks the real HAL's single
 * global OTG-enable bit (the bq25896 boost converter is a single physical resource,
 * so one shared bool models it faithfully for this control-flow question).
 *
 * ALT  = sobe_app()'s real exit sequence (sobe_worker_stop(); sobe_analysis_free();
 *        sobe_worker_free(); ... no OTG teardown anywhere).
 * NEU  = same sequence with the additive fix (call sobe_worker_set_tx_output(worker,
 *        false) once, right after sobe_worker_stop(), before freeing the worker,
 *        mirroring the official applications/main/infrared/infrared_app.c exit path
 *        which calls infrared_set_tx_pin(internal) + infrared_enable_otg(false)
 *        right after view_dispatcher_run() returns).
 *
 * Built/run with: gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
 * at -O0 and -O1.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* ---- mock furi_hal_power -------------------------------------------------- */

static bool g_otg_enabled = false;

static bool mock_furi_hal_power_enable_otg(void) {
  g_otg_enabled = true;
  return true;
}

static void mock_furi_hal_power_disable_otg(void) {
  g_otg_enabled = false;
}

/* ---- mock furi_hal_infrared (only the bit relevant to this question) ----- */

typedef enum { TxPinInternal, TxPinExtPA7 } MockTxPin;
static MockTxPin g_tx_pin = TxPinInternal;

static void mock_furi_hal_infrared_set_tx_output(MockTxPin pin) { g_tx_pin = pin; }

/* ---- 1:1 transcription of ir_worker.c::sobe_worker_set_tx_output() ------- */

static void sobe_worker_set_tx_output(bool use_external) {
  if (use_external) {
    mock_furi_hal_power_enable_otg();
    mock_furi_hal_infrared_set_tx_output(TxPinExtPA7);
  } else {
    mock_furi_hal_power_disable_otg();
    mock_furi_hal_infrared_set_tx_output(TxPinInternal);
  }
}

/* ---- sobe_app() lifecycle, transcribed from sobe.c ------------------------ */

typedef struct {
  bool use_external_ir;
} SobeApp;

/* sobe_app_alloc() + the GPIO auto-detect block, abbreviated: the detector
 * itself is out of scope here, only its result (an external module WAS
 * found) matters for this question. */
static SobeApp sobe_app_alloc_with_detected_external(bool external_present) {
  SobeApp app;
  app.use_external_ir = external_present;
  return app;
}

/* sobe_app()'s startup sequence, 1:1: */
static SobeApp sobe_app_startup(bool external_present) {
  SobeApp app = sobe_app_alloc_with_detected_external(external_present);
  sobe_worker_set_tx_output(app.use_external_ir);
  return app;
}

/* sobe_app()'s exit sequence -- ALT: exactly what sobe.c does today. */
static void sobe_app_exit_ALT(SobeApp *app) {
  (void)app;
  /* sobe_analysis_stop(analysis); sobe_worker_stop(worker); */
  /* sobe_analysis_free(analysis); sobe_worker_free(worker); */
  /* ... no call that ever touches OTG or the tx pin again. */
}

/* sobe_app()'s exit sequence -- NEU: additive fix from sobe_c.patch. */
static void sobe_app_exit_NEU(SobeApp *app) {
  /* sobe_analysis_stop(analysis); sobe_worker_stop(worker); */
  if (app->use_external_ir) {
    sobe_worker_set_tx_output(false);
  }
  /* sobe_analysis_free(analysis); sobe_worker_free(worker); */
}

/* ---- scenarios ------------------------------------------------------------ */

static void reset_mocks(void) {
  g_otg_enabled = false;
  g_tx_pin = TxPinInternal;
}

static void run_scenario(const char *name, bool external_present,
                          void (*exit_fn)(SobeApp *), bool expect_otg_enabled_after_exit) {
  reset_mocks();
  SobeApp app = sobe_app_startup(external_present);
  bool otg_right_after_startup = g_otg_enabled;
  exit_fn(&app);
  bool otg_after_exit = g_otg_enabled;

  printf("%-42s external=%d otg_after_startup=%d otg_after_exit=%d (expected %d) -> %s\n",
         name, external_present, otg_right_after_startup, otg_after_exit,
         expect_otg_enabled_after_exit,
         (otg_after_exit == expect_otg_enabled_after_exit) ? "OK" : "MISMATCH");

  assert(otg_after_exit == expect_otg_enabled_after_exit);
}

int main(void) {
  /* Regression: no external module detected -- OTG was never touched, must
   * stay off after exit in both ALT and NEU. */
  run_scenario("ALT, no external module", false, sobe_app_exit_ALT, false);
  run_scenario("NEU, no external module", false, sobe_app_exit_NEU, false);

  /* The actual finding: external module detected at startup -> OTG enabled.
   * ALT leaves it enabled forever after the app exits; NEU turns it back
   * off, matching the official infrared_app's exit path. */
  run_scenario("ALT, external module present (the bug)", true, sobe_app_exit_ALT, true);
  run_scenario("NEU, external module present (fixed)", true, sobe_app_exit_NEU, false);

  printf("\nALL CHECKS PASSED\n");
  return 0;
}
