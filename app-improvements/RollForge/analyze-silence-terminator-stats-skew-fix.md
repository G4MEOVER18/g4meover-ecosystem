# RollForge — Silence-Terminator-Flanke verfälscht die RollLab-Modul-Analyse

## Was
Das in RollForge gebündelte RollLab-Modul (`module_rolllab.c`) trägt denselben
Statistik-Skew-Bug, den ich bereits in meiner eigenständigen RollLab-App gefunden
und behoben habe (siehe
`app-improvements/RollLab/analyze-silence-terminator-stats-skew-fix.md`) — auch
hier eine unabhängige Neuimplementierung, der Fix wurde nicht mitgezogen.

Der RX-Callback für die Referenzaufnahme erkennt das Aufnahmeende über eine
Stille-Schwelle und legt die Terminator-Flanke selbst mit in `sig->buf[]` ab:

```c
// module_rolllab.c:17-29 (rl_rx_ref_cb)
static void rl_rx_ref_cb(bool level, uint32_t duration, void* ctx) {
    UNUSED(ctx);
    RollForgeApp* app = (RollForgeApp*)s_ref_app;
    if(!app) return;
    RfSig* sig = &app->rl.ref_sig;
    if(sig->ready || duration == 0) return;
    if(sig->count < RF_SIG_BUF) {
        uint32_t d = (duration > 0x7FFFFFFFUL) ? 0x7FFFFFFFUL : duration;
        sig->buf[sig->count++] = (level ? 0x80000000UL : 0UL) | d;
    }
    if((!level && duration >= RF_SILENCE_US && sig->count >= RF_MIN_EDGES) ||
       sig->count >= RF_SIG_BUF)
        sig->ready = true;
}
```

`rl_analyze()` (Analyzer-Modus, `module_rolllab.c:102-121`) iteriert anschließend
über den **kompletten** `sig->count`, inklusive dieser mindestens 25 ms
(`RF_SILENCE_US`) langen, oft deutlich längeren Stille-Flanke, die kein Teil des
eigentlichen Signals ist, sondern reines Endekriterium der Aufnahme:

```c
static void rl_analyze(RollForgeApp* app) {
    RlAnalysis* a = &app->rl.analysis;
    const RfSig* sig = &app->rl.ref_sig;
    if(!sig->count) return;

    a->edges     = (uint32_t)sig->count;
    a->min_us    = 0xFFFFFFFFUL;
    a->max_us    = 0;
    uint64_t sum = 0;
    for(size_t i = 0; i < sig->count; i++) {
        uint32_t d = sig->buf[i] & 0x7FFFFFFFUL;
        if(d < a->min_us) a->min_us = d;
        if(d > a->max_us) a->max_us = d;
        sum += d;
    }
    a->avg_us   = (uint32_t)(sum / sig->count);
    a->total_ms = (uint32_t)(sum / 1000UL);

    // Preamble: first edge HIGH and >= 200µs
    a->preamble_ok = (sig->count > 0) && ((sig->buf[0] & 0x80000000UL) != 0) &&
                     ((sig->buf[0] & 0x7FFFFFFFUL) >= 200);

    // Bi-modal heuristic: if (max/min > 3) likely OOK/PWM
    a->looks_ook = (a->min_us > 0) && ((a->max_us / a->min_us) >= 2);
}
```

