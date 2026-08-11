# RollForge — Puffer-Reset-Bug in der interlaced Jam+RX-State-Machine (RollJam-Modul)

## Was
RollForge bündelt RollJam als eigenständiges, nicht-blockierendes Tick-Modul
(`module_rolljam.c`) mit einer Zustandsmaschine `RJ_P1_JAM → RJ_P1_RX → RJ_P1_JAM → ...`
statt der blockierenden Schleife der eigenständigen RollJam-App. Das Timing ist
identisch übernommen (180 ms Jam / 80 ms RX, `RJ_JAM_MS`/`RJ_RX_MS`), und damit auch
derselbe Grundfehler, den ich bereits in der eigenständigen RollJam-App gefunden und
dort per Pause/Resume behoben habe: Diese Zustandsmaschine trägt den Fix bisher nicht.

Jedes Mal, wenn `rj_tick()` von `RJ_P1_JAM` zurück nach `RJ_P1_RX` wechselt (jeder neue
80-ms-RX-Abschnitt innerhalb derselben Phase), ruft sie `rj_cap_start(app, &m->sig_a)`
auf:

```c
// module_rolljam.c:160-166 (analog RJ_P2_JAM -> RJ_P2_RX, sig_b)
} else if(now - m->step_start_ms >= RJ_JAM_MS) {
    rj_jam_stop(app);
    rj_cap_start(app, &m->sig_a);   // <-- bei JEDEM RX-Fenster erneut
    m->step_start_ms = now;
    m->phase = RJ_P1_RX;
    snprintf(m->status, sizeof(m->status), "Ph1 RX...");
}
```

`rj_cap_start()` (`module_rolljam.c:69-75`) setzt dabei unbedingt `sig->count = 0;
sig->ready = false;` — genau wie im eigenständigen RollJam vermischt diese eine
Funktion zwei verschiedene Absichten: "neue Aufnahme in dieser Phase beginnen"
(einmalig richtig) und "Lauschen nach einer Jam-Pause innerhalb derselben Phase
fortsetzen" (das eigentliche Ziel der interlaced-Technik). Der RX-Fenster-Timeout-Pfad
verschärft das zusätzlich, indem er in beiden Fällen (Fortsetzung *und* endgültiger
Phasen-Timeout) dieselbe `rj_cap_stop()` aufruft, die `s_cap = NULL` setzt:

```c
// module_rolljam.c:170-188 (RJ_P1_RX, analog RJ_P2_RX)
case RJ_P1_RX:
    if(m->sig_a.ready && m->sig_a.count >= RF_MIN_EDGES) {
        rj_cap_stop(app);
        ...
        m->phase = RJ_CAPTURED;
    } else if(now - m->step_start_ms >= RJ_RX_MS) {
        rj_cap_stop(app);                              // <-- immer, auch bei Fortsetzung
        if(now - m->phase_start_ms >= RF_TIMEOUT_MS) {
            m->phase = RJ_IDLE;
        } else {
            rj_jam_start(app);
            m->step_start_ms = now;
            m->phase = RJ_P1_JAM;                       // zurück zu JAM, naechster RX-Eintritt resettet
        }
    }
    break;
```

Ein reales Keyfob-OOK-Signal überschreitet ein einzelnes 80-ms-Fenster häufig
(Präambel + mehrere Frame-Wiederholungen liegen bei vielen Fahrzeugsystemen im Bereich
von 100–300+ ms) — genau dafür ist die interlaced Jam+RX-Technik gedacht: Fragmente aus
mehreren RX-Fenstern sollen sich innerhalb einer Phase zu einem vollständigen Frame
zusammensetzen. Mit dem aktuellen Reset-Verhalten zählt aber effektiv nur das *letzte*
RX-Fenster vor Phasenende — alles, was in früheren Fenstern derselben Phase bereits
empfangen wurde, ist beim nächsten `rj_cap_start()`-Aufruf weg. `sig_a.count`/
`sig_b.count` wachsen dadurch praktisch nie über das hinaus, was ein einzelnes
80-ms-Fenster zufällig eingefangen hat, und Phase 1/2 läuft in vielen Fällen einfach ins
9-Sekunden-Timeout (`RF_TIMEOUT_MS`).

