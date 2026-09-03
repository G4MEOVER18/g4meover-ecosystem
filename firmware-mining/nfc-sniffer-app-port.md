# NFC Sniffer — externe App aus RogueMaster-Bündel (passiver NFC-Kommando-Logger)

## Was
Ich habe den seit der `mfdesfire_auth`-Sitzung offenen Rückstellpunkt "restlicher NFC-
Werkzeug-Cluster aus dem RogueMaster-Bündel" (`ideas/pentest-app-katalog.md`) aufgegriffen und
als nächsten Kandidaten **NFC Sniffer** geprüft (`applications/external/nfc_sniffer/`,
`fap_author` "Kyle Diller", Quelle laut `application.fam`/`fap_weburl`
`kdiller713/public-FlipperApps`).

Die App konfiguriert die eigene NFC-Hardware im **Listener-Modus** (`nfc_config(nfc,
NfcModeListener, protocol)`) für eines von drei wählbaren Protokollen (ISO14443-3A,
ISO15693-3, FeliCa — ISO14443-3B fällt weg, da die eigene NFC-HAL dafür keinen Listener
unterstützt) und protokolliert jedes empfangene Reader-Kommando als Hex-String, sowohl live
scrollbar im Display (`TextBox`) als auch persistent in eine Zeitstempel-Datei unter
`/ext/nfc_sniffer_logs/log_JJJJMMTT_HHMMSS.txt`. Sie fängt laut eigenem README **nur** die vom
Reader gesendeten Kommandos ab, nicht die Tag-Antworten — reines Kommando-Logging, kein
vollständiger Bus-Mitschnitt. Architektur: `nfc_listener/nfc_sniffer.c` kapselt den
NFC-Callback (100 Zeilen), `ui/protocol_select_scene.c` das Protokoll-Auswahlmenü,
`ui/log_scene.c` den Log-Bildschirm samt Datei-/Puffer-Verwaltung — insgesamt 493 Zeilen über
7 Dateien, keine Zusatzhardware nötig.

Beim Quelltext-Review einen echten Speicherzugriffsfehler gefunden: `nfc_sniffer_callback()`
in `ui/log_scene.c` baut für jedes empfangene Kommando (außer dem leeren EOF-Fall) einen
Hex-String in einem lokal auf dem Stack allozierten Array, dessen Größe exakt für "1
Zeilenumbruch + 2×N Hex-Zeichen + 1 NUL-Terminator" bemessen ist — schreibt aber **vor** der
eigentlichen Hex-Umwandlung zusätzlich einen NUL-Terminator eine Position hinter das Ende
dieses Arrays:

```c
// ui/log_scene.c, ALT — nfc_sniffer_callback():
char output[2 * buffer_length + 2];   // gueltige Indizes: 0 .. 2*buffer_length+1
output[0] = '\n';
output[2 * buffer_length + 2] = '\0'; // <- 1 Byte HINTER dem Array, Stack-Overwrite

for(size_t i = 0; i < buffer_length; i++) {
    snprintf(&output[i * 2 + 1], 3, "%02X", bit_buffer_get_byte(data, i));
}
```

Die Schleife selbst ist korrekt bemessen: der letzte `snprintf`-Aufruf (`i =
buffer_length - 1`) schreibt seinen eigenen NUL-Terminator exakt an Index `2 * buffer_length +
1`, dem letzten gültigen Indexplatz des Arrays. Die vorab ausgeführte Zeile
`output[2 * buffer_length + 2] = '\0';` ist also nicht nur überflüssig (der Terminator wird
gleich danach ohnehin korrekt gesetzt), sondern schreibt beim allerersten Aufruf sofort ein
Byte hinter das Ende der Stack-Allokation — bei **jedem einzelnen, nicht-leeren Kommando**, das
während des Sniffens erfasst wird, also im normalen Kernbetrieb der App. Mit Host-Repro unter
AddressSanitizer bestätigt (siehe Testhinweis).

## Nutzen
- Deckt eine mir bislang fehlende Kategorie ab: **passives Mitschneiden der tatsächlich von
  einem Reader gesendeten NFC-Kommandos**, unabhängig vom Kartentyp/Protokoll-Stack. Ergänzt
  meine bestehenden NFC-Werkzeuge (MFKey/Nested für Schlüssel-Recovery, NFC Magic zum
  Beschreiben) um die Analyseseite: was sendet ein Reader tatsächlich (Kommandosequenz,
  Timing-Reihenfolge, ungewöhnliche/nicht-standardkonforme Kommandos) bei einem autorisierten
  Zutrittskontroll- oder Reader-Audit.
