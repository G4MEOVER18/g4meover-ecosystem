# lilygo-ukfe-rx — ESP-NOW-seitiger Anti-Replay-Counter der `ukfe_rf`-Konsole öffnet das Replay-Fenster nach jedem Neustart

## Was
Ich habe `lilygo-ukfe-rx` geprüft — den LilyGo-T-Dongle-S3-Satelliten, der signierte `ukfe_rf`-
Befehle vom WROOM-Relay per ESP-NOW empfängt und direkt am angeschlossenen Zielrechner als
natives USB-HID ausführt (`src/main.cpp`, `onEspNowRecv()`/`loop()`/`act()`). Das Repo war
bisher in keinem meiner `app-improvements/`-Durchgänge erfasst — ich habe es dieses Mal
gezielt aufgegriffen, nachdem `RollJam`, `usb-army-penetrator`, `lora-ukfe` und `heltec-v4-hub`
in den letzten Sitzungen bereits an der Reihe waren.

Das gemeinsame `ukfe_rf`-Protokoll (`src/ukfe_rf.c`/`.h`, laut eigenem Dateikommentar
"auf beiden Seiten identisch implementieren", byte-für-byte identisch mit der bereits in
`usb-army-penetrator`/`heltec-v4-hub` geprüften Fassung) erzwingt einen strikt monoton
steigenden, MAC-authentifizierten Rolling-Counter als Anti-Replay-Schutz:

```c
if(last_counter) {                                        // Anti-Replay
    if(m.counter <= *last_counter) return false;          // verbraucht/replay
    *last_counter = m.counter;
}
```

In `lilygo-ukfe-rx` wird dieser Mechanismus für die WROOM→LilyGo-Richtung so angebunden
(`src/main.cpp`):

```cpp
static uint32_t enowCounter = 0;     // Anti-Replay-Fenster (WiFi)
...
void loop() {
    ...
    if(ukfe_rf_parse_frame(UKFE_SECRET, frame, real_len, &msg, &enowCounter)) {
        okCount++;
        ...
        act(&msg);
        send_espnow_ack(enowSenderMac, msg.cmd, 0);
    }
}
```

`enowCounter` ist ein reines RAM-Feld (statische globale Variable, Startwert `0`). Ich habe
den gesamten Baum nach `Preferences`/`nvs_` durchsucht (`grep -rn "Preferences\|nvs_" src/`) —
kein Treffer, `main.cpp` bindet `<Preferences.h>` bisher gar nicht ein. Das ist exakt derselbe
Fund wie bereits in `app-improvements/usb-army-penetrator/rf-counter-replay-window-reset-on-reboot-fix.md`
und `app-improvements/heltec-v4-hub/rf-counter-replay-window-reset-on-reboot-fix.md`
dokumentiert, hier aber in einem vierten, bislang ungeprüften Gerät: Jeder Neustart des
LilyGo (Absturz, USB-Power-Glitch, versehentlicher Reset, OTA-Update) setzt `enowCounter` auf
`0` zurück. Ein Angreifer, der zuvor per ESP-NOW-Sniffer ein gültiges, signiertes WROOM→LilyGo-
Frame mitgeschnitten hat (ESP-NOW überträgt hier unverschlüsselt, `esp_now_add_peer(...,
.encrypt = false)` — nur die `ukfe_rf`-MAC schützt den Inhalt), kann dieses Frame nach jedem
Neustart des LilyGo erneut senden — `25 <= 0` ist falsch, das Frame wird als "neu" akzeptiert,
obwohl es bereits einmal verwendet wurde.

**Warum das hier besonders schwer wiegt:** Anders als beim Hub (der einen Befehl nur selbst
ausführt und weiterfunkt) führt der LilyGo seine Befehle *direkt am angeschlossenen
Zielrechner* aus — `UkfeRfCmdHidDucky`/`UkfeRfCmdHidStream`/`UkfeRfCmdPayloadRun` tippen über
`USBHIDKeyboard` sofort eine BadUSB-Payload, `UkfeRfCmdWifiDeauth`/`UkfeRfCmdBeaconSpam`/
`UkfeRfCmdBleSpam` starten sofort einen Funkangriff. Ein wiedergespielter, bereits einmal
benutzter Befehl löst also nicht nur eine überflüssige Status-Aktion aus, sondern tippt eine
vollständige HID-Payload am (möglicherweise inzwischen anderen) angeschlossenen Rechner erneut
oder startet einen WiFi-/BLE-Angriff, ohne dass der Operator ein zweites Mal etwas gesendet
hat. Der gesamte Zweck des Rolling-Counters ist für jeden Angreifer wirkungslos, der einen
Neustart des LilyGo abwarten oder provozieren kann — und ein per USB an wechselnden Rechnern
betriebener Dongle wird gerade dadurch (Ab-/Anstecken) ohnehin öfter neu gestartet als ein
fest verbautes Gerät.

