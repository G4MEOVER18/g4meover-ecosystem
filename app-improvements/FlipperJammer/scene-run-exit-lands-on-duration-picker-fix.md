# FlipperJammer — Run-Scene-Ausgang landet auf der Dauer-Auswahl statt im Konfigurationsmenü

## Was
Nachdem meine eigene FlipperJammer-App bei einem früheren Tiefenaudit vollständig
durchgesehen war (alle `modules/*.c` plus `jammer_app.c`, siehe
`em4100-encoder-data-independent-output-fix.md` und
`hw-detect-otg-leak-after-exit-fix.md`), habe ich jetzt gezielt den bislang
ausgeklammerten Rest aufgegriffen: `scenes/` (18 Dateien, ~1900 Zeilen) — die
Scene-Manager-Navigation und das Dauer-/Start-Flow selbst waren noch nie
geprüft.

Fund: Jeder Modus, der eine Laufzeit wählen lässt ("Dauer wählen..."), schiebt
dafür die gemeinsame `Duration`-Scene zwischen die Konfigurationsscene und die
eigentliche Run-Scene:

```
Config --[Dauer wählen]--> Duration --[OK]--> Run
Config --[START]-------------------------------> Run   (nur wo ein direkter Start existiert)
```

`scene_duration.c`s `duration_enter_callback()` springt per
`scene_manager_next_scene(app->scene_manager, app->active_module)` zur Run-Scene
— das **pusht** sie nur, es ersetzt die `Duration`-Scene nicht. Der
Navigationsstapel wächst dadurch beim Dauer-Weg um eine Ebene mehr als beim
Direkt-Start-Weg (`Config, Duration, Run` statt `Config, Run`).

Jede der zehn betroffenen Run-Scenes (`SubGhzRun`, `Nrf24Run`, `BleRun`,
`WifiRun`, `IrRun`, `UsbFuzzerRun`, `RfidJamRun`, `NfcJamRun`, `RfidFuzzerRun`,
`NfcFuzzerRun`) verlässt sich beim automatischen Ausstieg (Dauer abgelaufen,
`0xDEAD`-Timer-Event) auf ein einfaches `scene_manager_previous_scene()` — das
poppt immer genau eine Ebene. Beim Direkt-Start-Weg ist das zufällig richtig
(landet auf der Konfigurationsscene). Beim Dauer-Weg poppt es nur die gerade
verlassene Run-Scene und landet auf der `Duration`-Scene selbst — der Nutzer
sieht nach Ablauf der gewählten Zeit (oder beim Drücken von Back, das mangels
eigener Behandlung des Back-Events im selben Fallback landet) unerwartet wieder
die Dauer-Auswahl statt das Konfigurationsmenü des gerade beendeten Moduls.

Für `RfidJamRun`/`NfcJamRun`/`RfidFuzzerRun`/`NfcFuzzerRun` ist das kein
Rand-, sondern der Regelfall: `scene_rfid_menu.c` bietet für diese vier Modi
**gar keinen** direkten Start-Weg — jeder einzelne Lauf geht ausschließlich
über `RfidMenu -> Duration -> Run`. Jedes Beenden eines dieser vier Modi
(Timer-Ablauf oder Back) landet deshalb unausweichlich auf der Dauer-Auswahl
statt im RFID/NFC-Menü, nicht nur gelegentlich.

## Fix
Additiv, zehn Dateien (`scenes/scene_{subghz,nrf24,ble,wifi,ir,usb_fuzzer,
rfid_jammer,nfc_jammer,rfid_fuzzer,nfc_fuzzer}.c`), jeweils in der Run-Scene's
`on_event()`-Funktion:

- Im bestehenden `0xDEAD`-Zweig (Timer abgelaufen) ersetzt
  `scene_manager_search_and_switch_to_previous_scene(app->scene_manager,
  Scene<Parent>)` das bisherige `scene_manager_previous_scene(...)`. Diese
  Flipper-eigene Scene-Manager-Funktion sucht die genannte Scene rückwärts im
  Navigationsstapel und kürzt ihn bis dorthin — unabhängig davon, ob dazwischen
  eine `Duration`-Scene liegt oder nicht, landet man immer exakt auf der echten
  Elternscene.
- Neuer `else if(event.type == SceneManagerEventTypeBack)`-Zweig mit demselben
  Aufruf, damit auch das Drücken der Zurück-Taste (bislang unbehandelt, fällt
  auf den Scene-Manager-Standard — ein einfaches Pop — zurück) konsistent zur
  Elternscene statt zur `Duration`-Scene zurückspringt.

Zielscene je Modul: `SubGhzConfig`/`Nrf24Config`/`BleConfig`/`WifiConfig`/
`IrConfig`/`UsbFuzzerConfig` für die sechs Module mit eigener
Konfigurationsscene, `RfidMenu` für die vier Module ohne eigene
Konfigurationsscene (deren einziger Einstieg bereits über `RfidMenu` läuft).

`scene_duration.c` selbst bleibt unverändert — der Fix sitzt bewusst auf der
Ausgangsseite der Run-Scenes, weil er dort unabhängig davon korrekt ist, ob die
`Duration`-Scene überhaupt im Stapel liegt (deckt beide Wege mit derselben
Codezeile ab, ohne den Eintritts-Pfad anzufassen).