- Sehr kleine, fokussierte App ohne Zusatzhardware — direkt mit eigener NFC-Antenne nutzbar,
  kein ESP32/nRF24-Modul, keine Lötarbeit.
- Reine Empfangsfunktion, kein eigener Sendebetrieb außerhalb des passiven Listener-Verhaltens
  der NFC-HAL selbst — passt in den defensiven Rahmen aus CONSTRAINTS Punkt 5.
- Der gefundene Fehler ist mit einer einzeiligen, lokal begrenzten Änderung behebbar (siehe
  Port-Skizze) — kein Architektur- oder API-Eingriff nötig.

## Betroffene Dateien (lokal)
Neu, aus der Quelle übernehmbar (eigenständiger externer App-Ordner):
- `applications_user/nfc_sniffer/application.fam`
- `applications_user/nfc_sniffer/application.c`
- `applications_user/nfc_sniffer/nfc_listener/nfc_sniffer.c` / `.h`
- `applications_user/nfc_sniffer/ui/ui.c` / `.h` / `ui_i.h`
- `applications_user/nfc_sniffer/ui/protocol_select_scene.c`
- `applications_user/nfc_sniffer/ui/log_scene.c` — **mit dem unten beschriebenen Fix**
- `applications_user/nfc_sniffer/ui/missing_api.c` / `.h`
- `applications_user/nfc_sniffer/assets/nfc_sniffer.png`
- `applications_user/nfc_sniffer/README.md`

Meine CI-Vorlage `ci-templates/build-fap.yml` passt unverändert (reine `.fap`, keine
Firmware-Kernänderung).

## Risiko
- Niedrig: additiver externer App-Ordner ohne Firmware-Kernänderung. Fehlschlag zeigt sich beim
  Build (`ufbt`/APPCHK) als Compile-Fehler.
- **API-87-Kompatibilität**: gegen den eigenen `mntm-012`-Header-Stand verifiziert (Momentum-
  Firmware-Tag sparse ausgecheckt: `lib/nfc/nfc.h`, `lib/toolbox/bit_buffer.h`). Alle
  verwendeten Signaturen 1:1 vorhanden: `nfc_config()` mit `NfcModeListener`,
  `nfc_set_guard_time_us()`, `nfc_set_fdt_poll_fc()`, `nfc_set_fdt_poll_poll_us()`,
  `nfc_start()`/`nfc_stop()`, `NfcEventTypeRxEnd`, `NfcCommandContinue`/`NfcCommandStop`,
  `NfcTechIso14443b`, `NfcTechNum`, `bit_buffer_get_size_bytes()`, `bit_buffer_get_byte()`.
  GUI-/Storage-Seite nutzt ausschließlich Standardheader (`gui/modules/submenu.h`,
  `gui/modules/text_box.h`, `gui/view_dispatcher.h`, `gui/scene_manager.h`,
  `notification/notification.h`, `storage/storage.h`, `datetime/datetime.h`), kein
  `requires`-Eintrag im `application.fam` über das Standard-NFC/-GUI hinaus.
- **Der gefundene Fehler selbst**: ein einzelnes Byte Stack-Overwrite direkt hinter einer
  variablen-länge Allokation (VLA), ausgelöst bei jedem nicht-leeren empfangenen Kommando.
  Unter AddressSanitizer ein harter Abbruch; auf dem echten Gerät ohne ASan wahrscheinlich eine
  stille Beschädigung benachbarter Stack-Daten (undefiniertes Verhalten, abhängig vom
  Stack-Layout des jeweiligen Builds) statt eines sofort sichtbaren Absturzes — also gerade
  deshalb vor dem produktiven Einsatz zu beheben, auch ohne bislang beobachtetes sichtbares
  Fehlverhalten.
- Fängt laut eigenem README nur Reader→Tag-Kommandos, keine Tag-Antworten — kein vollständiger
  bidirektionaler Mitschnitt, für die Analyse des Reader-Verhaltens aber ausreichend.
- ISO14443-3B bleibt unterstützungslos (Menüpunkt entfällt bewusst) — Einschränkung der eigenen
  NFC-HAL, nicht der App.

## Port-Skizze
1. App-Ordner `nfc_sniffer` (alle unter „Betroffene Dateien" gelisteten Dateien) aus
   `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`,
   `applications/external/nfc_sniffer/`) nach `applications_user/nfc_sniffer/` kopieren.
