# Chameleon Ultra Client (RogueMaster-Bündel) — Heap-Buffer-Overflow beim Antwort-Parsing

## Was
Ich habe den seit mehreren Sitzungen als "Fremd-/Zweitgerät-Fall" zurückgestellten
`chameleon`-Kandidaten aus dem restlichen NFC-Werkzeug-Cluster (`ideas/pentest-app-katalog.md`)
jetzt doch geprüft, statt ihn nur als Fernsteuerungsclient einzuordnen: die App
(`applications/external/chameleon/`, `fap_author` "Chameleon Flipper Team / muylder",
Quelle `muylder/Chameleon_Flipper`) ist zwar tatsächlich ein reiner Steuerclient für ein
externes Chameleon-Ultra-Gerät (kein eigener NFC-Hardwarezugriff), hat aber einen eigenen
Protokoll-Parser für die Antworten dieses Geräts — und genau dort einen Fund gemacht, der
unabhängig vom Fremdhardware-Status relevant ist: ein durch die Gegenseite kontrollierter
Heap-Buffer-Overflow beim Empfang.

`lib/chameleon_protocol/chameleon_protocol.c::chameleon_protocol_parse_frame()` extrahiert das
LEN-Feld einer eingehenden Antwort (Byte 6–7, big-endian `uint16_t`, Wertebereich 0–65535) und
verwendet es direkt als `memcpy()`-Länge in den vom Aufrufer übergebenen `data`-Puffer:

```c
// chameleon_protocol.c, ALT — chameleon_protocol_parse_frame():
*data_len = ((uint16_t)frame_data[6] << 8) | frame_data[7];
// ... LRC2-Pruefung ...
if(*data_len > 0) {
    memcpy(data, &frame_data[9], *data_len);   // <- *data_len nie gegen die Zielpuffergroesse geprueft
    // ... LRC3-Pruefung ...
}
```

Der einzige Aufrufer im gesamten App-Quellbestand, `chameleon_app_rx_callback()`
(`chameleon_app.c`), übergibt dafür `app->data_buffer`, das in `chameleon_app_i.h` als
`uint8_t data_buffer[512]` deklariert ist (== `CHAMELEON_MAX_DATA_LEN`) — Teil des
`malloc()`-allozierten `ChameleonApp`-Structs. `data`/`length` dieses Callbacks stammen 1:1 aus
dem empfangenen USB-CDC-Strom (`lib/uart_handler/uart_handler.c::uart_handler_rx_thread()` →
`furi_hal_cdc_receive()`), also direkt von der Gegenseite der gekoppelten Verbindung — dem
eigenen Chameleon-Ultra-Gerät im Normalfall, aber ebenso von jedem anderen USB-Host/Gerät, das
sich stattdessen an dieselbe CDC-Schnittstelle anschließt, oder einem MITM auf dieser
Verbindung.

Die einzige Vorab-Prüfung vor dem Parsen ist `chameleon_protocol_validate_frame()`, die nur
interne Konsistenz verlangt (`frame_len == CHAMELEON_FRAME_OVERHEAD + LEN`) — eine Antwort mit
`LEN = 2000` und tatsächlich 2010 gesendeten Bytes ist für diese Prüfung vollkommen gültig.
`chameleon_protocol_parse_frame()` selbst prüft `*data_len` an keiner Stelle gegen
`CHAMELEON_MAX_DATA_LEN`, bevor es als Kopierlänge dient — anders als die Sendefunktion
`chameleon_protocol_build_frame()` im selben File, die genau diese Prüfung bereits für den
umgekehrten Weg hat:

```c
// chameleon_protocol.c — chameleon_protocol_build_frame() (Sendepfad, bereits korrekt):
if(data_len > CHAMELEON_MAX_DATA_LEN) {
    FURI_LOG_E(TAG, "Data length %u exceeds maximum %u", data_len, CHAMELEON_MAX_DATA_LEN);
    return false;
}
```

Jede Antwort mit `LEN` zwischen 513 und 65535 (passende LRC2/LRC3 vorausgesetzt — beide sind
reine Prüfsummen, keine Geheimnisse, von der Gegenseite trivial korrekt berechenbar) schreibt
damit bis zu 65023 Byte über das Ende von `app->data_buffer[512]` hinaus in die direkt
anschließenden Felder des `ChameleonApp`-Structs (`protocol`-Zeiger, `response_mutex`,
`response_buffer`, Funktionszeiger der Scene-/View-Dispatcher-Strukturen dahinter auf dem Heap)
und potenziell darüber hinaus — ein klassischer, aus der Ferne über die gekoppelte Verbindung
auslösbarer Heap-Overflow, kein reiner Absturz. Selbst wenn `*data_len` innerhalb der Grenze
läge, würde der nachfolgende `memcpy(app->response_buffer, app->data_buffer, data_len)` in
`chameleon_app_rx_callback()` denselben ungeprüften Wert erneut verwenden — der eine Fix in
`parse_frame()` deckt beide Kopierstellen ab, da `chameleon_app_rx_callback()` den zweiten
`memcpy()` nur nach einem erfolgreichen `parse_frame()`-Aufruf ausführt.

