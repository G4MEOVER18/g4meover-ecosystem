// Host repro for the nfc_rfid_detector RogueMaster app port candidate.
//
// Models, 1:1 in control flow, the three real functions from
// targets/f7/furi_hal/furi_hal_nfc.c (mntm-012) that the app calls:
//   - furi_hal_nfc_acquire()   -- takes the SPI bus handle, then tries the NFC
//                                 mutex with a 100-tick timeout; on timeout it
//                                 releases the SPI bus again and returns
//                                 FuriHalNfcErrorBusy.
//   - furi_hal_nfc_release()   -- furi_check()s that the calling thread is the
//                                 mutex owner before releasing.
//   - furi_hal_nfc_field_detect_start() -- writes NFC chip registers via
//                                 furi_hal_spi_bus_tx(), which furi_check()s
//                                 that bus->current_handle == handle.
//
// furi_check() in real firmware halts/reboots the device. Here it is modeled
// as abort(), and each scenario runs in a forked child so a crash in one
// scenario does not stop the others from running.
//
// ALT is applications/external/nfc_rfid_detector/nfc_rfid_detector_app_i.c
// verbatim: it calls furi_hal_nfc_acquire() and discards the returned
// FuriHalNfcError. NEW checks the return value and skips touching NFC
// hardware (and the paired release()) when acquire failed.
//
// Build/run both variants:
//   cc -O0 -g -Wall -Wextra -fsanitize=address,undefined -o repro_alt repro.c && ./repro_alt
//   cc -O0 -g -Wall -Wextra -fsanitize=address,undefined -DFIXED -o repro_new repro.c && ./repro_new

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// ---- minimal furi_check ------------------------------------------------

static void my_check(bool cond, const char* what) {
    if(!cond) {
        fprintf(stderr, "furi_check FAILED: %s\n", what);
        abort();
    }
}
#define FURI_CHECK(cond) my_check((cond), #cond)

// ---- SPI bus model (targets/f7/furi_hal/furi_hal_spi.c) ---------------

typedef struct {
    const void* current_handle;
} SpiBus;

typedef struct {
    SpiBus* bus;
} SpiHandle;

static SpiBus g_spi_bus_nfc = {.current_handle = NULL};
static SpiHandle furi_hal_spi_bus_handle_nfc = {.bus = &g_spi_bus_nfc};

static void furi_hal_spi_acquire(const SpiHandle* handle) {
    FURI_CHECK(handle->bus->current_handle == NULL);
    handle->bus->current_handle = handle;
}

static void furi_hal_spi_release(const SpiHandle* handle) {
    FURI_CHECK(handle->bus->current_handle == handle);
    handle->bus->current_handle = NULL;
}

// Every st25r3916_* register access funnels through this, exactly like
// furi_hal_spi_bus_tx()/_trx() in the real HAL.
static void furi_hal_spi_bus_tx(const SpiHandle* handle) {
    FURI_CHECK(handle->bus->current_handle == handle);
}

// ---- NFC mutex model (targets/f7/furi_hal/furi_hal_nfc.c) -------------

typedef enum { FuriHalNfcErrorNone = 0, FuriHalNfcErrorBusy } FuriHalNfcError;

// 0 = free; nonzero = owning "thread" id. mutex_busy simulates another
// thread already holding furi_hal_nfc.mutex when this app starts (the
// concrete pre-condition the real FuriHalNfcErrorBusy path exists for).
static int g_nfc_mutex_owner = 0;
static bool g_mutex_busy_this_call = false;
#define THIS_THREAD 1

static FuriHalNfcError furi_hal_nfc_acquire(void) {
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_nfc);

    FuriHalNfcError error = FuriHalNfcErrorNone;
    if(g_mutex_busy_this_call) {
        furi_hal_spi_release(&furi_hal_spi_bus_handle_nfc);
        error = FuriHalNfcErrorBusy;
    } else {
        g_nfc_mutex_owner = THIS_THREAD;
    }
    return error;
}

static FuriHalNfcError furi_hal_nfc_release(void) {
    FURI_CHECK(g_nfc_mutex_owner == THIS_THREAD);
    g_nfc_mutex_owner = 0;
    furi_hal_spi_release(&furi_hal_spi_bus_handle_nfc);
    return FuriHalNfcErrorNone;
}

static FuriHalNfcError furi_hal_nfc_field_detect_start(void) {
    furi_hal_spi_bus_tx(&furi_hal_spi_bus_handle_nfc); // ST25R3916_REG_OP_CONTROL write
    furi_hal_spi_bus_tx(&furi_hal_spi_bus_handle_nfc); // ST25R3916_REG_MODE write
    return FuriHalNfcErrorNone;
}

static FuriHalNfcError furi_hal_nfc_field_detect_stop(void) {
    furi_hal_spi_bus_tx(&furi_hal_spi_bus_handle_nfc); // ST25R3916_REG_OP_CONTROL clear
    return FuriHalNfcErrorNone;
}

static bool furi_hal_nfc_field_is_present(void) {
    furi_hal_spi_bus_tx(&furi_hal_spi_bus_handle_nfc); // ST25R3916_REG_AUX_DISPLAY read
    return false;
}

