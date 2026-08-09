# RollLab — Silence-Terminator-Flanke verfälscht die Signalanalyse-Statistik

## Was
Meine RollLab-App erkennt das Ende einer Aufnahme in beiden RX-Callbacks
(`rlab_rx_ref_cb()`/`rlab_rx_work_cb()` in `rolllab_rf.c:34-71`) über eine
Stille-Schwelle: Sobald eine LOW-Flanke mit `duration >= RLAB_SILENCE_US`
(25 ms) eintrifft und bereits mindestens `RLAB_MIN_EDGES` (16) Flanken
vorliegen, wird `sig->ready = true` gesetzt. Diese Terminator-Flanke selbst
wird — wie jede andere Flanke — vorher ganz normal in `sig->buf[]`
abgelegt:

```c
// rolllab_rf.c:41-49 (rlab_rx_ref_cb, analog in rlab_rx_work_cb)
if(sig->count < RLAB_REF_BUF_SIZE) {
    uint32_t d = (duration > 0x7FFFFFFFUL) ? 0x7FFFFFFFUL : duration;
    sig->buf[sig->count++] = (level ? 0x80000000UL : 0UL) | d;
}

if((!level && duration >= RLAB_SILENCE_US && sig->count >= RLAB_MIN_EDGES) ||
   sig->count >= RLAB_REF_BUF_SIZE) {
    sig->ready = true;
}
```

`rlab_analyze()` (`rolllab_app.c:32-74`, Modus "Analyze Signal") iteriert
anschließend über **das komplette** `sig->buf[0 .. sig->count-1]`, um
`edge_count`, `min_us`/`max_us`/`avg_us`, `total_ms`, `preamble_ok` und den
OOK-Bimodalitäts-Heuristikwert `looks_ook` zu berechnen — inklusive dieser
letzten, mindestens 25 ms langen (oft deutlich längeren, da die Flanke erst
beim nächsten Pegelwechsel gemeldet wird und dieser Wechsel bei einem
bereits beendeten Signal ggf. erst deutlich später eintritt) Stille-Flanke.
Diese eine Flanke ist kein Teil des eigentlichen Protokoll-Signals, sondern
reines Endekriterium der Aufnahme — trägt aber ungefiltert in alle vier
Statistikwerte ein:

- `max_us` wird fast immer von dieser einen Flanke dominiert (25 ms+ gegen
  typische Sub-GHz-Pulsbreiten im niedrigen bis mittleren µs- bis
  einstelligen-ms-Bereich).
- `avg_us` wird durch den Ausreißer spürbar nach oben verzerrt, besonders
  bei kurzen Signalen mit wenigen Flanken (16–30), wo eine 25-ms+-Flanke
  einen erheblichen Anteil der Summe ausmacht.
