# Argus (at0m-b0mb/Argus-FlipperZero) — WiFi-Deauth-/Evil-Twin-Watchdog

## Quelle

`github.com/at0m-b0mb/Argus-FlipperZero`, MIT-Lizenz, Autor `at0m-b0mb`. Geprüfter Stand:
Commit `bfedeac9` (`main`, HEAD, zugleich Tag `v1.0`). Zweiteilige Anwendung: Flipper-Zero-`.fap`
(2303 Zeilen über 20 C-Dateien) plus eigenständige ESP32-Begleitfirmware
(`esp32/argus_esp32/argus_esp32.ino`, 256 Zeilen, Arduino/ESP-IDF). Kein RogueMaster-Bündel-Fund.

Letzter offener Kandidat aus demselben vierteiligen Gegenüberwachungs-Werkzeugsatz von
`at0m-b0mb` — Nyx (IR), Specter (NFC) und GhostTag (BLE) sind bereits geprüft (jeweils ohne
eigenen Fund); **Argus** (WiFi) schließt den Satz jetzt ab, diesmal mit einem eigenen Fund.

## Was

Argus macht aus dem Flipper-Zero-WiFi-Devboard (ESP32-S2) bzw. einem beliebigen angeschlossenen
ESP32 einen WiFi-Wächter gegen zwei Bedrohungen:

- **Deauth-/Disassoc-Fluten** (aktiver Angriff): der ESP32 läuft im Promiscuous-Modus, zählt
  802.11-Deauth-/Disassoc-Management-Frames in einem gleitenden 2-Sekunden-Fenster
  (`ARGUS_STORM_WINDOW_MS`) und meldet einen "Storm", sobald die Rate eine einstellbare
  Schwelle (High/Medium/Low → 3/6/12 Frames) überschreitet.
- **Evil-Twin-APs**: der ESP32 streamt Beacon-/Probe-Response-Metadaten (BSSID, Kanal, RSSI,
  Verschlüsselung, SSID) an den Flipper; dort hält `helpers/argus_db.c` eine AP-Tabelle
  (`ARGUS_MAX_APS = 32`) und markiert jede BSSID, die dieselbe SSID wie die vom Nutzer gesetzte
  "Guarded SSID" trägt, aber nicht die mit dem stärksten Signal (angenommen: das eigene, echte
  Heim-AP) ist, als Klon.
- Architektur-Trennung wie bei den übrigen `at0m-b0mb`-Werkzeugen: der ESP32 ist die reine
  Funk-Schicht (Promiscuous-Capture, Frame-Klassifikation, Verschlüsselungserkennung über die
  RSN-/Vendor-Tagged-Parameter), der Flipper die Analyse-/UI-Schicht ("The Eye" — eine
  animierte Iris mit Blips pro AP, Alarm-Lidschlag bei aktivem Angriff) über ein einfaches
  ASCII-Zeilenprotokoll (`AXD,...`/`AXAP,...`/`AXHELLO,...`) bei 115200 Baud auf dem
  Erweiterungsport-USART (GPIO 13/14 TX/RX) — exakt dasselbe Muster wie bei UART Sniff/
  GPIO Logic Analyzer/RS485 Modbus/Rogue AP Detector/BLE Scanner, nur auf der eigenen
  `FuriHalSerialIdUsart`-Schnittstelle statt dem Marauder-ESP32-Kanal.
- `helpers/argus_db.c` ist durchgängig mutex-geschützt (UART-Worker-Thread schreibt, GUI-Thread
  liest nur über atomare Snapshot-Kopien `argus_db_copy_aps/_copy_twins/_copy_log`), inkl.
  Least-Recently-Seen-Eviction bei voller AP-Tabelle und einem separaten 40-Einträge-Ringpuffer
  (`ARGUS_MAX_LOG`) für den Bedrohungs-Log.

## Nutzen

Schließt die in der Nyx-Sitzung angelegte Kategorie **Counter-Surveillance/TSCM** jetzt beim
WiFi-Kanal — bisherige eigene Gegenüberwachungs-Funde decken RF (SubGHz Jammer Detector), WiFi
bislang nur seitens Rogue-AP-Erkennung über die Marauder-ESP32-Bridge, NFC-Karten (NFC Canary),
NFC-Leser (Specter), IR (Nyx), BLE-Tracker (GhostTag) und UART (UART Sniff) ab. Argus bringt eine
eigenständige, von Marauder/GhostESP unabhängige WiFi-Deauth-/Evil-Twin-Erkennung mit eigener
Begleitfirmware (kein Abhängigkeit von meinem bestehenden Marauder-UI-Stack) — nützlich, um beim
autorisierten Test des eigenen Netzwerks aktive Deauth-Angriffe und untergeschobene Klon-APs
unabhängig vom restlichen WiFi-Tooling zu erkennen. Nutzt dieselbe Devboard-Hardware wie Rogue AP
Detector (ESP32 über GPIO/UART), kein Zusatzlöten nötig.