Den tatsächlichen BLE-Pfad (`lib/ble_handler/ble_handler.c`) habe ich dabei gegengeprüft: er ist
nach eigenem Kommentar ("BLE initialization on Flipper Zero is complex... This is a simplified
version", "TODO: Implement actual BLE scanning") noch ein Stub ohne echte GATT-Verbindung — der
praktisch nutzbare, bereits fertig implementierte Angriffs-/Auslösekanal ist ausschließlich die
USB-CDC-Schnittstelle (Interface 0) aus `uart_handler.c`.

## Nutzen
- Schließt den einzigen noch offenen Codereview-Rückstand im `chameleon`/`nfc_relay`-Rückstellpunkt,
  der bisher pauschal als "Fremdhardware-Fall, kein Port-Kandidat" eingestuft war — zeigt, dass
  auch ein reiner Fernsteuerungsclient einen eigenen, prüfbaren Protokoll-Parser mitbringt, der
  denselben Maßstab verdient wie die bereits geprüften NFC-Cluster-Apps.
- Der Fund betrifft die Robustheit des eigenen Flipper-seitigen Clients gegenüber der
  Gegenseite der gekoppelten Verbindung — relevant sowohl beim normalen Betrieb mit dem eigenen
  Chameleon-Ultra-Gerät (defekte/manipulierte Firmware darauf, fehlerhafte USB-Kabel-/
  Spannungsbedingungen mit Bitfehlern) als auch als Lehrbeispiel für autorisierte Tests: ein
  zweites Gerät, das sich an derselben CDC-Schnittstelle als "Chameleon Ultra" ausgibt, kann den
  Flipper zum Absturz bringen oder Heap-Zustand verderben, ohne dass der Nutzer das bemerkt.
- Die Fix-Form ist identisch zum bereits im selben File vorhandenen Muster
  (`build_frame()`-Prüfung), minimal-invasiv und ändert an der gültigen Protokoll-Semantik
  nichts.

## Betroffene Dateien (lokal)
Falls diese App künftig nach `applications_user/chameleon_ultra/` portiert wird (bisher nicht
Teil meines eigenen Firmware-Baums, da als Fremdhardware-Fall zurückgestellt):
- `applications_user/chameleon_ultra/lib/chameleon_protocol/chameleon_protocol.c` — **mit dem
  unten beschriebenen Fix**
- alle übrigen Dateien unter `applications_user/chameleon_ultra/` unverändert aus
  `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`,
  `applications/external/chameleon/`)

Der Fund und Fix stehen unabhängig von einem tatsächlichen Port für sich — relevant auch für
jeden, der `muylder/Chameleon_Flipper` direkt aus der Originalquelle nutzt.

## Risiko
- Niedrig für den Fix selbst: additive Grenzprüfung an einer einzigen Stelle, identisches Muster
  zur bereits vorhandenen Prüfung in `chameleon_protocol_build_frame()` im selben File. Ändert
  das Verhalten für jede Antwort innerhalb der gültigen Grenze (0–512 Byte) nicht.
- **API-87/`mntm-012`-Kompatibilität**: Die App nutzt ausschließlich Standard-Furi-HAL
  (`furi_hal_cdc_receive()`/`furi_hal_cdc_send()`, gegen den `mntm-012`-Tag selbst sparse
  ausgecheckt und in `targets/f7/furi_hal/furi_hal_usb_cdc.c` mit identischer Signatur
  bestätigt), `FuriStreamBuffer`, `FuriThread`, `FuriMutex`, Standard-GUI-Module
  (`Submenu`/`VariableItemList`/`TextInput`/`Popup`/`Widget`/`Loading`) — kein `requires`-Eintrag
  über das Standard-GUI/USB hinaus, kein Firmware-Kerneingriff nötig.
- **Der gefundene Fehler selbst** ist schwerwiegender als die bisher in diesem Cluster
  dokumentierten `furi_check()`-Abbrüche: ein echter Heap-Buffer-Overflow mit von der Gegenseite
  frei wählbarer Länge (bis zu 65023 Byte über das Ende des Zielpuffers), nicht nur ein
  kontrollierter Hard-Fault. Auslösbar durch jede Antwort mit `LEN > 512`, unabhängig vom
  konkreten Kommando — kein Rand- oder Spezialfall einer einzelnen Funktion.