RollForges `looks_ook`-Heuristik ist einfacher als die der eigenständigen
RollLab-App (reines `max_us / min_us >= 2`, kein Prozentanteil-Fenster, keine
obere Schranke). Das ändert die Fehlerrichtung gegenüber dem ursprünglichen
Fund: Weil die Terminator-Flanke `max_us` extrem in die Höhe treibt, kippt die
Heuristik hier nicht wie in RollLab in Richtung falscher "non-OOK"-Meldung,
sondern in die entgegengesetzte Richtung — sie meldet für so gut wie jedes
Signal `looks_ook = true` (inklusive Signale, deren tatsächliche Pulsbreiten
eng beieinanderliegen, z. B. FSK-artige Signale mit einem echten Verhältnis
knapp über 1), weil bereits `max_us / min_us` durch die 25-ms+-Flanke
automatisch weit über die Schwelle `2` springt. Unabhängig von der
OOK-Klassifikation sind `max_us`, `avg_us` und `total_ms` in jedem Fall massiv
verfälscht — der Analyzer zeigt eine "Signal-Gesamtdauer" und Puls-Extremwerte,
die die Stille-Wartezeit statt das eigentliche Signal widerspiegeln.

Betroffen ist ausschließlich `RL_ANALYZE_VIEW` (Analyzer-Modus). Die anderen
drei Modi (Replay/Rollback/SyncWin) nutzen `sig->count` nur als
Vollständigkeitskriterium (`m->ref_sig.count >= RF_MIN_EDGES` in `rl_tick()`)
und keine Pulsbreiten-Statistik — dort bleibt die mitgezählte Terminator-Flanke
folgenlos. Der Replay-Callback (`rl_replay_cb()`) sendet ebenfalls bis
`ref_sig.count`, also inklusive Terminator-Flanke — unproblematisch, da eine
zusätzliche lange LOW-Periode am Ende einer bereits vollständig gesendeten
TX-Übertragung für den Empfänger keine Auswirkung hat (identisch zur bereits
für die eigenständige RollLab-App dokumentierten Einschätzung).

## Nutzen
- Die im Analyzer-Modus angezeigten Werte (Min/Max/Avg, Gesamtdauer) spiegeln
  nach dem Fix die tatsächliche Pulsbreitenverteilung des Signals wider, nicht
  die Pulsbreiten plus eine künstliche Stille-Ausreißer-Flanke.
- Die OOK/PWM-Heuristik (`looks_ook`) wird wieder aussagekräftig — aktuell
  meldet sie wegen der Terminator-Flanke nahezu jedes Signal fälschlich als
  OOK, auch solche mit eng beieinanderliegenden Pulsbreiten.
- Reine Nachbearbeitungslogik in `rl_analyze()` — keine Änderung an
  RX-Callback, Capture-Timing oder Pufferverwaltung, kein Risiko für die
  anderen drei Modi oder für RollForges RollJam-/Jammer-Module.

## Betroffene Dateien
- `module_rolllab.c` — `rl_analyze()`: vor der Statistik-Schleife die
  Terminator-Flanke identifizieren (letzte Flanke im Puffer, LOW-Pegel, Dauer
  `>= RF_SILENCE_US`) und die Statistikschleife nur bis `stat_count` statt
  `sig->count` laufen lassen.

Kein neuer Include, keine neue State, keine Änderung an `module_rolllab.h`,
`rollforge_app.h` oder den RX-Callbacks nötig — der Fix ist vollständig lokal
in `rl_analyze()`.

## Port-Skizze / Patch

