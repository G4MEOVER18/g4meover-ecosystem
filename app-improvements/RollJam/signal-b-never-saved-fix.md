# RollJam — Signal B wird nie gespeichert, "Signal B still valid" ist eine leere Behauptung

## Was

Meine eigenständige `RollJam`-App fängt in Phase 1/2 zwei Signale ein: Signal A (wird
sofort in Phase 3 wiedergegeben, um das Fahrzeug zu öffnen) und Signal B — der
eigentliche Witz des RollJam-Angriffs, der "Ersatzcode", der durch das fortgesetzte
Jammen während der zweiten Tastenbetätigung des Keyfobs vom Fahrzeug nie akzeptiert
wurde und deshalb für eine spätere Nutzung gültig bleibt. Der `DONE`-Bildschirm
(`rj_draw_cb()`, `rolljam_app.c`) zeigt das auch so an:

```c
case ROLLJAM_DONE:
    ...
    canvas_draw_str(canvas, 0, 32, "Signal B still valid.");
```

Ich habe `rolljam_rf.c`/`.h` vollständig gelesen: es gibt in der gesamten App keine
einzige Zeile, die `app->signal_b` irgendwohin schreibt. Kein `storage_file_*`-Aufruf,
kein `#include <storage/storage.h>`, keine andere Export-/Speicherfunktion — Signal B
existiert ausschließlich im RAM der laufenden App-Instanz (`RollJamApp.signal_b`,
`rolljam_app.h`). Verlasse ich die App (egal ob regulär über "Exit" auf `DONE`, über
die Zurück-Taste, oder weil ich das Flipper-Menü wechsle), läuft `rolljam_rf_deinit()`
und am Ende `free(app)` (`rolljam_app.c`, Cleanup-Block) — Signal B ist in diesem
Moment unwiderruflich weg. Die Behauptung "Signal B still valid" auf dem Bildschirm,
den ich als Letztes vor dem Verlassen sehe, ist zu diesem Zeitpunkt bereits oder in
Kürze schlicht falsch: es gibt keinen Weg, auf den ich den Code später noch anwenden
könnte, selbst wenn das Fahrzeug ihn immer noch akzeptieren würde.

Das ist ein anderer Fehlertyp als der bereits von mir dokumentierte und behobene
`RollForge`-Fund (`app-improvements/RollForge/rj-b-premature-saved-claim-fix.md`):
dort hat `module_rolljam.c` eine *manuelle* `[v]`-Speicherfunktion für Signal B, nur
mit einer verfrühten Erfolgsbehauptung (Statuszeile sagt "gespeichert", bevor der
Speicherversuch tatsächlich lief). In der eigenständigen `RollJam`-App gibt es diese
manuelle Speicherfunktion gar nicht erst — die Behauptung "still valid" ist nicht nur
verfrüht, sondern unter jeder Umgebungsbedingung falsch, weil kein Codepfad existiert,
der Signal B je auf die SD-Karte schreiben könnte.

Die Schwester-App `RollForge` löst genau dieses Problem bereits mit
`rollforge_save_sig()` (`rollforge_storage.c`): eine .sub-RAW-Datei im bekannten
Flipper-SubGhz-Format, basierend auf demselben gepackten Puffer-Layout
(`bit31 = level, bits 0-30 = duration`), das `RollJamSignal` identisch verwendet. Ich
übertrage dieselbe, bereits produktiv genutzte Serialisierung nach `RollJam`, statt ein
neues Format zu erfinden.

## Nutzen

- Signal B — der eigentliche Mehrwert des RollJam-Angriffs — überlebt das Verlassen
  der App tatsächlich, statt beim nächsten `free(app)` spurlos zu verschwinden.
- Die Behauptung auf dem `DONE`-Bildschirm spiegelt jetzt den tatsächlichen Zustand:
  "Signal B saved to SD." nur, wenn die Speicherung wirklich gelungen ist, sonst
  "Signal B NOT saved." (z. B. SD-Karte voll oder entfernt) statt einer
  bedingungslosen, in Wahrheit nie erfüllten Zusage.
- Wiederverwendung eines bereits bewährten, identischen Dateiformats
  (`rollforge_save_sig()`) statt einer neuen, ungetesteten Serialisierung — die beiden
  Schwester-Apps erzeugen für dasselbe gepackte Puffer-Layout exakt dieselbe
  .sub-RAW-Ausgabe.
- Reine Ergänzung: Jam-/Capture-/Replay-Timing, die bereits behobene
  interlaced-Puffer-Logik (`cap_started`/`rolljam_capture_resume()`) und Signal A
  bleiben vollständig unverändert.

## Betroffene Dateien

