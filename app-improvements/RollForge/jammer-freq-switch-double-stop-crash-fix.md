# RollForge — Absturz beim Frequenzwechsel während des Jammings (Jammer-Modul)

## Was
Ich habe das RollForge-eigene Jammer-Modul (`module_jammer.c`) geprüft — bisher
laut Backlog als "Tiefenaudit offen" vorgemerkt, da RollForges RollJam-/RollLab-Module
schon durchgesehen waren. Fund: `jm_input()` löst bei laufendem Jamming einen
verbotenen Doppel-Stop der CC1101-Treiber-Zustandsmaschine aus, sobald ich während
`JM_JAMMING` die Frequenz wechsle (Taste Hoch/Runter) — das führt zu einem harten
Geräteabsturz (Crash/Reboot), nicht nur zu einem Logikfehler.

In `InputKeyUp`/`InputKeyDown` steht (beide Zweige identisch aufgebaut):
```c
case InputKeyUp:
    if(m->freq_idx > 0) m->freq_idx--;
    app->frequency = JM_FREQS[m->freq_idx];
    if(m->phase == JM_JAMMING && app->device) {
        s_jm_active = false;
        subghz_devices_stop_async_tx(app->device);   // Stop #1 -- direkt hier
        rollforge_rf_start_tx(app, jm_tx_cb, NULL);   // ruft INTERN nochmal Stop auf
        s_jm_active = true;
        app->rf_op = RF_JAMMING;
    }
    ...
```

`rollforge_rf_start_tx()` (`rollforge_rf.c:99-105`) stoppt die laufende Operation
aber bereits selbst, bevor es neu startet:
```c
void rollforge_rf_start_tx(RollForgeApp* app, FuriHalSubGhzAsyncTxCallback cb, void* ctx) {
    if(!app->device) return;
    rf_stop_current(app);   // state → Idle
    ...
}
```
und `rf_stop_current()` entscheidet dabei anhand von `app->rf_op`, was zu stoppen ist:
```c
static void rf_stop_current(RollForgeApp* app) {
    if(!app->device) return;
    if(app->rf_op == RF_JAMMING || app->rf_op == RF_REPLAYING)
        subghz_devices_stop_async_tx(app->device);
    ...
}
```
Der eigene Kommentar direkt über dieser Funktion benennt die Gefahr bereits explizit:
*"furi_hal prüft: stop_async_tx braucht state==AsyncTx, stop_async_rx braucht
state==AsyncRx. Nur den passenden Stop aufrufen!"* — genau das verletzt der
Input-Handler: `app->rf_op` wird in `InputKeyUp`/`InputKeyDown` erst **nach** dem
Aufruf von `rollforge_rf_start_tx()` wieder auf `RF_JAMMING` gesetzt, steht zum
Zeitpunkt des internen `rf_stop_current()`-Aufrufs also noch auf `RF_JAMMING` (vom
vorherigen Zustand). Damit ruft `rf_stop_current()` `subghz_devices_stop_async_tx()`
ein zweites Mal auf einem Gerät auf, das durch den direkten Stop-Aufruf im
Input-Handler bereits in `Idle` ist — genau die vom eigenen Kommentar beschriebene
Zustandsverletzung, die `furi_hal` per `furi_check()` hart quittiert (Abbruch/Neustart).

## Nutzen
- Behebt einen reproduzierbaren Absturz: aktuell crasht der Flipper bei *jedem*
  Frequenzwechsel (Hoch/Runter) während aktivem Jamming — eine der beiden
  Kernbedienungen des Moduls (Start + Frequenzwahl) ist damit praktisch unbenutzbar,
  sobald das Jamming bereits läuft.
- Nach dem Fix bleibt der eigentliche Zweck des direkten Stop-Aufrufs erhalten
  (sofortiger Trägerstopp vor dem Neustart auf neuer Frequenz, kein Jamming-Leck auf
  der alten Frequenz) — er passiert nur noch genau einmal, an der richtigen Stelle
  (innerhalb von `rollforge_rf_start_tx()`), statt doppelt.
- Reine Eingabe-/Zustandslogik im Jammer-Modul, keine Änderung an `rollforge_rf.c`,
  an `jm_tx_cb()` (Trägersignal-Erzeugung) oder an den anderen RollForge-Modulen.

## Betroffene Dateien
- `module_jammer.c` — `jm_input()`: die beiden identischen direkten
  `subghz_devices_stop_async_tx(app->device);`-Aufrufe in `InputKeyUp` und
  `InputKeyDown` entfallen ersatzlos; `rollforge_rf_start_tx()` übernimmt den Stop
  bereits korrekt.

## Port-Skizze / Patch