2. In der kopierten `ui/log_scene.c` in `nfc_sniffer_callback()` die überflüssige,
   fehlerhafte Zeile entfernen:

   ```c
   char output[2 * buffer_length + 2];
   output[0] = '\n';
   /* entfernt: output[2 * buffer_length + 2] = '\0';  -- 1 Byte hinter dem Array,
      ausserdem ueberfluessig, da der letzte snprintf()-Aufruf der folgenden Schleife
      den Terminator bereits korrekt an Index 2*buffer_length+1 setzt. */

   for(size_t i = 0; i < buffer_length; i++) {
       snprintf(&output[i * 2 + 1], 3, "%02X", bit_buffer_get_byte(data, i));
   }
   ```

   Einzeilige Änderung, kein Header-/API-Signaturbruch, keine andere Datei betroffen.
3. `ufbt launch_app APPSRC=applications_user/nfc_sniffer` bzw.
   `fbt launch_app APPSRC=applications_user/nfc_sniffer` — deckt etwaige Header-Abweichungen
   als Compile-Fehler auf.
4. Host-seitiges Mini-Repro (`repro.c`, unten) mit `gcc -O0 -g -Wall -Wextra
   -fsanitize=address,undefined` bauen und laufen lassen, um ALT (Absturz) gegen NEU (sauberer
   Lauf) zu bestätigen — bereits lokal ausgeführt, siehe Testhinweis.
5. Funktionstest je Protokoll (ISO14443-3A / ISO15693-3 / FeliCa): eigenen Reader bzw. einen
   zweiten Flipper im Lese-/Poll-Modus gegen eine eigene emulierte Karte laufen lassen,
   erfasste Kommando-Hexstrings im Log-Display und in der erzeugten
   `/ext/nfc_sniffer_logs/log_*.txt`-Datei gegen die erwarteten Kommandos (z. B. REQA/WUPA/
   Anticollision bei ISO14443-3A) prüfen.
6. Langlauf-Test: Sniffing über viele Kommandos laufen lassen, um die
   Puffer-Kürzungslogik in `append_text()` (`LOG_TEXT_SIZE = 4097`) nach dem Fix weiterhin
   korrekt zu bestätigen (von der Änderung nicht direkt berührt, aber im selben Codepfad).
7. Bei erfolgreichem Test: als Analyse-Ergänzung zu MFKey/Nested/NFC Magic im NFC-Werkzeugsatz
   führen.

## Testhinweis (lokal nachvollzogen)
Host-kompilierbares Mini-Repro (`repro.c`) in
`firmware-mining/repro-nfc-sniffer-log-nul-oob-write/` — bildet die exakte Array-Größen-/
Index-Rechnung aus `nfc_sniffer_callback()` nach (Flipper-spezifische Typen durch
`uint8_t`/`char` ersetzt, sonst identischer Ausdruck). Gebaut/ausgeführt mit
`gcc -O0 -g -Wall -Wextra -fsanitize=address,undefined` für 4 Testbytes (`DE AD BE EF`):

```
ALT (mit der fehlerhaften Zeile):
==...==ERROR: AddressSanitizer: dynamic-stack-buffer-overflow ...
WRITE of size 1 at ... thread T0
    #0 ... in append_hex_old repro.c:34
    #1 ... in main repro.c:51
SUMMARY: AddressSanitizer: dynamic-stack-buffer-overflow repro.c:34 in append_hex_old
Exit-Code: 1 (Abbruch)

NEU (Zeile entfernt):
output=
DEADBEEF
Exit-Code: 0 (sauberer Lauf, korrekte Ausgabe)
```

Bestätigt: die ALT-Fassung überschreibt zuverlässig 1 Byte hinter der Stack-Allokation bei
jedem nicht-leeren Kommando; die NEU-Fassung (Zeile entfernt) liefert exakt dieselbe korrekte
Ausgabe ohne den Zugriffsfehler.

## Quelle
- RogueMaster-Firmware, `flipperzero-firmware-wPlugins`, Branch `420`,
  `applications/external/nfc_sniffer/`.
- Ursprungsquelle laut `application.fam`/`fap_weburl`: `kdiller713/public-FlipperApps`
  (Kyle Diller), https://github.com/kdiller713/public-FlipperApps/tree/main/nfc_sniffer.
- Eigener Fund: Off-by-one-Stack-Schreibzugriff in `ui/log_scene.c`,
  `nfc_sniffer_callback()`, mit Host-Repro unter AddressSanitizer verifiziert (siehe
  `firmware-mining/repro-nfc-sniffer-log-nul-oob-write/repro.c`).
- Restlicher NFC-Werkzeug-Cluster aus demselben Rückstellpunkt weiterhin offen: `chameleon`,
  `nfc_relay`, `mifare_fuzzer`, `nfc_rfid_detector`, `nfctools`, `uhf_rfid`, `nfc_fuzzer`,
  `nfc_canary`, `nfc_comparator`, `simultaneous_rfid_reader`.