// RFID side is independent hardware (no shared mutex/SPI-ownership gate in
// the real furi_hal_rfid.c) -- not part of this bug, kept as a no-op stub.
static void furi_hal_rfid_field_detect_start(void) {
}
static void furi_hal_rfid_field_detect_stop(void) {
}

// ---- app_i.c (applications/external/nfc_rfid_detector) ----------------

typedef struct {
#ifdef FIXED
    bool nfc_acquired;
#endif
    int placeholder;
} NfcRfidDetectorApp;

#ifndef FIXED
// ALT: verbatim nfc_rfid_detector_app_field_presence_start()/_stop()/_is_nfc()
static void app_field_presence_start(NfcRfidDetectorApp* app) {
    (void)app;
    furi_hal_rfid_field_detect_start();

    furi_hal_nfc_acquire(); // return value discarded, exactly as upstream
    furi_hal_nfc_field_detect_start();
}

static void app_field_presence_stop(NfcRfidDetectorApp* app) {
    (void)app;
    furi_hal_rfid_field_detect_stop();

    furi_hal_nfc_field_detect_stop();
    furi_hal_nfc_release();
}

static bool app_field_presence_is_nfc(NfcRfidDetectorApp* app) {
    (void)app;
    return furi_hal_nfc_field_is_present();
}
#else
// NEW: check the acquire() result; skip every further NFC-HAL/SPI touch
// (including the paired release()) when it failed.
static void app_field_presence_start(NfcRfidDetectorApp* app) {
    furi_hal_rfid_field_detect_start();

    app->nfc_acquired = (furi_hal_nfc_acquire() == FuriHalNfcErrorNone);
    if(app->nfc_acquired) {
        furi_hal_nfc_field_detect_start();
    }
}

static void app_field_presence_stop(NfcRfidDetectorApp* app) {
    furi_hal_rfid_field_detect_stop();

    if(app->nfc_acquired) {
        furi_hal_nfc_field_detect_stop();
        furi_hal_nfc_release();
        app->nfc_acquired = false;
    }
}

static bool app_field_presence_is_nfc(NfcRfidDetectorApp* app) {
    if(!app->nfc_acquired) return false;
    return furi_hal_nfc_field_is_present();
}
#endif

// ---- scenarios ----------------------------------------------------------

// Runs `fn` in a forked child so a crash (abort()) in one scenario does not
// take down the others. Returns true if the child ran to completion,
// false if it crashed (non-zero exit / killed by signal).
typedef void (*scenario_fn)(void);

static bool run_isolated(const char* name, scenario_fn fn) {
    fflush(stdout);
    pid_t pid = fork();
    if(pid == 0) {
        fn();
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    bool crashed = !(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    printf(
        "[%s] %s -> %s\n",
        crashed ? "CRASH" : "OK   ",
        name,
        crashed ? "furi_check() aborted (device would hard-fault/reboot)" : "no crash");
    return !crashed;
}

// Scenario A: normal case, mutex free on entry -- must never crash, in
// either variant.
static void scenario_mutex_free(void) {
    g_mutex_busy_this_call = false;
    g_nfc_mutex_owner = 0;
    g_spi_bus_nfc.current_handle = NULL;

    NfcRfidDetectorApp app = {0};
    app_field_presence_start(&app);
    for(int i = 0; i < 3; i++) {
        (void)app_field_presence_is_nfc(&app);
    }
    app_field_presence_stop(&app);
}

// Scenario B: furi_hal_nfc.mutex already held by another thread when this
// app's scene is entered (FuriHalNfcErrorBusy path) -- the one the upstream
// code has no handling for at all.
static void scenario_mutex_busy_on_enter(void) {
    g_mutex_busy_this_call = true;
    g_nfc_mutex_owner = 0;
    g_spi_bus_nfc.current_handle = NULL;

    NfcRfidDetectorApp app = {0};
    app_field_presence_start(&app);
    // Scene tick would call this every 100 ms while the field-presence view
    // is shown, regardless of whether start() above already crashed.
    (void)app_field_presence_is_nfc(&app);
    app_field_presence_stop(&app);
}

int main(void) {
#ifdef FIXED
    puts("=== NEW (return value of furi_hal_nfc_acquire() checked) ===");
#else
    puts("=== ALT (upstream nfc_rfid_detector_app_i.c, verbatim) ===");
#endif

    bool a_ok = run_isolated("mutex free on enter", scenario_mutex_free);
    bool b_ok = run_isolated("mutex busy on enter (FuriHalNfcErrorBusy)", scenario_mutex_busy_on_enter);

#ifndef FIXED
    if(a_ok && !b_ok) {
        puts("Confirmed: ALT crashes only when furi_hal_nfc_acquire() returns Busy,");
        puts("silently ignored by nfc_rfid_detector_app_field_presence_start().");
        return 0;
    }
    fprintf(stderr, "Unexpected result pattern for ALT.\n");
    return 1;
#else
    if(a_ok && b_ok) {
        puts("Confirmed: NEW survives both the normal case and the Busy case.");
        return 0;
    }
    fprintf(stderr, "Unexpected result pattern for NEW.\n");
    return 1;
#endif
}