## Aufwand

**Gering** für den Flipper-`.fap`-Teil (reiner Quelltext-Build gegen die eigene `mntm-012`-SDK,
`fap_category="GPIO"`, `requires=["gui"]`, keine Submodul-Abhängigkeit). Für den ESP32-Teil:
eigene Arduino-/ESP-IDF-Toolchain nötig (wie bei Rogue AP Detector/BLE Scanner/Evil BLE bereits
etabliert) — Flashen der `argus_esp32.ino` auf das WiFi-Devboard oder einen eigenen ESP32. Mit dem
unten beschriebenen Fund ist der ESP32-Teil **nicht** mehr unverändert übernehmbar, siehe Fix.

## API-87-Status

**✅ Quelltextlich verifiziert, kompatibel — ein Fund (ESP32-Begleitfirmware, kein Fund im
Flipper-`.fap`-Teil).**

Flipper-Seite (`.fap`) gegen den eigenen `Next-Flip/Momentum-Firmware`-Klon (Tag `mntm-012`,
API 87.1) geprüft, sparse ausgecheckt gezielt für die hier verwendete USART-Schnittstelle:
alle sechs verwendeten `furi_hal_serial_*`-Symbole exakt wie aufgerufen in
`targets/f7/furi_hal/furi_hal_serial.h`/`furi_hal_serial_control.h` vorhanden
(`furi_hal_serial_control_acquire`/`_release`, `furi_hal_serial_init`/`_deinit`,
`furi_hal_serial_async_rx_start`/`_stop`/`furi_hal_serial_async_rx`, `furi_hal_serial_tx`,
`FuriHalSerialIdUsart`, `FuriHalSerialRxEvent`/`FuriHalSerialRxEventData`), ebenso
`expansion_enable`/`_disable`/`RECORD_EXPANSION` in
`applications/services/expansion/expansion.h` — dasselbe "Expansion-Dienst vor USART-Übernahme
deaktivieren"-Muster, das ich bereits bei UART Sniff/RS485 Modbus/GPIO Logic Analyzer/Rogue AP
Detector verifiziert habe. Übrige API-Fläche (`FuriThread`/`FuriStreamBuffer`/`FuriMutex`,
ViewDispatcher/SceneManager/Submenu/VariableItemList/TextInput/Widget,
NotificationApp/`NotificationSequence`) ist Standard-Furi-Kern-API, unverändert vorhanden.

**Vollständig gelesen (2303 von 2303 Zeilen Flipper-`.fap`-Quelltext, 100 %):** `argus.c`/
`argus_i.h` (Lebenszyklus, Benachrichtigungs-Sequenzen, UART-Callback-Verdrahtung),
`helpers/uart_link.c`/`.h` (Zeilenprotokoll-Parser inkl. `split_csv()`/`parse_mac()`, ISR→Stream-
Buffer→Worker-Thread-Pfad, Start/Stop-Lebenszyklus), `helpers/argus_db.c`/`.h` (AP-Tabelle,
Bedrohungs-Ringpuffer, Sliding-Window-Ratenzähler, Evil-Twin-Reklassifikation — alle
Mutex-Pfade, Eviction- und Ringpuffer-Arithmetik einzeln nachgerechnet, kein Fund), alle acht
Szenen-Dateien (`scenes/argus_scene*.c`, inkl. Settings-Persistenz-Defaults und der
Submenu-/State-Verdrahtung) sowie alle drei Views (`views/ap_list_view.c`, `views/monitor_view.c`
— "The Eye"-Zeichenroutinen inkl. Polarkoordinaten-/Lidkurven-Berechnung, `views/
threat_log_view.c`) — durchgehend saubere `snprintf`/`strncpy`-Diszplin mit vorab berechneten
Puffergrößen, keine Division durch potenziell Null außerhalb bereits durch den jeweils
umschließenden `m->count > VISIBLE`-Guard abgesicherter Scrollbar-Berechnungen, kein eigener Fund.