`scene_rfid_spoofer.c`/`scene_nfc_spoofer.c` nutzen das Dauer-/Run-Muster nicht
(eigenständige Start/Stopp-Scene ohne Zeitlimit) und `scene_nrf24_diag.c`/
`scene_info.c` ebenfalls nicht — alle drei bewusst nicht angefasst, kein
Befund dort.

## Host-Repro
Da es sich um reine Navigationsstapel-Logik handelt (kein Hardware-, UART- oder
GPIO-Zugriff), habe ich die relevanten Scene-Manager-Funktionen
(`scene_manager_next_scene`/`scene_manager_previous_scene`/
`scene_manager_search_and_switch_to_previous_scene`) 1:1 als Host-Modell
nachgebaut (Push/Pop/Truncate-Semantik, gegen die eigentliche
Flipper-Firmware-Quelle abgeglichen) und für alle zehn betroffenen Module je
zwei Einstiegswege durchgespielt:

- **Via Dauer** (einziger Weg bei den vier RFID/NFC-Modulen, optionaler Weg
  bei den übrigen sechs): ALT (`scene_manager_previous_scene`) muss den Fund
  reproduzieren (landet auf `Duration`), NEU
  (`scene_manager_search_and_switch_to_previous_scene`) muss exakt auf der
  echten Elternscene landen.
- **Direkt-Start** (nur bei den sechs Modulen mit eigener
  Konfigurationsscene): ALT ist hier bereits zufällig korrekt, NEU darf das
  nicht verschlechtern.

32 Einzelprüfungen (10 Module × Dauer-Weg, 6 davon zusätzlich × Direkt-Start-Weg),
unter `gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined
-fno-sanitize-recover=undefined` bei `-O0`/`-O1`: alle 32 bestanden,
identischer Output beider Optimierungsstufen, keine Compiler-Warnungen, keine
Sanitizer-Befunde — in `repro-scene-run-exit-lands-on-duration-picker/repro.c`.

## Patches
Zehn Patches gegen zwei frische, unabhängige Zweitkopien von
`G4MEOVER18/FlipperJammer` (`6a3cd49`) mit `git apply --check` verifiziert, real
angewendet, je Datei per `md5sum` abgeglichen — identisch:

- `scene-run-exit-lands-on-duration-picker-fix/scene_subghz.patch`
- `scene-run-exit-lands-on-duration-picker-fix/scene_nrf24.patch`
- `scene-run-exit-lands-on-duration-picker-fix/scene_ble.patch`
- `scene-run-exit-lands-on-duration-picker-fix/scene_wifi.patch`
- `scene-run-exit-lands-on-duration-picker-fix/scene_ir.patch`
- `scene-run-exit-lands-on-duration-picker-fix/scene_usb_fuzzer.patch`
- `scene-run-exit-lands-on-duration-picker-fix/scene_rfid_jammer.patch`
- `scene-run-exit-lands-on-duration-picker-fix/scene_nfc_jammer.patch`
- `scene-run-exit-lands-on-duration-picker-fix/scene_rfid_fuzzer.patch`
- `scene-run-exit-lands-on-duration-picker-fix/scene_nfc_fuzzer.patch`

## Risiko
Rein kosmetisch/UX — keine Auswirkung auf die eigentliche Jamming-/Fuzzing-
Funktion selbst (Start/Stopp/Timer/Hardware-Ansteuerung bleiben unverändert,
der Fix ändert ausschließlich das Ziel des Navigationssprungs beim Verlassen
der Run-Scene). Kein Firmware-Risiko, da reine App-Ebene.

## Testhinweis
Gegen die eigene `mntm-012`-Basis mit `ufbt launch`/`fbt` bauen. Für jedes der
zehn Module: Dauer auf 10 Sekunden stellen, starten, Timer ablaufen lassen —
muss auf der jeweiligen Konfigurationsscene (bzw. bei RFID/NFC-Jammer/-Fuzzer
im RFID/NFC-Menü) landen statt auf der Dauer-Auswahl. Zusätzlich während des
Laufs Back drücken — gleiches Ziel. Gegenprobe mit dem direkten Start-Weg (wo
vorhanden): Back/Timer-Ablauf muss weiterhin wie bisher auf der
Konfigurationsscene landen (keine Regression).

## Offen
Weiterhin offen (unverändert aus vorherigen Sitzungen): `texkom` Phase 2-5
(Poller, Listener, Signalpreset, `furi_hal`-Low-Level-RF, Protokollbaum-
Registrierung/UI), NFC/RFID-Parser-Teilaspekt, MIFARE-Plus-/Ultralight-AES
Phase 3 Rest, PSA-Bruteforce-Krypto-Konstanten, `sanitize_filename()`-
Zeichenbereichs-Nebenfund, `usb-army-penetrator`s restliche Ducky-Handler,
neun verbleibende FeliCa-Multisystem-Teilfixe aus PR #4465, LFRFID-Hitag-S-
Dispatch-Integration (Hardware-Test), `hitagmicro.c`/`.h`-Dispatch-Integration
(blockiert an fehlender Config-Bitwert-Quelle), GlitchTrigger-Sweep-UI-Auto-
Hit-Status. `lora-ukfe` (eigene App, Quelle `G4MEOVER18/lora-ukfe`, nur die RF-
Console-Teile bereits geprüft) weiterhin nicht vollständig durchgesehen.