## Nutzen
- Die interlaced Jam+RX-Technik funktioniert danach so, wie das Timing-Design
  (180 ms Jam / 80 ms RX) es nahelegt: Flanken aus mehreren RX-Fenstern akkumulieren
  sich innerhalb einer Phase, statt bei jedem Fenster-Wechsel verworfen zu werden —
  identischer Nutzen wie beim bereits gefixten eigenständigen RollJam, hier für die
  RollForge-eigene Tick-State-Machine nachgezogen.
- Höhere Erfolgsquote pro Phase innerhalb des bestehenden 9-Sekunden-Timeouts, gerade
  bei Fahrzeugen mit längeren Frames — der Fall, den die Interlacing-Technik überhaupt
  adressieren soll.
- Reine Zustands-/Pufferverwaltung im RollJam-Tick-Modul, keine Änderung an
  `rollforge_rf.c` (Preset/Frequenz/Geräte-Umschaltung bleiben unverändert) und keine
  Änderung an RollLab- oder Jammer-Modul, an Phase 3 (Replay) oder an der GUI
  (`rj_draw`).

## Betroffene Dateien
- `rollforge_app.h` — `ModRollJam`: ein neues Feld `bool cap_started;` (Zustand "wurde
  die Aufnahme dieser Phase schon einmal gestartet"), bestehende Felder unverändert.
- `module_rolljam.c` — `rj_cap_start()` implementiert über eine neue `rj_cap_resume()`;
  neue `rj_cap_pause()` für Fensterwechsel ohne Pufferverlust. `rj_cap_stop()`
  unverändert (bleibt der endgültige Stop). `rj_phase_start()` setzt `cap_started`
  zurück. `RJ_P1_JAM`/`RJ_P2_JAM` und `RJ_P1_RX`/`RJ_P2_RX` unterscheiden jetzt
  zwischen einmaligem Start, wiederholtem Pause/Resume und endgültigem Stop.
- `module_rolljam.h` — keine Änderung (öffentliche API bleibt `rj_enter/exit/input/
  tick/draw`; die neuen Helfer sind wie `rj_cap_start`/`rj_cap_stop` file-static).

## Port-Skizze / Patch

### `rollforge_app.h` — `ModRollJam`
```c
typedef struct {
    RjPhase  phase;
    RfSig    sig_a, sig_b;
    volatile size_t tx_pos;
    uint32_t phase_start_ms;
    uint32_t step_start_ms;
    bool     cap_started;    // NEU: true sobald diese Phase einmal rj_cap_start() rief
    char     status[64];
} ModRollJam;
```

### `module_rolljam.c` — Capture-Helfer
```c
// RX-Hardware nach einer Jam-Pause (oder beim allerersten Start) scharf schalten,
// ohne den Puffer zu beruehren -- Fortsetzung derselben Aufnahme, kein Neustart.
static void rj_cap_resume(RollForgeApp* app) {
    if(!app->device) return;
    rollforge_rf_start_rx(app, rj_rx_cb, NULL);
    app->rf_op = RF_CAPTURING;
}

static void rj_cap_start(RollForgeApp* app, RfSig* sig) {
    if(!app->device) return;
    sig->count = 0; sig->ready = false;
    s_cap = sig;
    rj_cap_resume(app);
}

// RX-Hardware anhalten, ohne die laufende Aufnahme aufzugeben: Puffer (sig->count/
// ready) und s_cap bleiben unveraendert -- fuer den Wechsel zurueck zu JAMMING
// innerhalb derselben Phase.
static void rj_cap_pause(RollForgeApp* app) {
    if(app->device && app->rf_op == RF_CAPTURING)
        subghz_devices_stop_async_rx(app->device);
    app->rf_op = RF_IDLE;
}

static void rj_cap_stop(RollForgeApp* app) {
    s_cap = NULL;
    if(app->device && app->rf_op == RF_CAPTURING)
        subghz_devices_stop_async_rx(app->device);
    app->rf_op = RF_IDLE;
}
```

### `module_rolljam.c` — `rj_phase_start()`
```c
static void rj_phase_start(RollForgeApp* app, RfSig* sig, RjPhase jam_state) {
    sig->count = 0; sig->ready = false;
    app->rj.cap_started    = false;   // NEU
    app->rj.phase_start_ms = furi_get_tick();
    app->rj.step_start_ms  = app->rj.phase_start_ms;
    app->rj.phase = jam_state;
    rj_jam_start(app);
}
```

### `module_rolljam.c` — `rj_tick()`, Phase 1 (Phase 2 analog mit `sig_b`/`RJ_P2_*`)
```c
case RJ_P1_JAM:
    if(now - m->phase_start_ms >= RF_TIMEOUT_MS) {
        rj_jam_stop(app);
        rj_cap_stop(app);                 // NEU: falls diese Phase schon einmal
                                           // pausiert hatte, s_cap/Geraet sauber freigeben
        m->phase = RJ_IDLE;
        snprintf(m->status, sizeof(m->status), "Phase 1 Timeout");
    } else if(now - m->step_start_ms >= RJ_JAM_MS) {
        rj_jam_stop(app);
        if(!m->cap_started) {
            rj_cap_start(app, &m->sig_a); // einmalig: Puffer initialisieren
            m->cap_started = true;
        } else {
            rj_cap_resume(app);           // Fortsetzung: Puffer bleibt erhalten
        }
        m->step_start_ms = now;
        m->phase = RJ_P1_RX;
        snprintf(m->status, sizeof(m->status), "Ph1 RX...");
    }
    break;

case RJ_P1_RX:
    if(m->sig_a.ready && m->sig_a.count >= RF_MIN_EDGES) {
        rj_cap_stop(app);                 // endgueltig: Phase erfolgreich abgeschlossen
        snprintf(m->status, sizeof(m->status), "A: %lu edges -- [OK] Ph2", (unsigned long)m->sig_a.count);
        m->phase = RJ_CAPTURED;
        notification_message(app->notif, &sequence_success);
    } else if(now - m->step_start_ms >= RJ_RX_MS) {
        if(now - m->phase_start_ms >= RF_TIMEOUT_MS) {
            rj_cap_stop(app);             // endgueltig: Phasen-Timeout
            m->phase = RJ_IDLE;
            snprintf(m->status, sizeof(m->status), "Phase 1 Timeout");
        } else {
            rj_cap_pause(app);            // GEAENDERT von rj_cap_stop: Puffer behalten
            rj_jam_start(app);
            m->step_start_ms = now;
            m->phase = RJ_P1_JAM;
            snprintf(m->status, sizeof(m->status), "Ph1 Jam...");
        }
    }
    break;
```

Einzige Verhaltensänderung: Innerhalb einer Phase wird der Signalpuffer nur beim
allerersten JAM→RX-Übergang zurückgesetzt (`rj_cap_start`, markiert über
`m->cap_started`). Jeder weitere Übergang JAM→RX ruft `rj_cap_resume()` auf (Hardware
neu scharf schalten, Puffer unangetastet), jeder Übergang RX→JAM ruft `rj_cap_pause()`
auf (Hardware anhalten, Puffer unangetastet). Erst am Phasenende (Erfolg oder Timeout)
räumt `rj_cap_stop()` endgültig auf (`s_cap = NULL`). Phase 2 (`RJ_P2_JAM`/`RJ_P2_RX`)
bekommt exakt dieselbe Umstellung mit `sig_b` statt `sig_a`. Replay (`RJ_REPLAY`), die
Jammer-/RollLab-Module und die GUI (`rj_draw`) sind unverändert.

## Testhinweis (ohne Hardware)
Reine Zustands-/Pufferverwaltung, host-kompilierbar nachvollziehbar. Das folgende
Programm bildet nur das für den Bug relevante Verhalten auf Fensterebene nach — wie
viele Flanken nach zwei aufeinanderfolgenden RX-Fenstern derselben Phase im Puffer
stehen, je nachdem ob bei jedem Fenstereintritt zurückgesetzt wird (alt) oder nur beim
ersten Eintritt der Phase (neu, über `cap_started`):

```c
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>

#define RF_MIN_EDGES 16
#define RF_SIG_BUF   512

typedef struct {
    size_t count;
    bool   ready;
} Sig;

// Vorher: rj_cap_start() wird bei JEDEM RX-Fenster-Eintritt aufgerufen -> Reset.
static void old_cap_start(Sig* sig) {
    sig->count = 0;
    sig->ready = false;
}

// Nachher: nur beim allerersten RX-Fenster der Phase resetten (rj_cap_start),
// danach rj_cap_resume() -- Puffer bleibt erhalten (Analogie zu m->cap_started).
static void new_cap_enter(Sig* sig, bool* cap_started) {
    if(!*cap_started) {
        sig->count = 0;
        sig->ready = false;
        *cap_started = true;
    }
    // sonst: rj_cap_resume() -- kein Zugriff auf sig->count/ready
}

static void feed_window(Sig* sig, size_t edges_in_window, bool trailing_silence) {
    for(size_t i = 0; i < edges_in_window && sig->count < RF_SIG_BUF; i++) sig->count++;
    if(trailing_silence && sig->count >= RF_MIN_EDGES) sig->ready = true;
}

int main(void) {
    Sig old_sig = {0}, new_sig = {0};
    bool new_cap_started = false;

    // RX-Fenster 1: 10 Flanken kommen an, dann unterbricht der Jam-Burst (keine Stille).
    old_cap_start(&old_sig);
    feed_window(&old_sig, 10, false);
    new_cap_enter(&new_sig, &new_cap_started);
    feed_window(&new_sig, 10, false);
    printf("nach Fenster 1: alt count=%zu ready=%d | neu count=%zu ready=%d\n",
           old_sig.count, old_sig.ready, new_sig.count, new_sig.ready);

    // RX-Fenster 2 (nach einem Jam-Zyklus, gleiche Phase): weitere 10 Flanken +
    // Stille (Signalende).
    old_cap_start(&old_sig);   // BUG: verwirft die 10 Flanken aus Fenster 1
    feed_window(&old_sig, 10, true);
    new_cap_enter(&new_sig, &new_cap_started);  // cap_started bereits true -> kein Reset
    feed_window(&new_sig, 10, true);
    printf("nach Fenster 2: alt count=%zu ready=%d | neu count=%zu ready=%d\n",
           old_sig.count, old_sig.ready, new_sig.count, new_sig.ready);

    return 0;
}
```

Erwartete Ausgabe (per `gcc -O2` gegengerechnet):
```
nach Fenster 1: alt count=10 ready=0 | neu count=10 ready=0
nach Fenster 2: alt count=10 ready=0 | neu count=20 ready=1
```

Nach Fenster 2 zeigt sich der Unterschied: "alt" verwirft beim erneuten
`rj_cap_start`-Aufruf die 10 Flanken aus Fenster 1 und bleibt bei 10 (< 16
`RF_MIN_EDGES`) — die Phase erkennt keinen Abschluss, obwohl das Signal insgesamt
bereits 20 Flanken mit korrektem Silence-Trailer geliefert hätte. "Neu" akkumuliert
korrekt auf 20 Flanken und erkennt den Abschluss (`ready=1`).

Funktionaler Test danach lokal auf dem Flipper (eigenes Testfahrzeug/eigener Sender,
autorisiert): RollForge > RollJam, Phase 1 mit einem Keyfob starten, dessen Frame
bekanntlich länger als 80 ms ist (z. B. per vorherigem SubGHz-Read-Vergleich
gemessen); vor dem Fix läuft die Phase bei solchen Signalen deutlich häufiger ins
9-Sekunden-Timeout ("Phase 1 Timeout"), nach dem Fix sollte "A: N edges -- [OK] Ph2"
mit einer Flankenzahl erscheinen, die plausibel über das hinausgeht, was ein
einzelnes 80-ms-Fenster liefern könnte. Der im Status während RJ_P1_RX/RJ_P2_RX
mitgezählte Wert (falls ich die Statuszeile künftig um `[%zu edges]` erweitere) wäre
ein zusätzlicher direkter Beleg für das Akkumulieren über mehrere Fenster.

## Risiko
- Niedrig: Die Änderung bleibt vollständig innerhalb der Capture-Steuerung von
  `module_rolljam.c` (`rj_cap_start/pause/resume/stop`, `rj_phase_start`, die
  `RJ_P1_*`/`RJ_P2_*`-Zweige von `rj_tick`). Kein Eingriff in `rollforge_rf.c`
  (Preset/Frequenz/Geräte-Umschaltung unverändert), keine Änderung an
  `rj_jam_start/stop`, `rj_replay_cb` oder Phase 3 (`RJ_REPLAY`).
- `rj_cap_stop()` (der endgültige Stop) bleibt exakt so, wie er war, und wird
  weiterhin nur an Phasenende aufgerufen (Erfolg oder Timeout) — dieselbe Semantik
  wie vorher, nur an einer zusätzlichen Stelle (JAM-Zweig-Timeout) ergänzt, um
  `s_cap` auch dann sicher zu räumen, wenn die Phase während eines pausierten
  Capture-Zustands abläuft.
- Neues Feld `cap_started` in `ModRollJam` — reiner Zustandsflag ohne
  Speicher-/Timing-Auswirkung, wird ausschließlich in `rj_phase_start()` (Reset) und
  den beiden `RJ_P*_JAM`-Zweigen (Abfrage/Setzen) verwendet.
- Einzig denkbare Nebenwirkung: Eine Phase, die vorher zufällig (weil das komplette
  Signal in ein einzelnes 80-ms-Fenster passte) bereits erfolgreich war, verhält sich
  unverändert — der Fix ändert nichts an bereits funktionierenden
  Einzel-Fenster-Fällen, er behebt ausschließlich das Verwerfen von
  Mehr-Fenster-Fällen.
- Kein zusätzliches Sicherheits-/Missbrauchsrisiko: Der Fix verbessert nur die
  Zuverlässigkeit der bereits vorhandenen, für autorisierte Tests an eigenen
  Geräten/eigenen Fahrzeugen gedachten Funktion — keine neue Fähigkeit, kein
  verändertes Ziel-/Frequenzverhalten.

## Quelle
- `G4MEOVER18/RollForge` (main): `module_rolljam.c` (`rj_tick`, `rj_cap_start/stop`,
  `rj_phase_start`, `s_cap`), `rollforge_app.h` (`ModRollJam`, `RJ_JAM_MS`,
  `RJ_RX_MS`, `RF_MIN_EDGES`, `RF_SILENCE_US`, `RF_TIMEOUT_MS`).
- Ursprünglicher Fund/Fix in der eigenständigen App:
  `app-improvements/RollJam/interlaced-capture-buffer-reset-fix.md` (dieselbe
  Ursache, hier für RollForges eigene, strukturell andere Tick-State-Machine erneut
  analysiert und portiert — die dortige Lösung war nicht automatisch mitgezogen, da
  RollForge eine unabhängige Neuimplementierung des Moduls ist, keine geteilte
  Quelle).