**ESP32-Begleitfirmware vollständig gelesen (256 von 256 Zeilen, 100 %) — ein Fund:**
`sniffer_cb()` (Promiscuous-Callback) prüft `len < 24` vor dem Lesen der drei MAC-Adressfelder
(Guard bewusst strenger als nötig, deckt denselben Fehlertyp ab, den ich bereits in
`app-improvements/heltec-ukfe-rx/wifi-recon-probe-ssid-tag-oob-read-fix.md` für ein anderes
eigenes ESP32-Projekt gefunden und gefixt habe) sowie `len < 36` vor dem Lesen der
Capabilities-Bytes — beide korrekt. `classify_enc()` (RSN-/Vendor-Tagged-Parameter-Parser für die
Verschlüsselungserkennung) begrenzt jeden Tag-Zugriff korrekt gegen `body_len`, kein Fund dort.

**Fund:** die SSID-Tag-Extraktion direkt danach in `sniffer_cb()` prüft die vom Sender
deklarierte Tag-Länge nur gegen die feste Puffergröße 32, nicht gegen die tatsächlich im
empfangenen Frame noch verbleibenden Bytes:

```c
// argus_esp32.ino, ALT — sniffer_cb(), SSID-Tag-Block:
char ssid[33] = {0};
if (body_len >= 2 && body[0] == 0) {
  int sl = body[1];           // vom Sender deklarierte SSID-Laenge, 0-255
  if (sl > 32) sl = 32;       // nur gegen die Zielpuffergroesse geklemmt
  for (int k = 0; k < sl; k++) {
    char c = (char)body[2 + k];   // <- kann ueber body_len hinauslesen
    ssid[k] = (c == ',' || c == '\n' || c == '\r' || c < 32) ? '_' : c;
  }
  ssid[sl] = '\0';
}
```

`body` zeigt auf `pkt->payload + 36`, `body_len = len - 36`, wobei `len = pkt->rx_ctrl.sig_len`
die tatsächlich vom ESP32-WiFi-Treiber empfangene Framegröße ist — eine vom Sender frei
gewählte Zahl, unabhängig von der im SSID-Information-Element selbst deklarierten Länge `sl`.
Jedes Gerät in Funkreichweite kann ein Beacon- oder Probe-Response-Frame senden, dessen
SSID-Tag eine Länge bis 32 deklariert, dessen tatsächliche Frame-Nutzlast aber kürzer endet
(verkürzter Tag-Body, absichtlich oder durch reale Funkbedingungen) — die Schleife liest dann
über das Ende des vom Treiber tatsächlich zugewiesenen Empfangspuffers hinaus. Auf dem ESP32
führt das je nach Heap-Layout zu gelesenem Nachbarspeicher (landet, sofern druckbar, unsanitisiert
in der angezeigten/geloggten SSID — die bestehende Sanitisierung ersetzt nur Komma/Zeilenende/
Steuerzeichen durch `_`, keine Längenprüfung) oder im ungünstigen Fall zu einem Absturz/Reset des
ESP32-Begleitgeräts; betrifft ausschließlich die SSID-Anzeige, nicht die
Deauth-/Disassoc-Erkennung oder die Verschlüsselungsklassifikation.

## Sicherheitsrahmen

Rein passiv auf der Erkennungsseite (sendet selbst keine Deauth-/Management-Frames, reine
Promiscuous-Beobachtung plus Analyse) — genau der in `CONSTRAINTS.md` verlangte defensive
Rahmen: Schutz des eigenen Netzwerks vor Deauth-Angriffen und Evil-Twin-Klonen, keine
Angriffsfunktion. Der gefundene Fund ist ein App-eigenes Robustheitsproblem (OOB-Read im
ESP32-Begleitgerät), kein Ziel-Sicherheitsrisiko und keine neue Angriffsfunktion.

## Fix-Skizze

Zusätzliche Klemmung auf die tatsächlich nach dem Tag-Header verbleibenden Bytes, bevor die
deklarierte Länge verwendet wird:

```c
// argus_esp32.ino, NEU:
char ssid[33] = {0};
if (body_len >= 2 && body[0] == 0) {
  int sl = body[1];
  if (sl > 32) sl = 32;
  int avail = body_len - 2; // tatsaechlich nach dem Tag-Header empfangene Bytes
  if (sl > avail) sl = avail; // deklarierte Tag-Laenge kann die empfangene Framegroesse ueberschreiten
  for (int k = 0; k < sl; k++) {
    char c = (char)body[2 + k];
    ssid[k] = (c == ',' || c == '\n' || c == '\r' || c < 32) ? '_' : c;
  }
  ssid[sl] = '\0';
}
```