## Nutzen
- Schließt dieselbe Sicherheitslücke im eigenen Anti-Replay-Mechanismus wie bereits bei
  `usb-army-penetrator` und `heltec-v4-hub` behoben, diesmal für das Gerät mit der
  unmittelbarsten Auswirkung eines Replays (direkte HID-Tastatureingabe am Zielrechner) —
  passt in den in CONSTRAINTS Punkt 5 verlangten defensiven Rahmen (eigenes Pentest-Werkzeug
  soll selbst keine unnötig ausnutzbare Schwachstelle enthalten).
- Wiederverwendet exakt dieselbe, bereits zweimal verifizierte Reserve-mit-
  Sicherheitsspanne-Logik (`ukfe_rf_counter_initial_reserve()`/`ukfe_rf_counter_needs_bump()`)
  — keine neue Krypto, kein neues Protokollfeld, dieselbe bewährte Idee ein viertes Mal
  angewendet.
- Nutzt die ESP32-Arduino-Standardbibliothek `Preferences` (NVS) — bei mir auf dem S3 bereits
  über die Arduino-ESP32-Toolchain verfügbar, kein zusätzlicher `lib_deps`-Eintrag in
  `platformio.ini` nötig.
- Das Wire-Format, `ukfe_rf_mac()`/`ukfe_rf_build_frame()`/`ukfe_rf_parse_frame()` und die
  `counter <= last_counter`-Regel selbst bleiben unverändert — der sendende WROOM-Hub
  benötigt keine Anpassung.
- `send_espnow_ack()`/`respCounter` (LilyGo→WROOM-Richtung) bleiben bewusst unangetastet:
  dort führt ein RAM-Reset nur zu einer vorübergehenden Selbstaussperrung der eigenen ACKs
  (Verfügbarkeits-Bug wie beim ursprünglichen `lora-ukfe`-TX-Fund), nicht zu einer
  Sicherheitslücke, und ist kein Gegenstand dieser Sitzung.

## Betroffene Dateien
- `src/main.cpp` — neues `#include <Preferences.h>`, neues `Preferences`-Objekt
  `s_rfCtrPrefs` + `s_rfCtrPersistedFloor` neben `enowCounter`, `setup()` lädt den
  persistierten Stand aus NVS statt `enowCounter` bei `0` zu belassen, `loop()` prüft nach
  jedem akzeptierten ESP-NOW-Frame, ob die Reserve nachgezogen werden muss.

## Patch
Unified-Diff gegen die unveränderte `G4MEOVER18/lilygo-ukfe-rx`-Basis (`main`, Commit
`aa8a285`; `git apply --check` vorab gegen eine frische, unabhängige Zweitkopie verifiziert,
real angewendet und byte-für-byte gegen die beabsichtigte Endfassung abgeglichen — identisch)
in
[`rf-counter-replay-window-reset-on-reboot-fix/main_cpp.patch`](rf-counter-replay-window-reset-on-reboot-fix/main_cpp.patch).

Einziger inhaltlicher Eingriff: Ursprung und Fortschreibung von `enowCounter` in `main.cpp`.
`ukfe_rf.c`/`.h` (Frame-Format, MAC, CRC, die `counter <= last_counter`-Prüfung selbst)
bleiben unverändert.

## Testhinweis (ohne Hardware)
[`repro-rf-counter-replay-window-reset/repro.c`](repro-rf-counter-replay-window-reset/repro.c)
nutzt die unveränderten Original-Dateien `ukfe_rf.c`/`ukfe_rf.h` (1:1 aus `lilygo-ukfe-rx`
übernommen, laut eigenem Dateikommentar "plattformunabhaengig, ohne HW-Abhaengigkeit") — keine
ESP32/NimBLE/Arduino-Abhängigkeit nötig:

```
$ gcc -std=c11 -O0 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -o repro repro.c ukfe_rf.c
$ ./repro
Szenario 1 (Sitzung vor Neustart): 0/50 abgelehnt (sollte 0 sein)
BUG:  captured HidDucky-Frame (Counter 25, script_id=1) nach Neustart erneut gesendet -> ANGENOMMEN (Replay erfolgreich -> tippt erneut am Zielrechner!)
Szenario 2 (Sitzung vor Neustart, mit Fix): 0/50 abgelehnt, 10 NVS-Schreibzugriffe (statt 50)
FIX:  dasselbe captured Frame (Counter 25) nach Neustart erneut gesendet -> abgelehnt (Replay verhindert)
FIX:  neuer, legitimer Befehl (Counter 61) nach Neustart -> angenommen
Alle Szenarien bestaetigt (exit code 0)
```

