# RollForge — manuelles "Save" zweimal in derselben Sekunde überschreibt die vorherige Aufnahme ersatzlos

## Was
`rollforge_storage.c::rollforge_save_sig()` baut den Dateinamen für eine gespeicherte
Aufnahme ausschließlich aus Stunde/Minute/Sekunde der Systemuhr:

```c
DateTime dt;
furi_hal_rtc_get_datetime(&dt);
char path[128];
snprintf(path, sizeof(path), "%s/%s_%02u%02u%02u.sub",
    RF_SAVE_DIR,
    filename_hint ? filename_hint : "capture",
    dt.hour, dt.minute, dt.second);

File* f = storage_file_alloc(storage);
bool ok = storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
```

`FSOM_CREATE_ALWAYS` legt die Datei an diesem Pfad an oder überschreibt sie
kommentarlos, falls sie schon existiert — es gibt keine Kollisionsprüfung. Diese
Funktion wird in beiden Modulen ausschließlich manuell per Tastendruck aufgerufen,
mit demselben Hint pro Bildschirm:

- `module_rolljam.c::rj_input()` — `[v]` (Down) in `RJ_CAPTURED` speichert Signal A
  unter dem Hint `"rj_A"`, `[v]` in `RJ_DONE` speichert Signal B unter `"rj_B"`.
- `module_rolllab.c::rl_input()` — `[<]` (Left) sowohl in `RL_ANALYZE_VIEW` als auch
  in `RL_REPLAY_READY` speichert `ref_sig` jeweils unter demselben Hint `"rl_ref"`.

Drücke ich die Save-Taste zweimal, während die Sekunde der Systemuhr gleich bleibt
(ein plausibler Bedienfall — "sicherheitshalber nochmal speichern", ein versehentlicher
Doppeldruck, oder einfach zwei Saves auf verschiedenen Bildschirmen desselben Signals
wie bei `rl_ref`), erzeugen beide Aufrufe exakt denselben Pfad. Der zweite
`storage_file_open(..., FSOM_CREATE_ALWAYS)` ersetzt die Datei des ersten Saves
vollständig — ohne jede Rückmeldung, die sich von einem normalen, kollisionsfreien
Speichern unterscheidet: beide Male zeigt die UI identisch `"Gespeichert"`
(`"A gespeichert"`/`"B gespeichert"`/`"Gespeichert"`). Die erste Aufnahme ist damit
unwiederbringlich von der SD-Karte verschwunden, ohne dass ich das beim Bedienen
bemerke.

Das trifft den eigentlichen Zweck des Tools direkt: Bei RollJam/RollLab ist genau das
gespeicherte Signal (Signal B bzw. die Referenzaufnahme) der Wert, den ich für einen
späteren Zugriff oder eine spätere Auswertung brauche — sein stillschweigender Verlust
durch einen zweiten Tastendruck innerhalb derselben Sekunde ist nicht wiederherstellbar,
da der zugehörige RAM-Puffer (`sig_a`/`sig_b`/`ref_sig`) beim nächsten Durchlauf
ohnehin überschrieben wird.

## Nutzen
- Zwei Aufnahmen, die zufällig in dieselbe Sekunde fallen, bleiben beide auf der
  SD-Karte erhalten, statt dass die zweite die erste kommentarlos ersetzt.
- Der Normalfall (ein Save pro Sekunde — der weit überwiegende praktische Fall)
  bleibt byte-identisch: derselbe Pfad wie bisher, kein unnötiger Suffix.
- Reine Dateinamens-/Kollisionslogik in `rollforge_storage.c`. Keine Änderung an
  `module_rolljam.c`, `module_rolllab.c`, `rollforge_rf.c` oder am RAW-Dateiformat
  selbst (Header, Datenzeile) — nur der Pfad, unter dem geschrieben wird, kann sich
  bei einer tatsächlichen Kollision ändern.

## Betroffene Dateien (lokal, in meinem RollForge-Repo)
- `rollforge_storage.c` — `rollforge_save_sig()`: nach dem Aufbau des
  Standard-Pfads per `storage_file_exists()` auf eine bereits vorhandene Datei
  prüfen; bei Kollision einen laufenden Zähler-Suffix (`_1`, `_2`, …) anhängen,
  bis ein freier Pfad gefunden ist, bevor die Datei angelegt wird.

## Patch
Siehe `save-same-second-filename-collision-fix/rollforge_storage_c.patch`
(`git apply --check` vorab gegen eine frische, unabhängige Kopie von
`G4MEOVER18/RollForge` (`main`, Commit `cc23cd486`) verifiziert, real angewendet
und byte-für-byte mit der Arbeitskopie abgeglichen — identisch).