- `rolljam_rf.h` — neues Makro `ROLLJAM_SAVE_DIR`, neue Deklaration
  `rolljam_rf_save_signal()`. Patch: `signal-b-never-saved-fix/rolljam_rf_h.patch`.
- `rolljam_rf.c` — neue Includes (`storage/storage.h`, `furi_hal_rtc.h`, `stdio.h`),
  neue Funktion `rolljam_rf_save_signal()` (Byte-für-Byte identische Serialisierung
  zu `rollforge_save_sig()`, nur umbenannt und auf `RollJamSignal`/`ROLLJAM_SAVE_DIR`
  umgestellt). Patch: `signal-b-never-saved-fix/rolljam_rf_c.patch`.
- `rolljam_app.h` — neues Feld `bool signal_b_saved;` in `RollJamApp`. Patch:
  `signal-b-never-saved-fix/rolljam_app_h.patch`.
- `rolljam_app.c`:
  - Init-Block in `rolljam_app()`: `app->signal_b_saved = false;`.
  - `ROLLJAM_PHASE2_JAMMING`-Fall: bei `captured == true` wird
    `rolljam_rf_save_signal(&app->signal_b, app->frequency, "signal_b")` aufgerufen,
    das Ergebnis in `app->signal_b_saved` übernommen und die Statuszeile passend
    gesetzt ("B saved" / "B save FAILED"). Bei `captured == false` unverändert
    "B skipped", zusätzlich `signal_b_saved = false` (kein Speicherversuch auf ein
    leeres Signal).
  - `rj_draw_cb()`, `ROLLJAM_DONE`-Fall: zeigt jetzt `app->signal_b_saved`-abhängig
    "Signal B saved to SD." oder "Signal B NOT saved." statt der bisherigen,
    bedingungslosen Zeile.
  - Patch: `signal-b-never-saved-fix/rolljam_app_c.patch`.

Keine Änderung an `rj_run_jam_capture_phase()`, den Jam-/Capture-/Replay-Callbacks in
`rolljam_rf.c`, `rolljam_states.h` oder Signal A.

## Patch

Vier additive Patches in `signal-b-never-saved-fix/` (`rolljam_rf_h.patch`,
`rolljam_rf_c.patch`, `rolljam_app_h.patch`, `rolljam_app_c.patch`), jeweils gegen eine
frische, unabhängige Kopie von `G4MEOVER18/RollJam` (`main`, Commit `1f90c00e1`)
erzeugt und verifiziert: einmal per `patch -p1` (`--dry-run` vorab, dann real) gegen
eine erste Zweitkopie, einmal per `git apply --check`/real gegen eine zweite,
unabhängige Zweitkopie angewendet — alle vier betroffenen Dateien laut `md5sum` über
beide Ergebnisse byte-identisch.

Die Patches sind unabhängig von den bereits dokumentierten, noch nicht übernommenen
`abort-right-key-quits-app-fix`/`exit-button-left-key-unhandled-fix` (unterschiedliche
Funktionen/Zeilenbereiche: `ROLLJAM_PHASE2_JAMMING` und der `ROLLJAM_DONE`-Zweig von
`rj_draw_cb()` werden von keinem der beiden anderen Patches berührt) — sie lassen sich
in beliebiger Reihenfolge übernehmen.

## Host-Repro (ohne Hardware)

`repro-signal-b-never-saved/repro.c`:

- **Teil A** transkribiert die reine Byte-Erzeugung aus `rolljam_rf_save_signal()`
  (Header-String + `RAW_Data:`-Zeile), `storage_file_write()`/
  `furi_hal_rtc_get_datetime()` durch einen wachsenden Puffer bzw. ein injiziertes
  `DateTime` ersetzt. 5 Szenarien: leeres Signal/`NULL` werden abgelehnt; vier Flanken
  ergeben exakt den erwarteten Header- und `RAW_Data`-String samt korrektem
  Dateinamen-Format; der 31-Bit-Maskierungs-Grenzwert `0x7FFFFFFF` wird ohne
  Overflow/UB als `2147483647` formatiert; ein volles 2048-Element-Signal erzeugt
  exakt 2048 RAW-Werte ohne Abschneiden.
- **Teil B** transkribiert die neue Aufrufstellen-Logik in `ROLLJAM_PHASE2_JAMMING`
  (Speicherfunktion durch einen Stub mit einstellbarem Rückgabewert ersetzt). 3
  Szenarien: Speicherung gelingt (`signal_b_saved=true`, Status "B saved"); Speicherung
  schlägt fehl, z. B. SD-Karte voll/entfernt (`signal_b_saved=false`, Status warnt
  statt bedingungslos "still valid" zu behaupten — der eigentliche Fund, in ALT gibt es
  diesen Fall gar nicht, weil kein Speicherversuch je unternommen wird); Regression bei
  `captured == false` (unverändertes "B skipped", kein Speicherversuch).