### `module_jammer.c` — `jm_input()`, `InputKeyUp` (Down analog)
```c
case InputKeyUp:
    if(m->freq_idx > 0) m->freq_idx--;
    app->frequency = JM_FREQS[m->freq_idx];
    if(m->phase == JM_JAMMING && app->device) {
        s_jm_active = false;
        rollforge_rf_start_tx(app, jm_tx_cb, NULL);   // stoppt selbst passend zu rf_op
        s_jm_active = true;
        app->rf_op = RF_JAMMING;
    }
    snprintf(m->status, sizeof(m->status), "%s", JM_FREQ_LABELS[m->freq_idx]);
    break;
case InputKeyDown:
    if(m->freq_idx < JM_FREQ_COUNT - 1) m->freq_idx++;
    app->frequency = JM_FREQS[m->freq_idx];
    if(m->phase == JM_JAMMING && app->device) {
        s_jm_active = false;
        rollforge_rf_start_tx(app, jm_tx_cb, NULL);   // stoppt selbst passend zu rf_op
        s_jm_active = true;
        app->rf_op = RF_JAMMING;
    }
    snprintf(m->status, sizeof(m->status), "%s", JM_FREQ_LABELS[m->freq_idx]);
    break;
```
Einzige Änderung: die Zeile `subghz_devices_stop_async_tx(app->device);` entfällt in
beiden Zweigen. `app->rf_op` steht zu diesem Zeitpunkt noch auf `RF_JAMMING` (wird erst
danach neu gesetzt), also stoppt `rollforge_rf_start_tx()` → `rf_stop_current()` das
Gerät korrekt genau einmal, bevor es die neue Frequenz lädt und den Träger neu startet.

## Testhinweis (ohne Hardware)
Reine Zustandsmaschinen-Verletzung, host-nachstellbar ohne echten CC1101-Treiber.
`repro-jammer-double-stop/repro.c` bildet die relevanten Treiber-Zustände
(`Idle`/`AsyncTx`) plus `rf_stop_current()`/`rollforge_rf_start_tx()` als Modell nach
und prüft mit `assert()` exakt die vom `rf_stop_current()`-Kommentar beschriebene
Regel ("stop_async_tx braucht state==AsyncTx"), die im echten `furi_hal` per
`furi_check()` durchgesetzt wird:

```
$ gcc -O0 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c
$ ./repro bug   # vorher: direkter Stop im Input-Handler + interner Stop in rollforge_rf_start_tx()
Start: Geraet=AsyncTx, rf_op=JAMMING, Variante=vorher (Bug)
repro: repro.c:21: mock_stop_async_tx: Assertion `g_dev == DEV_ASYNC_TX && "stop_async_tx auf nicht-AsyncTx-Geraet (furi_check-Abbruch)"' failed.
Aborted (exit code 134)
$ ./repro fix   # nachher: nur noch der interne Stop in rollforge_rf_start_tx()
Start: Geraet=AsyncTx, rf_op=JAMMING, Variante=nachher (Fix)
Nach Frequenzwechsel: Geraet=AsyncTx, rf_op=JAMMING -- kein Abbruch.
exit code: 0
```
"vorher" bricht zuverlässig mit `SIGABRT` ab (steht für den `furi_check()`-Absturz auf
echter Hardware), "nachher" durchläuft denselben Frequenzwechsel-Ablauf ohne
Zustandsverletzung.

Funktionaler Test danach lokal auf dem Flipper: RollForge > Jammer, mit `[OK]` das
Jamming auf einer Frequenz starten, danach mehrfach Hoch/Runter drücken, um durch alle
vier Frequenzen zu wechseln. Vor dem Fix: Absturz/Neustart beim ersten Tastendruck
während `JM_JAMMING`. Nach dem Fix: Frequenzanzeige wechselt sauber durch, `[OK]`
stoppt das Jamming weiterhin normal (dieser Pfad war schon vorher korrekt, da dort nur
ein einziger direkter Stop-Aufruf steht, kein zweiter über `rollforge_rf_start_tx()`).
Da das Jamming aktiv einen Träger sendet: Test nur auf einer für eigene Zwecke
zugelassenen Frequenz/in HF-abgeschirmter Umgebung, wie ohnehin für dieses Modul
vorausgesetzt.

## Risiko
- Niedrig: reine Streichung von zwei redundanten, identischen Zeilen in `jm_input()`.
  Kein Eingriff in `rollforge_rf.c`, `jm_tx_cb()`, `jm_enter/exit/tick/draw` oder in
  die anderen RollForge-Module (RollJam-/RollLab-Tick-Module bleiben unberührt).
- Verhalten bei `InputKeyOk` (Jamming stoppen) ist bereits korrekt (genau ein
  Stop-Aufruf dort) und bleibt durch diesen Patch unverändert.
- Einzig denkbare Nebenwirkung: der Träger steht während des Frequenzwechsels minimal
  länger an, bis `rollforge_rf_start_tx()` intern stoppt, statt sofort beim
  Tastendruck — praktisch nicht messbar (ein Funktionsaufruf Unterschied), kein neues
  Sicherheits-/Missbrauchsrisiko.
- Deckt nur den beschriebenen Doppel-Stop-Pfad ab; der Rest des Jammer-Moduls
  (Frequenzliste, `jm_tx_cb`, Zeichnen) wurde mitprüft und zeigte keine weiteren
  Zustands- oder Pufferprobleme.

## Quelle
- `G4MEOVER18/RollForge` (main): `module_jammer.c` (`jm_input`, `s_jm_active`,
  `jm_tx_cb`), `rollforge_rf.c` (`rollforge_rf_start_tx`, `rf_stop_current`).