## Testhinweis (ohne Hardware)
Die echte, unverändert mitkompilierte `rollforge_storage.c` (einmal vor, einmal nach
dem Patch, jeweils nur `rollforge_save_sig` zu `_ALT`/`_NEU` umbenannt, sonst kein
Zeichen geändert) gegen minimale Mocks für `storage_file_*`/`furi_record_*`/
`furi_hal_rtc_get_datetime` (ein virtuelles In-Memory-Dateisystem, das
`FSOM_CREATE_ALWAYS` exakt wie der echte Flipper-Storage-Treiber nachbildet: Datei am
selben Pfad wird beim zweiten Schreiben ersetzt, nicht ergänzt):

```sh
gcc -std=c11 -O0 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined \
  -I. -o repro mocks.c rollforge_storage_ALT.c rollforge_storage_NEU.c repro_main.c
./repro
```

`app-improvements/RollForge/repro-save-same-second-filename-collision/` — bei mir
lokal unter `-O0` und `-O1` identisch durchgelaufen (vier Szenarien): (1) zwei
verschiedene Aufnahmen, derselbe Hint, dieselbe Sekunde — ALT: nur noch eine Datei,
Inhalt der ersten Aufnahme nicht mehr auffindbar; NEU: beide Dateien, beide Inhalte
erhalten; (2) Regression — nur ein Save in dieser Sekunde: Pfad bei NEU identisch zu
ALT (kein unnötiger Suffix im Normalfall); (3) drei Saves in derselben Sekunde: NEU
behält alle drei getrennt; (4) Regression — zwei verschiedene Hints (`rj_A`/`rj_B`)
in derselben Sekunde kollidieren bei NEU nicht (unverändertes Verhalten, da bereits
vorher kollisionsfrei). `ALL CHECKS PASSED`, keine ASan-/UBSan-Meldungen, keine
Compiler-Warnungen bei beiden Optimierungsstufen.

Funktionaler Test nach dem Patch, lokal auf dem Flipper: im RollLab-Modul einmal bis
`RL_ANALYZE_VIEW` capturen, dort zweimal schnell hintereinander `[<]` ("Save")
drücken (innerhalb derselben Sekunde der angezeigten Uhrzeit) — auf der SD-Karte
müssen danach zwei getrennte `.sub`-Dateien unter `/ext/subghz/rollforge/` liegen
(`rl_ref_HHMMSS.sub` und `rl_ref_HHMMSS_1.sub`), nicht nur eine.

## Risiko
- Sehr gering: rein additive Kollisionsprüfung vor dem bestehenden
  `storage_file_open(..., FSOM_CREATE_ALWAYS)`-Aufruf, der selbst unverändert bleibt.
- Der mit Abstand häufigste Fall (ein Save pro Sekunde) nimmt exakt denselben Pfad
  wie bisher — durch den Host-Repro-Regressionsfall (Szenario 2) bestätigt.
- Obergrenze von 99 Kollisionsversuchen pro Sekunde, danach wird (wie bisher) der
  letzte berechnete Pfad verwendet und ggf. überschrieben — in der Praxis (eine
  Taste, menschliche Reaktionszeit) wird dieser Fall nie erreicht; bewusst nicht
  mit einer eigenen Fehlerrückmeldung für diesen theoretischen Grenzfall verkompliziert.
- Kein Einfluss auf den separat dokumentierten, weiterhin offenen Fund
  `rj-b-premature-saved-claim-fix.md` (Statuszeile behauptet "B saved" vor dem
  eigentlichen Speichern) — dieser Patch ändert nichts an der Status-/Phasenlogik
  in `module_rolljam.c`, nur an `rollforge_save_sig()` selbst.

## Quelle
- `G4MEOVER18/RollForge` (main, Commit `cc23cd486`): `rollforge_storage.c`
  (`rollforge_save_sig`), `module_rolljam.c` (`rj_input`, Aufrufstellen),
  `module_rolllab.c` (`rl_input`, Aufrufstellen) — eigener Fund beim gezielten
  Gegenprüfen der Speicherlogik im Anschluss an den bereits dokumentierten,
  benachbarten Fund `rj-b-premature-saved-claim-fix.md` (derselbe Aufrufpfad, andere
  Schicht: dort die Statusmeldung, hier die tatsächliche Dateischreiblogik).
- API-87-Einordnung: `storage_file_exists()` ist Teil der seit Jahren stabilen
  `storage/storage.h`-Kern-API (gegen `applications/services/storage/storage.h` im
  eigenen `G4MEOVER-FW`-Stand verifiziert), keine neue Firmware-API, kein
  Kompatibilitätsrisiko.