Rein additiv (eine neue lokale Variable, eine zusätzliche Klemmung), ändert das Verhalten im
Normalfall (`sl <= avail`, d. h. jede korrekt konstruierte Beacon-/Probe-Response-Nachricht)
nicht — siehe Host-Repro-Äquivalenztest unten.

## Testhinweis (lokal nachvollzogen)

Host-kompilierbares Mini-Repro (`repro.c`) in `ideas/repro-argus-ssid-tag-oob-read/` — bildet
exakt den SSID-Tag-Extraktionsblock aus `sniffer_cb()` 1:1 nach (ALT und NEU), mit `body` als
exakt auf `body_len` Byte alloziertem Heap-Puffer (wie ein über die tatsächliche Framegröße
hinaus nicht weiter reservierter DMA-Empfangspuffer), damit ein Overread unter
AddressSanitizer sichtbar wird. Jedes Szenario läuft in einem eigenen `fork()`-Kindprozess.
Gebaut/ausgeführt mit `gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined` bei `-O0` und
`-O1`:

- Zwei Regressionsfälle (`sl == avail`, `sl == 0`): ALT und NEU beide ohne Crash, identisches
  Ergebnis.
- Fund, zwei Szenarien (verkürztes Frame mit `avail = 8` bei deklarierter Länge 32; nur der
  Tag-Header selbst empfangen, `avail = 0`, Länge trotzdem deklariert): ALT crasht beide Male
  reproduzierbar mit `AddressSanitizer: heap-buffer-overflow`, exakt an der erwarteten Adresse
  (0 Byte nach dem jeweiligen Allokationsende); NEU übersteht beide Fälle, liefert die korrekt
  abgeschnittene SSID.
- Äquivalenztest über `body_len` 2–40 × `declared_len` 0–32 (alle Fälle mit `sl <= avail`, also
  der tatsächlich auftretende Normalfall): ALT und NEU liefern byte-identische Ausgabe in jedem
  einzelnen Fall.

Ergebnis beider Durchläufe (`-O0`/`-O1`): **ALLE CHECKS BESTANDEN**, keine weiteren
ASan-/UBSan-Meldungen, keine Compiler-Warnungen.

## Port-/Anwendungs-Hinweis

Lokal: `argus_esp32.ino` mit dem obigen Fix in `sniffer_cb()` flashen (Arduino-IDE/
PlatformIO gegen die eigene ESP32-S2-/ESP32-Toolchain wie bereits bei Rogue AP Detector
etabliert), Flipper-`.fap` unverändert aus dem Quellbestand bauen
(`ufbt launch`/`fbt launch_app`). Funktionstest: eigenes Test-Netzwerk als "Guarded SSID"
setzen, Watch-Ansicht öffnen, eigenen zweiten AP mit identischer SSID als Klon aufsetzen
(bestätigt Evil-Twin-Erkennung) und mit einem eigenen zweiten Gerät einen Deauth-Test gegen
das eigene Testnetz fahren (bestätigt Storm-Alarm) — beide Testfälle unverändert vom Fix
betroffen, da dieser ausschließlich den SSID-Tag-Extraktionspfad einer zu kurz empfangenen
Beacon-/Probe-Response-Nachricht berührt. Der Fund selbst lässt sich am Gerät nur durch
gezieltes Senden eines verkürzten/manipulierten Beacon-Frames provozieren (eigenes
Test-Equipment, z. B. ein eigener ESP32 im Raw-802.11-Sendemodus) — für den Nachweis reicht
das Host-Repro, am Gerät genügt der Normalfalltest.

## Quelle für den Fund

Eigener Fund: `argus_esp32.ino::sniffer_cb()` klemmt die aus dem SSID-Information-Element
gelesene, senderkontrollierte Tag-Länge nur gegen die feste Zielpuffergröße (32), nicht gegen
die tatsächlich im empfangenen Frame nach dem Tag-Header verbleibenden Bytes — verifiziert mit
Host-Repro unter AddressSanitizer/UBSan (siehe `ideas/repro-argus-ssid-tag-oob-read/repro.c`).
Damit ist der vierteilige `at0m-b0mb`-Gegenüberwachungs-Werkzeugsatz (Nyx/Specter/GhostTag/Argus)
vollständig geprüft.