Identisch unter `-O1` verifiziert, in beiden Fällen keine ASan-/UBSan-Meldungen und keine
Compiler-Warnungen. Das Repro baut mit den echten `ukfe_rf_build_frame()`/
`ukfe_rf_parse_frame()`-Funktionen 50 gültig signierte `UkfeRfCmdHidDucky`-Frames (simuliert
eine normale Sitzung), schneidet eines davon (Counter 25) als "vom Angreifer per ESP-NOW-
Sniffer mitgeschnitten" mit, simuliert dann einen LilyGo-Neustart (`enowCounter` zurück auf
den jeweiligen Startwert) und spielt das mitgeschnittene Frame erneut ein — einmal mit dem
Ist-Zustand (`0`, Replay gelingt, die HID-Payload würde erneut getippt) und einmal mit der
vorgeschlagenen Fix-Logik (persistierter Floor, Replay wird abgelehnt, neue legitime Befehle
funktionieren weiterhin, NVS-Schreibzugriffe bleiben selten).

Der eigentliche Patch (`main.cpp`) wurde nicht gegen echte Hardware kompiliert (benötigt
PlatformIO + ESP32-S3-Toolchain inkl. `TFT_eSPI`/`USBHIDKeyboard`/NimBLE), ist aber als rein
additive Änderung an bereits bestehenden Codepfaden gehalten (ein neues Include, ein neues
`Preferences`-Objekt, zwei neue freie Funktionen, ein `begin()`/`getUInt()`/`putUInt()`-Aufruf
in `setup()`, ein bedingter `putUInt()`-Aufruf in `loop()`) und folgt exakt demselben, bereits
in `usb-army-penetrator` und `heltec-v4-hub` angewendeten Muster. Lokaler Funktionstest:
PlatformIO-Build, auf den LilyGo flashen, Befehl vom WROOM-Hub senden (z. B. `HidDucky`), den
LilyGo per Reset-Taste oder USB-Trennen neu starten, mit einem zweiten ESP32 im Promiscuous-
Modus das zuvor mitgeschnittene Frame erneut per `esp_wifi_80211_tx()`/ESP-NOW aussenden — vor
dem Fix tippt der LilyGo die Payload erneut, nach dem Fix bleibt die Reaktion aus. Zusätzlich:
Erst-Inbetriebnahme (NVS-Namespace `g4meoverLily` einmal löschen, `loaded == 0` prüfen) sowie
ein harter Reset mitten in einer laufenden Sitzung (der zuletzt gespeicherte
`s_rfCtrPersistedFloor` muss danach weiterhin über jedem tatsächlich akzeptierten Counter
liegen).

## Risiko
- Niedrig: rein additiv in `src/main.cpp` (ein neues Include, ein neues Preferences-Objekt,
  zwei neue freie Funktionen, ein `begin()`/`getUInt()`/`putUInt()`-Aufruf in `setup()`, ein
  bedingter `putUInt()`-Aufruf in `loop()`). Wire-Format, MAC/CRC-Berechnung und die
  Anti-Replay-Regel selbst (`ukfe_rf.c`) sind unverändert.
- Neue NVS-Nutzung über einen neuen, dedizierten Namespace (`g4meoverLily`/`rfCtr`) — kein
  Konflikt mit vorhandenen NVS-Zugriffen, da `lilygo-ukfe-rx` bislang gar keine `Preferences`
  verwendet. `UKFE_RF_COUNTER_MARGIN = 10` hält die Schreibhäufigkeit gering (ein
  Schreibzugriff etwa alle 5 akzeptierten Funkbefehle).
- Bewusst nicht behoben: ein *echter* Desync (z. B. Secret-Rotation oder Austausch des
  WROOM- oder LilyGo-Geräts ohne die gespeicherte Datei) bleibt weiterhin ohne
  Re-Pair-Mechanismus — dieser Patch schließt nur das Zeitfenster, das *jeder* LilyGo-Neustart
  bislang unabhängig vom Betriebsszenario öffnet.
- Verändert nicht `act()`/die Angriffs-/HID-Ausführung selbst — ausschließlich die Frage, ob
  ein Funkbefehl als "neu genug" akzeptiert wird, bleibt unangetastet.
- `send_espnow_ack()`/`respCounter` (Rückweg LilyGo→WROOM) bleiben wie oben begründet
  bewusst unangetastet.

## Quelle
- `G4MEOVER18/lilygo-ukfe-rx` (main, Commit `aa8a285`): `src/main.cpp` (`enowCounter`,
  `onEspNowRecv()`, `loop()`, `setup()`), `src/ukfe_rf.c`/`.h` (`ukfe_rf_parse_frame`,
  Anti-Replay-Prüfung, Header-Kommentar "auf beiden Seiten identisch implementieren").
- `G4MEOVER18/g4meover-ecosystem`:
  `app-improvements/usb-army-penetrator/rf-counter-replay-window-reset-on-reboot-fix.md`
  (ursprünglicher RX-seitiger Fund derselben Protokoll-Anbindungslücke, Ursprung der hier
  wiederverwendeten Reserve-Logik), `app-improvements/heltec-v4-hub/rf-counter-replay-window-reset-on-reboot-fix.md`
  (identischer Fund im dritten geprüften Gerät).