- Restrisiko nach dem Fix: `chameleon_protocol_parse_frame()` lehnt eine solche Antwort jetzt
  komplett ab (`return false`), die App protokolliert nur einen Parse-Fehler und verwirft die
  Antwort — kein Status-/Fortschrittsverlust über das reine Ignorieren der einen fehlerhaften
  Antwort hinaus.

## Port-Skizze
1. Bei einem künftigen Port: App-Ordner `chameleon` unverändert aus
   `RogueMaster/flipperzero-firmware-wPlugins` (`applications/external/chameleon/`) nach
   `applications_user/chameleon_ultra/` kopieren.
2. Patch `chameleon-protocol-parse-frame-oob-write-fix/chameleon_protocol_c.patch` (unten)
   gegen `lib/chameleon_protocol/chameleon_protocol.c` anwenden — fügt direkt nach dem
   Extrahieren von `*data_len` eine Prüfung gegen `CHAMELEON_MAX_DATA_LEN` ein, die bei
   Überschreitung `false` zurückgibt, bevor LRC2-Prüfung oder `memcpy()` erreicht werden.
3. `ufbt launch_app APPSRC=applications_user/chameleon_ultra` bzw.
   `fbt launch_app APPSRC=applications_user/chameleon_ultra` — deckt etwaige
   Header-Abweichungen als Compile-Fehler auf.
4. Host-seitiges Mini-Repro (`repro.c`, unten) mit `gcc -O0`/`-O1 -Wall -Wextra
   -fsanitize=address,undefined` bauen und laufen lassen, um ALT (Heap-Overflow bei
   `LEN > 512`) gegen NEU (Ablehnung ohne Schreibzugriff) zu bestätigen — bereits lokal
   ausgeführt, siehe Testhinweis.
5. Funktionstest am Gerät: mit einem eigenen, unveränderten Chameleon-Ultra-Gerät über USB
   verbinden, normale Operationen (Geräteinfo, Slot-Liste, Tag lesen/schreiben) durchführen —
   alle regulären Antworten bleiben unter 512 Byte, Verhalten muss identisch zum unveränderten
   Stand bleiben.
6. Den eigentlichen Fund (Antwort mit `LEN > 512`) am Gerät nachzustellen braucht eine bewusst
   fehlerhafte/manipulierte Gegenseite (zweites Gerät oder Skript, das sich als Chameleon Ultra
   an derselben USB-CDC-Schnittstelle meldet) — für den Nachweis reicht das Host-Repro, am
   Gerät nur der Normalfall zwingend zu testen.

## Testhinweis (lokal nachvollzogen)
`firmware-mining/repro-chameleon-protocol-parse-frame-oob-write/repro.c` transkribiert die
SOF/LRC1/CMD/STATUS/LEN/LRC2-Extraktion und das DATA-`memcpy()` aus
`chameleon_protocol_parse_frame()` 1:1 (ALT) und ergänzt in einer zweiten Fassung (NEU) nur die
vorgeschlagene Grenzprüfung. Acht Szenarien:
- Regression bei 20 Byte und beim Grenzfall exakt 512 Byte (ALT/NEU liefern identische
  Nutzdaten und `data_len`).
- Fund bei `LEN = 513`, `LEN = 2000` und `LEN = 65535` (maximal möglicher `uint16_t`-Wert) — je
  in einem eigenen `fork()`-Kindprozess, da ASan den Heap-Overflow fatal abbricht.
- Fix-Nachweis bei `LEN = 513` und `LEN = 65535`: NEU lehnt beide ab (`return false`) und lässt
  den mit einem Wachposten-Muster (`0xAA`) vorbelegten Zielpuffer nachweislich unverändert.
- Äquivalenztest über den gesamten gültigen Bereich `data_len` 0–512 Byte (ALT/NEU liefern
  identische Nutzdaten und Rückgabewert für jede Länge).

Unter `gcc -std=c11 -O0`/`-O1 -Wall -Wextra -fsanitize=address,undefined` verifiziert: ALT
bricht alle drei Fund-Szenarien reproduzierbar mit
`AddressSanitizer: heap-buffer-overflow ... WRITE of size <LEN> ... 0 bytes after 512-byte
region` ab, NEU übersteht dieselben Antworten ohne jeden Schreibzugriff auf den Zielpuffer,
Regressions- und Äquivalenztest liefern byte-identische Ausgabe zwischen ALT und NEU — 14/14
Einzelprüfungen bestanden, ALLE CHECKS BESTANDEN, keine weiteren ASan-/UBSan-Meldungen, keine
Compiler-Warnungen.

Patch (`git apply --check` vorab gegen eine frische, unabhängige Kopie von
`RogueMaster/flipperzero-firmware-wPlugins`, Branch `420`, verifiziert, real angewendet und
byte-für-byte mit der beabsichtigten Endfassung abgeglichen) in
`firmware-mining/chameleon-protocol-parse-frame-oob-write-fix/chameleon_protocol_c.patch`.