```sh
gcc -std=c17 -Wall -Wextra -O0 -fsanitize=address,undefined -fno-sanitize-recover=undefined repro.c -o repro_o0
gcc -std=c17 -Wall -Wextra -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined repro.c -o repro_o1
./repro_o0 && ./repro_o1
```

Bei mir lokal reproduziert (beide Optimierungsstufen, ASan/UBSan): `ALL CHECKS PASSED`,
keine Sanitizer-Befunde, keine Compiler-Warnungen.

## Testhinweis

Lokal auf dem Flipper (CC1101 int oder ext, keine zweite Sendequelle nötig außer dem
eigenen Testsender/Keyfob für die Capture-Phasen selbst):

1. `ufbt`/`fbt` bauen, App starten, einen vollständigen Durchlauf (Phase 1 → 2 → 3 →
   `Done!`) mit eingelegter, beschreibbarer SD-Karte durchführen.
2. Auf `DONE` muss "Signal B saved to SD." erscheinen (vorher: bedingungslos
   "Signal B still valid.", unabhängig vom tatsächlichen Speicherzustand).
3. Nach Verlassen der App: `/ext/subghz/rolljam/signal_b_HHMMSS.sub` muss auf der
   SD-Karte existieren und sich über das normale Flipper-SubGhz-Menü als RAW-Datei
   laden lassen (Frequenz/Preset aus dem Header, `RAW_Data`-Zeile mit den erfassten
   Flanken) — exakt das Dateiformat, das `RollForge` für dasselbe Szenario bereits
   produktiv erzeugt.
4. Regressionsfall: SD-Karte vor Phase 2 entfernen (oder einen bereits vollen/
   schreibgeschützten Datenträger verwenden) — `DONE` muss "Signal B NOT saved."
   zeigen, Phase 3 (Replay von Signal A) muss davon unberührt normal weiterlaufen.
5. Regressionsfall: Phase 2 absichtlich ins Timeout laufen lassen (`captured == false`)
   — Statuszeile "B skipped — replaying A" bleibt wie bisher, kein Speicherversuch,
   `DONE` zeigt "Signal B NOT saved." (korrekt, da B nie erfasst wurde).

## Risiko

- Niedrig: rein additiv, keine Änderung an Timing, Zustandsautomat oder den bereits
  bestehenden RF-Callbacks. Die neue Funktion wird ausschließlich beim Übergang
  `PHASE2_JAMMING → PHASE3_REPLAY` aufgerufen, einmalig pro Durchlauf.
- Neue Laufzeitabhängigkeit auf `Storage`/RTC-Hardware-Rekorde (`RECORD_STORAGE`), die
  in der App bisher nicht genutzt wurden — identisch zur bereits produktiv laufenden
  Nutzung in `RollForge`, dasselbe Firmware-API (`storage_file_open/write/close/free`,
  `furi_hal_rtc_get_datetime`), kein neues Risiko gegenüber der Schwester-App.
- Scheitert die Speicherung (SD fehlt/voll/schreibgeschützt), bricht die App nicht ab
  — `rolljam_rf_save_signal()` gibt `false` zurück, der Rest von Phase 2/3 läuft
  unverändert weiter, nur die Statuszeile und der `DONE`-Bildschirm spiegeln den
  Fehlschlag.
- Erzeugt beim Testen echte `.sub`-Dateien auf der SD-Karte — das sind meine eigenen,
  mit meinem eigenen Sender erfassten Testsignale für autorisierte Tests an meinen
  eigenen Geräten, keine Fremd-Kartendaten oder echten Zugangsdaten Dritter.

## Quelle

- `G4MEOVER18/RollJam` (main, Commit `1f90c00e1`): `rolljam_rf.c/.h`,
  `rolljam_app.c/.h` (`rj_draw_cb()`, `rolljam_app()`-Init-Block,
  `ROLLJAM_PHASE2_JAMMING`-Fall).
- `G4MEOVER18/RollForge` (main, Commit `cc23cd486`): `rollforge_storage.c/.h`
  (`rollforge_save_sig()`, `RF_SAVE_DIR`) — Vorlage für das identische
  .sub-RAW-Dateiformat und die Übertragung des gepackten Puffer-Layouts.
- Eigener vorheriger, verwandter Fund (anderer Fehlertyp, zum Vergleich):
  `app-improvements/RollForge/rj-b-premature-saved-claim-fix.md` (dort: verfrühte
  Erfolgsbehauptung bei vorhandener manueller Speicherfunktion; hier: Speicherfunktion
  fehlt in der eigenständigen App vollständig).
