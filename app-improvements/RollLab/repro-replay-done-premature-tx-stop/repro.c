// repro.c -- host-seitiges Reproduktionsprogramm fuer den RollLab-Fund
// "rlab_replay_done() beendet TX, bevor die letzte Flanke physisch gesendet wurde".
//
// Modelliert den dokumentierten Vertrag von subghz_devices_start_async_tx()/
// subghz_devices_is_async_complete_tx() als Mock-Treiber: die Callback-Schnittstelle ist
// Timer-/Double-Buffering-getrieben -- der Treiber ruft den TX-Callback fuer die NAECHSTE
// Dauer ab, WAEHREND die aktuelle Dauer noch physisch ausgegeben wird (sonst waere beim
// naechsten Timer-Interrupt keine neue Periode geladen -- Standardtechnik jedes
// Timer-getriebenen Bit-Bang-Senders). Dadurch liegt die Zahl der Callback-Aufrufe
// (= app->tx_pos) IMMER mindestens einen vollen Flanken-Zyklus vor der Zahl der tatsaechlich
// physisch abgeschlossenen Dauern.
//
// Verifiziert per offiziellem Referenzmuster (DarkFlippers/unleashed-firmware,
// applications/system/secplus_pin/secplus_pin_radio.c, secplus_pin_radio_tx()):
//   while(!subghz_devices_is_async_complete_tx(device) && (waited_ms < TX_TIMEOUT_MS)) ...
// -- der offizielle Code wartet AUSSCHLIESSLICH auf is_async_complete_tx() (plus
// Sicherheits-Timeout), nicht auf einen eigenen "Daten alle abgerufen"-Zaehler. Exakt dieser
// zusaetzliche, bei mir ODER-verknuepfte Zaehler-Check ist der hier reproduzierte Fund.
//
// Die beiden unter Test stehenden Funktionen sind woertlich aus rolllab_rf.c uebernommen
// (rlab_replay_tx_cb(), rlab_replay_done() ALT/NEU), nur mit einem Mock-Treiber statt der
// echten furi_hal_subghz-Anbindung.
//
// Bauen & ausfuehren:
//   gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -O0 -o repro_O0 repro.c && ./repro_O0
//   gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -O1 -o repro_O1 repro.c && ./repro_O1

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Minimal-Nachbildung der Flipper-/RollLab-Typen, nur was die Funktionen brauchen ---

typedef struct {
    bool     valid;    // false == level_duration_reset()
    bool     level;
    uint32_t duration;
} LevelDuration;

static LevelDuration level_duration_reset(void) {
    return (LevelDuration){.valid = false, .level = false, .duration = 0};
}
static LevelDuration level_duration_make(bool level, uint32_t duration) {
    return (LevelDuration){.valid = true, .level = level, .duration = duration};
}

#define RLAB_REF_BUF_SIZE 1024U

typedef struct {
    uint32_t buf[RLAB_REF_BUF_SIZE];
    size_t   count;
} RLabRefSignal;

typedef struct MockDevice MockDevice;

typedef struct {
    const MockDevice* device; // nur als Opaque-Handle fuer subghz_devices_is_async_complete_tx()
    RLabRefSignal     ref_sig;
    size_t            tx_pos;
} RollLabApp;

static volatile RollLabApp* s_replay_app = NULL;

// --- Woertlich aus rolllab_rf.c: TX-Callback ---
static LevelDuration rlab_replay_tx_cb(void* ctx) {
    (void)ctx;
    RollLabApp* app = (RollLabApp*)s_replay_app;
    if(!app) return level_duration_reset();

    const RLabRefSignal* sig = &app->ref_sig;
    size_t pos = app->tx_pos;
    if(pos >= sig->count) return level_duration_reset();

    app->tx_pos = pos + 1;
    uint32_t packed = sig->buf[pos];
    bool lv      = (packed & 0x80000000UL) != 0;
    uint32_t dur = packed & 0x7FFFFFFFUL;
    return level_duration_make(lv, dur > 0 ? dur : 100);
}

// --- ALT (unveraendert) ---
static bool rlab_replay_done_ALT(const RollLabApp* app, bool (*is_async_complete_tx)(const MockDevice*)) {
    return (app->tx_pos >= app->ref_sig.count) || is_async_complete_tx(app->device);
}

// --- NEU (gefixt) ---
static bool rlab_replay_done_NEU(const RollLabApp* app, bool (*is_async_complete_tx)(const MockDevice*)) {
    return is_async_complete_tx(app->device);
}

// ---------------------------------------------------------------------------------------
// Mock-Treiber: bildet den dokumentierten Double-Buffering-Vertrag von
// subghz_devices_start_async_tx()/subghz_devices_is_async_complete_tx() nach.
// ---------------------------------------------------------------------------------------
struct MockDevice {
    LevelDuration (*cb)(void*);
    void* ctx;
    LevelDuration armed;      // aktuell physisch ausgegebene Dauer
    bool          armed_valid;
    bool          complete;   // == subghz_devices_is_async_complete_tx()
    size_t        phys_complete_count; // wie viele Dauern WIRKLICH vollstaendig ausgegeben wurden
};