### `module_rolllab.c` — `rl_analyze()`
```c
static void rl_analyze(RollForgeApp* app) {
    RlAnalysis* a = &app->rl.analysis;
    const RfSig* sig = &app->rl.ref_sig;
    if(!sig->count) return;

    // Die letzte Flanke ist bei einer per Stille-Timeout beendeten Aufnahme
    // nur das Endekriterium (>= RF_SILENCE_US LOW), kein echter Signalpuls --
    // aus der Statistik ausschliessen. Bei einer per Pufferueberlauf
    // beendeten Aufnahme (sig->count >= RF_SIG_BUF) ist die letzte
    // gespeicherte Flanke echte Nutzdaten und bleibt erhalten.
    size_t stat_count = sig->count;
    if(stat_count < RF_SIG_BUF) {
        uint32_t last = sig->buf[stat_count - 1];
        bool last_level = (last & 0x80000000UL) != 0;
        uint32_t last_dur = last & 0x7FFFFFFFUL;
        if(!last_level && last_dur >= RF_SILENCE_US) stat_count--;
    }
    if(!stat_count) return;

    a->edges     = (uint32_t)stat_count;
    a->min_us    = 0xFFFFFFFFUL;
    a->max_us    = 0;
    uint64_t sum = 0;
    for(size_t i = 0; i < stat_count; i++) {
        uint32_t d = sig->buf[i] & 0x7FFFFFFFUL;
        if(d < a->min_us) a->min_us = d;
        if(d > a->max_us) a->max_us = d;
        sum += d;
    }
    a->avg_us   = (uint32_t)(sum / stat_count);
    a->total_ms = (uint32_t)(sum / 1000UL);

    // Preamble: first edge HIGH and >= 200µs
    a->preamble_ok = (sig->count > 0) && ((sig->buf[0] & 0x80000000UL) != 0) &&
                     ((sig->buf[0] & 0x7FFFFFFFUL) >= 200);

    // Bi-modal heuristic: if (max/min > 3) likely OOK/PWM
    a->looks_ook = (a->min_us > 0) && ((a->max_us / a->min_us) >= 2);
}
```

Nur zwei Änderungen gegenüber dem bestehenden Code: die neue
`stat_count`-Berechnung direkt nach dem bestehenden `if(!sig->count) return;`,
und die Statistikschleife (Min/Max/Summe) sowie die `avg_us`-Berechnung laufen
bis `stat_count` statt bis `sig->count`. `edges` wird bewusst ebenfalls auf
`stat_count` umgestellt, damit die angezeigte Flankenzahl zur tatsächlich
ausgewerteten Pulsbreitenmenge passt. Die Preamble-Prüfung bleibt bei
`sig->buf[0]`/`sig->count` — sie betrachtet nur die erste Flanke, von der
Terminator-Flanke am Ende unbetroffen.

## Testhinweis (ohne Hardware)
Reine Nachbearbeitungslogik über ein Array aus Duration/Level-Werten,
host-kompilierbar ohne Flipper-SDK. Zwei Testfälle, beide 1:1-Nachbau der
`rl_analyze()`-Schleifen (alt: über den vollen Puffer, neu: mit
Terminator-Ausschluss) mit RollForges echten Konstanten (`RF_SIG_BUF=512`,
`RF_SILENCE_US=25000`):