- `total_ms` (Anzeige „Edges:N T:Xms") enthält die Stille-Dauer mit, zeigt
  also eine deutlich zu lange „Signal-Gesamtdauer" an.
- Der OOK-Heuristik-Check `a->max_us < a->min_us * 8` (`rolllab_app.c:72`)
  schlägt wegen des künstlich aufgeblähten `max_us` in der Praxis fast
  immer fehl — echte OOK-Signale werden dadurch als "non-OOK" fehlklassifiziert,
  obwohl die eigentliche Pulsverteilung bimodal und sauber OOK-typisch wäre.

Das betrifft ausschließlich den Analyze-Modus. Die anderen drei Modi
(Replay/Rollback/SyncWin) nutzen `sig->count` nur als reines
Vollständigkeits-/Bereitschaftskriterium (`rlab_capture_ref_done()`/
`rlab_capture_work_done()`) und keine Pulsbreiten-Statistik — dort ist die
mitgezählte Terminator-Flanke folgenlos. Die Replay-TX-Funktion
(`rlab_replay_tx_cb()`) sendet zwar ebenfalls bis `sig->count`, also
inklusive der Terminator-Flanke — das ist aber unproblematisch, da eine
zusätzliche lange LOW-Periode am Ende einer TX-Übertragung für den
Empfänger keine sichtbare Auswirkung hat (Signal ist zu diesem Zeitpunkt
bereits vollständig gesendet).

## Nutzen
- Die im Analyze-Modus angezeigten Werte (Min/Max/Avg, Gesamtdauer, OOK/PWM-
  Erkennung) spiegeln nach dem Fix die tatsächliche Pulsbreitenverteilung
  des Signals wider, nicht die Pulsbreiten plus eine künstliche
  Stille-Ausreißer-Flanke.
- Die OOK-Heuristik (`looks_ook`) wird wieder aussagekräftig — genau das ist
  der Kernzweck des Analyze-Modus laut README ("OOK/FSK heuristic"). Aktuell
  liefert sie bei den meisten realen Aufnahmen ein falsches „non-OOK", weil
  `max_us` von der Terminator-Flanke dominiert wird.
- Reine Statistik-/Nachbearbeitungslogik in `rlab_analyze()` — keine Änderung
  an RX-Callback, Capture-Timing oder Pufferverwaltung, kein Risiko für die
  anderen drei Modi.

## Betroffene Dateien
- `rolllab_app.c` — `rlab_analyze()`: vor der Statistik-Schleife die
  Terminator-Flanke identifizieren (letzte Flanke im Puffer, LOW-Pegel,
  Dauer `>= RLAB_SILENCE_US`) und die Statistikschleifen nur bis
  `stat_count` statt `sig->count` laufen lassen.

Kein neuer Include, keine neue State, keine Änderung an `rolllab_rf.c`,
`rolllab_app.h` oder `rolllab_states.h` nötig — der Fix ist vollständig
lokal in `rlab_analyze()`.

## Port-Skizze / Patch

### `rolllab_app.c` — `rlab_analyze()`
```c
static void rlab_analyze(RollLabApp* app) {
    RLabAnalysis* a    = &app->analysis;
    const RLabRefSignal* sig = &app->ref_sig;

    *a = (RLabAnalysis){.min_us = UINT32_MAX};

    // Die letzte Flanke ist bei einer per Stille-Timeout beendeten Aufnahme
    // nur das Endekriterium (>= RLAB_SILENCE_US LOW), kein echter Signal-
    // Puls -- aus allen Statistiken ausschliessen. Bei einer per Puffer-
    // ueberlauf beendeten Aufnahme (sig->count >= RLAB_REF_BUF_SIZE) ist die
    // letzte Flanke echte Nutzdaten und bleibt erhalten.
    size_t stat_count = sig->count;
    if(stat_count > 0 && stat_count < RLAB_REF_BUF_SIZE) {
        uint32_t last = sig->buf[stat_count - 1];
        bool last_level    = (last & 0x80000000UL) != 0;
        uint32_t last_dur  = last & 0x7FFFFFFFUL;
        if(!last_level && last_dur >= RLAB_SILENCE_US) stat_count--;
    }

    uint64_t sum      = 0;
    uint32_t sh_count = 0;

    for(size_t i = 0; i < stat_count; i++) {
        uint32_t dur = sig->buf[i] & 0x7FFFFFFFUL;
        if(!dur) continue;
        a->edge_count++;
        sum += dur;
        if(dur < a->min_us) a->min_us = dur;
        if(dur > a->max_us) a->max_us = dur;
    }

    if(!a->edge_count) return;
    a->avg_us = (uint32_t)(sum / a->edge_count);
    if(a->min_us == UINT32_MAX) a->min_us = 0;

    // Total signal time in ms (capped at 999 ms for display)
    uint64_t total_us = sum;
    a->total_ms = (uint32_t)(total_us / 1000 > 999 ? 999 : total_us / 1000);

    // Preamble: first edge is HIGH and > 3x average
    if(sig->count > 0 && (sig->buf[0] & 0x80000000UL)) {
        uint32_t first = sig->buf[0] & 0x7FFFFFFFUL;
        a->preamble_ok = (a->avg_us > 0) && (first > a->avg_us * 3);
    }

    // OOK heuristic: bi-modal duration distribution (short vs long pulses)
    if(a->max_us > a->min_us) {
        uint32_t thresh = a->min_us + (a->max_us - a->min_us) / 3;
        for(size_t i = 0; i < stat_count; i++) {
            uint32_t dur = sig->buf[i] & 0x7FFFFFFFUL;
            if(dur && dur <= thresh) sh_count++;
        }
        uint32_t pct = (sh_count * 100) / a->edge_count;
        a->looks_ook = (pct >= 25 && pct <= 75) && (a->max_us < a->min_us * 8);
    }
}
```

Nur drei Änderungen gegenüber dem bestehenden Code: die neue
`stat_count`-Berechnung am Funktionsanfang, und die beiden Statistik-
Schleifen (`edge_count`/Summe/Min/Max sowie die OOK-Bimodalitäts-Zählung)
laufen bis `stat_count` statt bis `sig->count`. Die Preamble-Prüfung bleibt
bewusst bei `sig->buf[0]`/`sig->count` — sie betrachtet nur die erste
Flanke, die von der Terminator-Flanke am Ende nicht betroffen ist.

## Testhinweis (ohne Hardware)
Reine Nachbearbeitungslogik über ein Array aus Duration/Level-Werten,
host-kompilierbar ohne Flipper-SDK. Das folgende Programm bildet exakt die
Statistik-Schleifen aus `rlab_analyze()` nach (alt: über den vollen Puffer,
neu: mit Terminator-Ausschluss) und rechnet sie über eine synthetische
Flankenfolge nach, wie sie eine reale Aufnahme mit 20 echten Pulsen
(alternierend 300/100 µs, ein typisches bimodales OOK-Muster mit
Max/Min-Verhältnis 3) plus einer 30-ms-Stille-Terminator-Flanke liefern
würde:

```c
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define RLAB_SILENCE_US 25000U
#define RLAB_REF_BUF_SIZE 1024U

typedef struct { uint32_t edge_count, min_us, max_us, avg_us, total_ms; bool looks_ook; } Analysis;

static void analyze(const uint32_t* buf, size_t count, bool exclude_terminator, Analysis* a) {
    *a = (Analysis){.min_us = UINT32_MAX};

    size_t stat_count = count;
    if(exclude_terminator && stat_count > 0 && stat_count < RLAB_REF_BUF_SIZE) {
        uint32_t last = buf[stat_count - 1];
        bool last_level = (last & 0x80000000UL) != 0;
        uint32_t last_dur = last & 0x7FFFFFFFUL;
        if(!last_level && last_dur >= RLAB_SILENCE_US) stat_count--;
    }

    uint64_t sum = 0;
    uint32_t sh_count = 0;
    for(size_t i = 0; i < stat_count; i++) {
        uint32_t dur = buf[i] & 0x7FFFFFFFUL;
        if(!dur) continue;
        a->edge_count++;
        sum += dur;
        if(dur < a->min_us) a->min_us = dur;
        if(dur > a->max_us) a->max_us = dur;
    }
    if(!a->edge_count) return;
    a->avg_us = (uint32_t)(sum / a->edge_count);
    a->total_ms = (uint32_t)(sum / 1000 > 999 ? 999 : sum / 1000);

    if(a->max_us > a->min_us) {
        uint32_t thresh = a->min_us + (a->max_us - a->min_us) / 3;
        for(size_t i = 0; i < stat_count; i++) {
            uint32_t dur = buf[i] & 0x7FFFFFFFUL;
            if(dur && dur <= thresh) sh_count++;
        }
        uint32_t pct = (sh_count * 100) / a->edge_count;
        a->looks_ook = (pct >= 25 && pct <= 75) && (a->max_us < a->min_us * 8);
    }
}

int main(void) {
    // 20 alternierende Pulse (HIGH 300 us / LOW 100 us) -- bimodales
    // OOK-Muster mit Max/Min-Verhaeltnis 3 -- plus eine 30-ms-Stille-
    // Terminator-Flanke (LOW, wie sie rlab_rx_ref_cb beim Beenden der
    // Aufnahme liefert).
    uint32_t buf[21];
    uint32_t widths[20];
    for(int i = 0; i < 20; i++) widths[i] = (i % 2 == 0) ? 300 : 100;
    for(size_t i = 0; i < 20; i++)
        buf[i] = (i % 2 == 0 ? 0x80000000UL : 0UL) | widths[i];
    buf[20] = 0UL | 30000UL; // LOW, 30 ms Terminator

    Analysis old_a, new_a;
    analyze(buf, 21, false, &old_a);
    analyze(buf, 21, true,  &new_a);

    printf("alt (mit Terminator):  edges=%u min=%u max=%u avg=%u total_ms=%u ook=%d\n",
           old_a.edge_count, old_a.min_us, old_a.max_us, old_a.avg_us, old_a.total_ms, old_a.looks_ook);
    printf("neu (ohne Terminator): edges=%u min=%u max=%u avg=%u total_ms=%u ook=%d\n",
           new_a.edge_count, new_a.min_us, new_a.max_us, new_a.avg_us, new_a.total_ms, new_a.looks_ook);
    return 0;
}
```

Erwartete Ausgabe (per `gcc -O2` gegengerechnet):
```
alt (mit Terminator):  edges=21 min=100 max=30000 avg=1619 total_ms=34 ook=0
neu (ohne Terminator): edges=20 min=100 max=300 avg=200 total_ms=4 ook=1
```

Der Unterschied zeigt das Problem direkt: "alt" meldet `max=30000`,
`avg=1619`, `total_ms=34` (statt der tatsächlichen 100/300/200 us / 4 ms)
und klassifiziert das Signal wegen `max_us < min_us*8` (30000 < 800 ist
falsch) als "non-OOK", obwohl die zugrunde liegenden 20 Pulse ein sauberes
100/300-us-Bimodal-Muster (Verhältnis 3, klar innerhalb der
OOK-Heuristikschwelle) bilden. "neu" liefert die korrekten
Pulsbreiten-Werte und erkennt das Muster korrekt als OOK.

Funktionaler Test danach lokal auf dem Flipper (eigener Sender/eigenes
Testsignal, autorisiert): Analyze Signal mit einem bekannten Sub-GHz-Signal
(z. B. eigene Fernbedienung mit bekannter Pulsbreite laut Datenblatt oder
vorherigem SubGHz-Read) ausführen; vor dem Fix zeigt Max/Avg/Total deutlich
zu hohe, von der Pulsbreite losgelöste Werte (im Bereich der 25-ms-Stille-
Schwelle oder höher), nach dem Fix liegen Min/Max/Avg im erwarteten
Pulsbreitenbereich des Signals und die OOK/PWM-Erkennung sollte für ein
bekanntes OOK-Signal "OOK/PWM" statt "non-OOK" anzeigen.

## Risiko
- Sehr niedrig: Die Änderung betrifft ausschließlich die Nachbearbeitung in
  `rlab_analyze()` (Analyze-Modus), nicht die RX-Callbacks, nicht die
  Capture-/Replay-Steuerung und nicht die anderen drei Modi (Replay/
  Rollback/SyncWin), die `sig->count` nur als Bereitschaftskriterium nutzen.
- `sig->buf[]`/`sig->count` selbst bleiben unverändert — es wird nichts aus
  dem Originalpuffer gelöscht, nur die Statistikschleifen bekommen eine
  engere obere Grenze für den Fall, dass die letzte Flanke eindeutig als
  Stille-Terminator identifizierbar ist (LOW-Pegel **und** Dauer
  `>= RLAB_SILENCE_US`).
- Randfall Pufferüberlauf (`sig->count >= RLAB_REF_BUF_SIZE`, 1024 Flanken):
  hier wird die letzte Flanke bewusst **nicht** ausgeschlossen (`stat_count
  < RLAB_REF_BUF_SIZE`-Bedingung), da die Aufnahme in diesem Fall nicht über
  die Stille-Schwelle, sondern durch volle Pufferkapazität beendet wurde —
  die letzte gespeicherte Flanke ist dort mit hoher Wahrscheinlichkeit noch
  echte Nutzdatenflanke.
- Randfall kurzes Signal, dessen letzte echte Datenflanke zufällig selbst
  LOW und `>= RLAB_SILENCE_US` lang ist (ungewöhnlich für ein aktives
  Protokoll-Signal, aber theoretisch möglich): würde durch den Fix ebenfalls
  ausgeschlossen. Das ist die gleiche Interpretation, die die RX-Callback-
  Logik selbst für „Ende der Aufnahme" verwendet (`rolllab_rf.c:46-49`) —
  konsistent mit dem bestehenden Bereitschaftskriterium, kein neues
  Sonderverhalten.
- Der Build (`fbt`/`ufbt`) deckt Tippfehler/Signaturabweichungen als
  Compile-Fehler auf; die reine Vergleichs-/Schleifenlogik ist mit dem
  Testprogramm oben bereits ohne Hardware nachgerechnet.

## Quelle
- `G4MEOVER18/RollLab` (main): `rolllab_app.c` (`rlab_analyze()`),
  `rolllab_rf.c` (`rlab_rx_ref_cb()`, `RLAB_SILENCE_US`-Verwendung),
  `rolllab_app.h` (`RLAB_SILENCE_US`, `RLAB_MIN_EDGES`, `RLAB_REF_BUF_SIZE`).