// subghz_devices_start_async_tx(): laedt sofort die erste Dauer vor (ein Callback-Aufruf),
// genau wie der echte Treiber vor Beginn der physischen Ausgabe bereits die erste Periode
// in sein Timer-Register laden muss.
static void mock_start_async_tx(MockDevice* dev, LevelDuration (*cb)(void*), void* ctx) {
    memset(dev, 0, sizeof(*dev));
    dev->cb = cb;
    dev->ctx = ctx;
    dev->armed = dev->cb(dev->ctx);
    dev->armed_valid = dev->armed.valid;
    dev->complete = !dev->armed_valid; // leerer Puffer: sofort "fertig"
}

// Ein Timer-Tick: die aktuell geladene Dauer ist jetzt physisch vollstaendig ausgegeben
// worden; danach wird (Double-Buffering) sofort die naechste Dauer per Callback
// vorab abgerufen, BEVOR diese neue Dauer physisch zu laufen beginnt.
static void mock_tick(MockDevice* dev) {
    if(dev->complete || !dev->armed_valid) return;
    dev->phys_complete_count++; // die gerade geladene Dauer ist jetzt WIRKLICH fertig
    LevelDuration next = dev->cb(dev->ctx);
    if(!next.valid) {
        dev->complete = true; // keine weitere Dauer mehr -- Treiber geht in Idle
        dev->armed_valid = false;
    } else {
        dev->armed = next; // ab jetzt physisch ausgegeben, wird erst beim naechsten Tick fertig
    }
}

static bool mock_is_async_complete_tx(const MockDevice* dev) { return dev->complete; }

static void mock_stop_async_tx(MockDevice* dev) { (void)dev; /* no-op im Modell: Zustand bleibt stehen, wie er beim Stop war */ }

// ---------------------------------------------------------------------------------------
// Test-Treiber: fuehrt die reale while-Schleife aus RLAB_REPLAYING/RLAB_PROBING nach:
//   rlab_replay_start(app); while(!abort && !rlab_replay_done(app)) { ...; tick(); }
//   rlab_replay_stop(app);
// und meldet, wie viele der N Flanken beim Stop tatsaechlich PHYSISCH fertig waren.
// ---------------------------------------------------------------------------------------
typedef bool (*DoneFn)(const RollLabApp*, bool (*)(const MockDevice*));

static size_t run_replay(RollLabApp* app, DoneFn done_fn, size_t max_ticks) {
    MockDevice dev;
    app->tx_pos = 0;
    s_replay_app = app;

    mock_start_async_tx(&dev, rlab_replay_tx_cb, NULL);
    app->device = &dev;

    size_t ticks = 0;
    while(!done_fn(app, mock_is_async_complete_tx) && ticks < max_ticks) {
        mock_tick(&dev);
        ticks++;
    }
    mock_stop_async_tx(&dev);
    s_replay_app = NULL;
    return dev.phys_complete_count;
}

static void fill_signal(RLabRefSignal* sig, size_t n) {
    sig->count = n;
    for(size_t i = 0; i < n; i++) {
        sig->buf[i] = (i % 2 ? 0x80000000UL : 0UL) | (300 + (uint32_t)i); // beliebige Testwerte
    }
}

static int checks_failed = 0;
#define CHECK(cond, msg) do { \
        if(!(cond)) { printf("  FAIL: %s\n", msg); checks_failed++; } \
        else printf("  ok:   %s\n", msg); \
    } while(0)

int main(void) {
    size_t ns[] = {1, 2, 16, 1024};
    for(size_t k = 0; k < sizeof(ns) / sizeof(ns[0]); k++) {
        size_t n = ns[k];
        printf("== N=%zu Flanken ==\n", n);

        RollLabApp app_alt = {0};
        fill_signal(&app_alt.ref_sig, n);
        size_t phys_alt = run_replay(&app_alt, rlab_replay_done_ALT, n + 10);
        printf("  ALT: Schleife endet nach tx_pos=%zu, physisch fertig=%zu von %zu\n",
               app_alt.tx_pos, phys_alt, n);

        RollLabApp app_neu = {0};
        fill_signal(&app_neu.ref_sig, n);
        size_t phys_neu = run_replay(&app_neu, rlab_replay_done_NEU, n + 10);
        printf("  NEU: Schleife endet nach tx_pos=%zu, physisch fertig=%zu von %zu\n",
               app_neu.tx_pos, phys_neu, n);

        char label[96];
        snprintf(label, sizeof(label),
                 "N=%zu: ALT stoppt TX, bevor alle %zu Flanken physisch fertig sind", n, n);
        CHECK(phys_alt == n - 1, label);

        snprintf(label, sizeof(label),
                 "N=%zu: NEU stoppt TX erst, wenn alle %zu Flanken physisch fertig sind", n, n);
        CHECK(phys_neu == n, label);

        printf("\n");
    }

    if(checks_failed == 0) {
        printf("ALL CHECKS PASSED\n");
        return 0;
    } else {
        printf("%d CHECK(S) FAILED\n", checks_failed);
        return 1;
    }
}