```c
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define RF_SIG_BUF     512U
#define RF_SILENCE_US 25000U

typedef struct { uint32_t edges, min_us, max_us, avg_us, total_ms; bool looks_ook; } RlAnalysis;

static void analyze(const uint32_t* buf, size_t count, bool exclude_terminator, RlAnalysis* a) {
    *a = (RlAnalysis){0};
    if(!count) return;
    size_t stat_count = count;
    if(exclude_terminator && stat_count < RF_SIG_BUF) {
        uint32_t last = buf[stat_count - 1];
        bool last_level = (last & 0x80000000UL) != 0;
        uint32_t last_dur = last & 0x7FFFFFFFUL;
        if(!last_level && last_dur >= RF_SILENCE_US) stat_count--;
    }
    if(!stat_count) return;
    a->edges = (uint32_t)stat_count;
    a->min_us = 0xFFFFFFFFUL; a->max_us = 0;
    uint64_t sum = 0;
    for(size_t i = 0; i < stat_count; i++) {
        uint32_t d = buf[i] & 0x7FFFFFFFUL;
        if(d < a->min_us) a->min_us = d;
        if(d > a->max_us) a->max_us = d;
        sum += d;
    }
    a->avg_us = (uint32_t)(sum / stat_count);
    a->total_ms = (uint32_t)(sum / 1000UL);
    a->looks_ook = (a->min_us > 0) && ((a->max_us / a->min_us) >= 2);
}

int main(void) {
    // Testfall 1: 20 alternierende Pulse (HIGH 300us / LOW 100us, echtes
    // OOK-Muster) + 30ms Stille-Terminator-Flanke.
    uint32_t buf1[21];
    for(size_t i = 0; i < 20; i++) {
        uint32_t d = (i % 2 == 0) ? 300 : 100;
        buf1[i] = (i % 2 == 0 ? 0x80000000UL : 0UL) | d;
    }
    buf1[20] = 0UL | 30000UL;
    RlAnalysis a1o, a1n;
    analyze(buf1, 21, false, &a1o);
    analyze(buf1, 21, true,  &a1n);
    printf("Test1 (echtes OOK) alt:  max=%u avg=%u total_ms=%u ook=%d\n",
        a1o.max_us, a1o.avg_us, a1o.total_ms, a1o.looks_ook);
    printf("Test1 (echtes OOK) neu:  max=%u avg=%u total_ms=%u ook=%d\n",
        a1n.max_us, a1n.avg_us, a1n.total_ms, a1n.looks_ook);

    // Testfall 2: 20 Pulse mit eng beieinanderliegenden Breiten (145-155us,
    // echtes Verhaeltnis < 2 -- kein OOK) + 30ms Terminator-Flanke.
    uint32_t widths[20] = {150,145,155,150,148,152,150,149,151,150,
                            147,153,150,150,146,154,150,151,149,150};
    uint32_t buf2[21];
    for(size_t i = 0; i < 20; i++)
        buf2[i] = (i % 2 == 0 ? 0x80000000UL : 0UL) | widths[i];
    buf2[20] = 0UL | 30000UL;
    RlAnalysis a2o, a2n;
    analyze(buf2, 21, false, &a2o);
    analyze(buf2, 21, true,  &a2n);
    printf("Test2 (eng, kein OOK) alt: max/min=%u/%u ook=%d (falsch)\n",
        a2o.max_us, a2o.min_us, a2o.looks_ook);
    printf("Test2 (eng, kein OOK) neu: max/min=%u/%u ook=%d (korrekt)\n",
        a2n.max_us, a2n.min_us, a2n.looks_ook);

    // Randfall: Pufferueberlauf (count == RF_SIG_BUF) -- letzte Flanke bleibt erhalten.
    uint32_t full[RF_SIG_BUF];
    for(size_t i = 0; i < RF_SIG_BUF; i++) full[i] = (i % 2 == 0 ? 0x80000000UL : 0UL) | 150U;
    RlAnalysis fo, fn;
    analyze(full, RF_SIG_BUF, false, &fo);
    analyze(full, RF_SIG_BUF, true,  &fn);
    printf("Randfall voller Puffer: edges alt=%u neu=%u (gleich erwartet)\n", fo.edges, fn.edges);
    return 0;
}
```

Lokal mit `gcc -O2 -Wall -Wextra` gebaut und ausgeführt, Ausgabe:
```
Test1 (echtes OOK) alt:  max=30000 avg=1619 total_ms=34 ook=1
Test1 (echtes OOK) neu:  max=300 avg=200 total_ms=4 ook=1
Test2 (eng, kein OOK) alt: max/min=30000/145 ook=1 (falsch)
Test2 (eng, kein OOK) neu: max/min=155/145 ook=0 (korrekt)
Randfall voller Puffer: edges alt=512 neu=512 (gleich erwartet)
```

Testfall 1 zeigt: selbst wenn die OOK-Klassifikation zufällig in beiden
Varianten `true` bleibt (weil das echte Signal bereits ein deutliches
Pulsbreiten-Verhältnis hat), sind `max`/`avg`/`total_ms` vorher komplett von
der Stille-Flanke dominiert (30000/1619/34 statt der tatsächlichen
300/200/4). Testfall 2 zeigt den direkten Fehlklassifikations-Fall: ein
Signal mit eng beieinanderliegenden, nicht-bimodalen Pulsbreiten (Verhältnis
155/145 ≈ 1,07, klar unter der Schwelle 2) wird vorher wegen der
Terminator-Flanke trotzdem als "OOK" gemeldet, nach dem Fix korrekt als
"non-OOK". Der Randfall bestätigt, dass ein per Pufferüberlauf beendeter Scan
unverändert bleibt.

