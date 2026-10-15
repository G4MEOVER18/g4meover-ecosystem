# SubGHz: unbekannter Preset-Name haelt die ganze App an statt nur den Vorgang abzubrechen

## Was

Ausgangspunkt war derselbe Unleashed-`dev`-Commit, den ich in einer vorherigen Sitzung schon
einmal aufgegriffen habe: `e1ebaa3` ("SubGHz: safe exit on unknown preset, fix Security+ 2.0
transmitter leak", schliesst Issues #1159/#1161). Den kleineren, vollstaendig eigenstaendigen
Teil (Security+-2.0-Transmitter-Leak in `subghz_txrx_gen_secplus_v2_protocol()`) hatte ich
bereits portiert und gemerged. Den groesseren Teil -- fuenf Dateien, API-Aenderung von
`furi_crash()` auf einen `-1`/`NULL`-Rueckgabewert ueber mehrere Aufrufer hinweg -- hatte ich
damals als eigenen Rueckstellpunkt vermerkt, weil er fuer einen einzelnen Beitrag zu gross
schien. Erneut gegen den `dev`-Branch geprueft (weiterhin derselbe Commit, kein neuerer Stand
seitdem) und jetzt als eigenen, vollstaendigen Beitrag umgesetzt.

`subghz_setting_get_inx_preset_by_name()` (`lib/subghz/subghz_setting.c`) durchsucht die Liste
geladener Presets nach einem Namen; findet sie keinen, ruft sie bislang `furi_crash("SubGhz: No
name preset.")` auf -- ein nicht fortsetzbarer Halt der gesamten Firmware. `furi_crash()` ist in
meiner Basis (wie in Unleashed) als Debug-Build ein `__breakpoint()` bzw. in Release ein
`furi_hal_crash()` samt Reset; in beiden Faellen stirbt die App, nicht nur der aktuelle Vorgang.

Vier Aufrufer koennen diesen Pfad mit einem Preset-Namen erreichen, der tatsaechlich nicht
(mehr) existiert -- keiner davon ist ein seltener Grenzfall:

- `subghz_txrx_tx_start()`/`subghz_txrx_rx_start()` (`applications/main/subghz/helpers/
  subghz_txrx.c`): der aktuell gesetzte `instance->preset->name` wird direkt an
  `subghz_setting_get_preset_data_by_name()` gereicht, die intern denselben
  Index-Lookup aufruft. Ein per RPC/mobiler App oder per `.sub`-Datei mit einem inzwischen
  geloeschten Custom-Preset gesetzter Name trifft hier zu.
- `subghz_key_load()` (`applications/main/subghz/subghz_i.c`): laedt den Preset-Namen direkt aus
  der Nutzdatei (`Preset`-Feld) -- ein von Hand bearbeitetes, aus einer aelteren
  Firmware-Version stammendes oder schlicht fehlerhaftes `.sub` mit einem unbekannten
  Preset-Namen reicht.
- `subghz_scene_receiver_config_var_list_enter_callback()`
  (`applications/main/subghz/scenes/subghz_scene_receiver_config.c`, Zweig
  "Reset to default"): nutzt den aktuell aktiven Preset-Namen, der zum Zeitpunkt des Zuruecksetzens
  bereits aus der Liste entfernt worden sein kann (z. B. External-Modul-Presets nach
  Modul-Wechsel).

In allen vier Faellen ist "unbekannter Preset-Name" ein erwartbarer Eingabefehler -- keine
Speicherverletzung, kein API-Missbrauch -- und sollte wie jeder andere Parsing-/Validierungsfehler
behandelt werden (Logeintrag, Vorgang abbrechen), nicht die Firmware zum Absturz bringen.

## Nutzen

Verwandelt einen durch gaengige, nicht praeparierte Eingaben (manipuliertes/altes `.sub`,
geloeschter Custom-Preset, Modulwechsel) auslösbaren Totalabsturz der Firmware in einen
kontrollierten, geloggten Fehlerfall, der nur den einzelnen Vorgang (Laden/Senden/Empfangen/
Zuruecksetzen) abbricht. Rein additiv an den vier Aufrufstellen, keine Verhaltensaenderung im
Erfolgsfall (bekannter Preset-Name).

## Betroffene Dateien

- `lib/subghz/subghz_setting.c` -- `subghz_setting_get_inx_preset_by_name()`: `furi_crash()` durch
  `FURI_LOG_E()` ersetzt, Funktion kehrt bei Nichttreffer regulaer mit `-1` zurueck (das tat sie
  vorher implizit nie, weil `furi_crash()` nicht zurueckkehrt). `subghz_setting_get_preset_data_by_name()`:
  prueft den `idx`-Rueckgabewert jetzt vor dem Array-Zugriff und liefert `NULL` statt eines
  Zugriffs mit `idx == -1`.
- `lib/subghz/subghz_setting.h` -- Doc-Kommentare an beiden Funktionsdeklarationen ergaenzt
  (dokumentiert den jetzt tatsaechlich erreichbaren `-1`-/`NULL`-Rueckgabefall).
- `applications/main/subghz/helpers/subghz_txrx.c` -- `subghz_txrx_tx_start()`: holt den
  Preset-Datenzeiger jetzt in eine lokale Variable, bevor sie unterschieden werden, prueft sie
  auf `NULL` und faellt in genau denselben, bereits bestehenden "Unknown name preset"-Fehlerpfad,
  der bisher nur den Fall eines leeren Namens behandelte. `subghz_txrx_rx_start()`: prueft den
  Datenzeiger auf `NULL`, loggt und bricht ab, statt mit einem ungueltigen Zeiger weiterzumachen.
- `applications/main/subghz/subghz_i.c` -- `subghz_key_load()`: `preset_index` von `size_t` auf
  `int` umgestellt, bricht bei `-1` mit Logeintrag aus der bestehenden `do { ... } while(false)`-
  Fehlerkette ab (identisches Muster zu den zehn bereits vorhandenen `break`-Stellen derselben
  Funktion).
- `applications/main/subghz/scenes/subghz_scene_receiver_config.c` --
  `subghz_scene_receiver_config_var_list_enter_callback()`: faellt bei `-1` auf den ohnehin schon
  vorhandenen `default_index` zurueck, statt den ungueltigen Index weiterzureichen.

## Risiko

Gering. Alle fuenf Aenderungen sind additiv (neue Prüfungen, kein geänderter Erfolgspfad) oder
rein kosmetisch (Header-Kommentare). Die einzige Signaturaenderung ist `size_t preset_index` zu
`int preset_index` in `subghz_key_load()` -- rein lokal, keine andere Funktion nimmt diese
Variable als Parameter. Kein Aufrufer von `subghz_setting_get_inx_preset_by_name()`/
`subghz_setting_get_preset_data_by_name()` verlaesst sich anderswo im Quellbestand auf den
bisherigen `furi_crash()`-Stop als Kontrollflusskonstrukt -- per `grep` ueber den gesamten
Quellbestand nach beiden Funktionsnamen verifiziert, es gibt keine weiteren, hier nicht
behandelten Aufrufer.

## Port-Skizze

1. Fuenf Patches in dieser Reihenfolge gegen die eigene Firmware anwenden (`git apply --check`
   vorab, dann real) -- Reihenfolge ist unabhaengig, da sich keine zwei Patches ueberlappen:
   - `firmware-mining/subghz-safe-exit-unknown-preset/subghz_setting_c.patch`
   - `firmware-mining/subghz-safe-exit-unknown-preset/subghz_setting_h.patch`
   - `firmware-mining/subghz-safe-exit-unknown-preset/subghz_txrx_c.patch`
   - `firmware-mining/subghz-safe-exit-unknown-preset/subghz_i_c.patch`
   - `firmware-mining/subghz-safe-exit-unknown-preset/subghz_scene_receiver_config_c.patch`
2. Mit `ufbt`/`fbt firmware_all` bauen.
3. Funktionstest (siehe unten).

## Testhinweis

Lokal, ohne Hardware verifizierbar als reine Logikprüfung (siehe Host-Repro unten) -- der
unbekannte-Preset-Fall ist auf dem Geraet selbst am einfachsten ueber ein handgebautes `.sub`
auszuloesen: eine gespeicherte Datei nehmen, das `Preset:`-Feld auf einen erfundenen Namen
setzen (z. B. `FuriHalSubGhzPresetCustomDoesNotExist`), per Dateimanager zurueck in `SubGHz/`
kopieren und im SubGHz-App-Hauptmenue oeffnen ("Parsed"/"Saved"). Vor dem Fix stuerzt die
Firmware beim Oeffnen/Senden dieser Datei ab (Reset); nach dem Fix erscheint ein
Fehler-Log-Eintrag (`[SubGhz] Unknown Preset` bzw. `[SubGhzTxRx] Unknown name preset "..."`,
sichtbar per `fbt cli`/`storage.log`), die App bleibt bedienbar, nur das Laden/Senden dieser
einen Datei schlaegt fehl. Funktional fuer alle regulaeren, mit real existierenden Presets
gespeicherten Dateien unveraendert (vor und nach dem Fix identisch).

## Host-Repro

`firmware-mining/repro-subghz-safe-exit-unknown-preset/repro.c`: bildet
`subghz_setting_get_inx_preset_by_name()`, `subghz_setting_get_preset_data_by_name()` sowie
stellvertretend fuer die vier echten Aufrufer das `subghz_txrx_rx_start()`-Muster ALT
(unveraendert) und NEU (mit dem portierten Fix) nach. `furi_crash()` wird exakt wie im
bestehenden `stub_furi_crash.c`-Muster dieses Repos durch einen sofortigen, erkennbaren
Prozessabbruch (`_exit(42)`) nachgebildet.

```
gcc -std=c11 -O1 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-sanitize-recover=undefined -o repro repro.c
./repro known   alt   # bekannter Preset, ALT
./repro known   neu   # bekannter Preset, NEU
./repro unknown alt   # unbekannter Preset, ALT
./repro unknown neu   # unbekannter Preset, NEU
```

Ergebnis (Patches gegen zwei frische, unabhaengige Zweitkopien von `G4MEOVER18/G4MEOVER-FW`
mit `git apply --check` verifiziert, real angewendet, byte-fuer-byte abgeglichen -- identisch;
Repro separat unter `-O0` und `-O1` ausgefuehrt, keine Compiler-/Sanitizer-Warnungen in beiden
Stufen):

| Szenario           | ALT                                  | NEU                                              |
|---------------------|---------------------------------------|---------------------------------------------------|
| bekannter Preset    | Exit 0, RX gestartet                 | Exit 0, RX gestartet (identisch)                  |
| unbekannter Preset  | Exit 42 (`furi_crash()` ausgeloest)  | Exit 0, Fehler geloggt, RX nicht gestartet, Prozess am Leben |

Vier aufeinanderfolgende Laeufe je Kombination deterministisch reproduziert.

## Offen / zurückgestellt

Die seit längerem zurückgestellten Backlog-Punkte (NFC/RFID-Parser-Teilaspekt,
MIFARE-Plus-/Ultralight-AES Phase 3 Rest, PSA-Bruteforce-Krypto-Konstanten,
`sanitize_filename()`-Zeichenbereichs-Nebenfund, `usb-army-penetrator`s restliche
Ducky-Handler, `texkom`/`hitagmicro.c` aus dem Unleashed-dev-Diff, Hitag-S-Phase-2-
Integrationsschicht, neun verbleibende FeliCa-Multisystem-Teilfixe aus PR #4465,
`g4meover-deck`s `flipper_link.py`-Bestätigung) bleiben unverändert offen. Damit ist der
gesamte Inhalt des Unleashed-Commits `e1ebaa3` jetzt vollständig in diesem Repo erfasst (beide
Teile portiert).

## Quelle

Unleashed-Firmware (`DarkFlippers/unleashed-firmware`), `dev`-Branch, Commit `e1ebaa3`
("SubGHz: safe exit on unknown preset, fix Security+ 2.0 transmitter leak", schließt Issues
#1159/#1161).