Funktionaler Test danach lokal auf dem Flipper (eigener Sender/eigenes
Testsignal, autorisiert): RollForge > RollLab > Analyzer mit einem bekannten
Sub-GHz-Signal ausführen; vor dem Fix zeigt Max/Avg/Gesamtdauer deutlich zu
hohe, von der Pulsbreite losgelöste Werte (im Bereich der 25-ms-Stille-Schwelle
oder höher) und die OOK-Anzeige ist für die meisten Signale unabhängig vom
tatsächlichen Muster "OOK"; nach dem Fix liegen Min/Max/Avg im erwarteten
Pulsbreitenbereich des Signals und die OOK-Anzeige unterscheidet wieder
zwischen bimodalen und eng beieinanderliegenden Pulsbreiten.

## Risiko
- Sehr niedrig: Die Änderung betrifft ausschließlich die Nachbearbeitung in
  `rl_analyze()` (Analyzer-Modus), nicht die RX-Callbacks, nicht die
  Capture-/Replay-Steuerung und nicht die anderen drei Modi (Replay/
  Rollback/SyncWin) oder RollForges RollJam-/Jammer-Module.
- `sig->buf[]`/`sig->count` selbst bleiben unverändert — es wird nichts aus
  dem Originalpuffer gelöscht, nur die Statistikschleife bekommt eine engere
  obere Grenze für den Fall, dass die letzte Flanke eindeutig als
  Stille-Terminator identifizierbar ist (LOW-Pegel **und** Dauer
  `>= RF_SILENCE_US`).
- Randfall Pufferüberlauf (`sig->count >= RF_SIG_BUF`): letzte Flanke bewusst
  **nicht** ausgeschlossen, da die Aufnahme hier durch volle Pufferkapazität
  statt durch die Stille-Schwelle beendet wurde — mit hoher Wahrscheinlichkeit
  noch echte Nutzdatenflanke (durch das Testprogramm oben verifiziert:
  `edges alt=neu=512`).
- Randfall kurzes Signal, dessen letzte echte Datenflanke zufällig selbst LOW
  und `>= RF_SILENCE_US` lang ist (ungewöhnlich für ein aktives
  Protokoll-Signal, aber theoretisch möglich): würde durch den Fix ebenfalls
  ausgeschlossen — dieselbe Interpretation, die die RX-Callback-Logik selbst
  für „Ende der Aufnahme" verwendet (`rl_rx_ref_cb()`), also konsistent mit
  dem bestehenden Bereitschaftskriterium.
- Der Build (`fbt`/`ufbt`) deckt Tippfehler/Signaturabweichungen als
  Compile-Fehler auf; die reine Vergleichs-/Schleifenlogik ist mit dem
  Testprogramm oben bereits ohne Hardware nachgerechnet.

## Quelle
- `G4MEOVER18/RollForge` (main): `module_rolllab.c` (`rl_analyze()`,
  `rl_rx_ref_cb()`, `RF_SILENCE_US`-Verwendung), `rollforge_app.h`
  (`RF_SILENCE_US`, `RF_MIN_EDGES`, `RF_SIG_BUF`, `RfSig`, `RlAnalysis`).
- Ursprünglicher Fund/Fix in der eigenständigen App:
  `app-improvements/RollLab/analyze-silence-terminator-stats-skew-fix.md`
  (dieselbe Ursache, hier für RollForges eigene, einfachere OOK-Heuristik
  erneut analysiert — die Fehlerrichtung bei `looks_ook` unterscheidet sich
  dadurch von der eigenständigen App: dort falsches "non-OOK", hier falsches
  "OOK" bei eng beieinanderliegenden Pulsbreiten).
