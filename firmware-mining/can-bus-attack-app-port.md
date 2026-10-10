# CAN Bus Attack — externe App von jorgepnaranjo25 (universitäres CAN-Pentest-Projekt)

## Was

Aus der seit der CAN-Tools-Sitzung offenen Restliste des RogueMaster-CAN-Bus-Clusters
(`can_bus_attack`, `can_commander`, `can_transceiver`, `canbus` — `can_tools` und
`can_transceiver` bereits geprüft) **CAN Bus Attack** (`jorgepnaranjo25/CAN-Bus-Attack`, App-ID
`canbus_attack_app`) aufgegriffen: mit App-Kern (`app_user.c`/`.h`), neun `scenes/`-Dateien,
eigenen `libraries/` (`Spi_lib`, `mcp_can_2515`, `pid_library`, `uds_library`) und
`draw_functions/` zusammen 4349 Zeilen — die kleinste der drei noch offenen Hardware-Apps
(`can_commander`: 15165 Zeilen, `canbus`: 9042 Zeilen). Laut eigenem `application.fam` explizit
"als Teil eines universitären Cybersecurity-Forschungsprojekts entwickelt, nur für Bildungs- und
ethische Zwecke gedacht".

Direkte Low-Level-Ansteuerung eines MCP2515-CAN-Controllers über den Erweiterungsport (eigener,
selbst gebauter SPI-Bus-Handle auf `furi_hal_spi_bus_r`, nicht
`furi_hal_spi_bus_handle_external` wie bei `can_tools`/`can_transceiver`): **Sniffing** (laufender
Empfang, bis zu 100 unterschiedliche CAN-IDs mit je bis zu 20 historischen Rahmen im RAM),
**Attack DoS** (Flooding einer Ziel-ID), **Replay Attack** (zuvor gesnifften Rahmen erneut
senden) und **Modify Attack** (gesnifften Rahmen vor dem erneuten Senden bearbeiten). Zusätzlich
eine eigene UDS-Bibliothek (`uds_library.c`, ISO-14229-ähnliche Diagnosedienste: Read/Write DID,
ECU Reset, Security Access, Session Control) und eine PID-Bibliothek (`pid_library.c`, OBD-II
Mode-01-Abfragen) — beide aktuell ohne eigene Szene angebunden (kein `#include`/Aufruf in
`scenes/`), laut `app_user.h`s `SniffingTestOption`/`AttackDoSOption`/`ReplayAttackOption`/
`ModifyAttackOption`/`AboutUsOption`-Menü nur die vier genannten Hauptfunktionen aktiv erreichbar.

## Nutzen für mich

Deckt zusammen mit dem bereits geprüften `can_tools` (Signalinterpretation) und `can_transceiver`
(reines Rohframe-Senden/Empfangen) die aktive Angriffsseite ab: DoS-Flooding, Replay und
gezielte Rahmen-Modifikation sind die drei klassischen CAN-Bus-Pentest-Primitive für ein eigenes
Testfahrzeug/-netz. Von den drei noch offenen Hardware-Apps die kleinste und am schnellsten lokal
baubare.

## API-87-Kompatibilität (mntm-012)

Gegen eine frische, unabhängige Zweitkopie von `Next-Flip/Momentum-Firmware` (Tag `mntm-012`)
geprüft:

- `furi_hal_spi_bus_r` (`furi_hal_spi_config.h`, deklariert als `extern FuriHalSpiBus
  furi_hal_spi_bus_r`, Kommentar "External on `furi_hal_spi_bus_r`") — byteidentisch vorhanden.
  Anders als `can_tools`/`can_transceiver`, die den höherwertigen
  `furi_hal_spi_bus_handle_external`-Wrapper nutzen, baut sich diese App ihren eigenen
  `FuriHalSpiBusHandle` direkt auf `furi_hal_spi_bus_r` zusammen (`libraries/Spi_lib.c:spi_alloc()`),
  inklusive eigenem Event-Callback (`furi_hal_gpio_init_ex(..., GpioAltFn5SPI1)` für
  MISO/MOSI/SCK, manuelles CS-Toggle) — exakt das in mehreren anderen Flipper-Apps verbreitete
  Eigenbau-Muster für einen zusätzlichen SPI1-Erweiterungsport-Chip neben dem internen CC1101/
  ST25R3916. `GpioAltFn5SPI1`, `gpio_ext_pa4/_pa6/_pa7/_pb3`, `LL_SPI_Init/_Enable/_Disable/
  _SetRxFIFOThreshold`, `FuriHalSpiBusHandleEvent{Init,Deinit,Activate,Deactivate}` — alle
  byteidentisch vorhanden.
- `furi_hal_light_set()`/`_blink_start()`/`_blink_stop()` (LED-Rückmeldung während Attack-
  Szenen), `furi_hal_gpio_read(&gpio_button_back)` (Abbruchprüfung im Sniffing-Worker-Thread) —
  Standard-HAL, byteidentisch vorhanden.
- `gui/modules/{submenu,widget,text_box,text_input,byte_input,variable_item_list,file_browser,
  file_browser_worker}.h`, `dialogs/dialogs.h`, `storage/storage.h`, `scene_manager`/
  `view_dispatcher` — Standard-GUI-/Storage-APIs, byteidentisch vorhanden, dasselbe Muster wie
  bei `can_tools`/`can_transceiver` bereits verifiziert.

Kein Port-Hindernis: reines externes FAP, keine Firmware-Core-Änderung nötig.

## Fund: `read_frame()` begrenzt das empfangene DLC-Feld nicht auf die deklarierte Puffergröße

`read_frame()` (`libraries/mcp_can_2515.c`) liest einen vom MCP2515 empfangenen CAN-Rahmen aus
dessen RX-Registern:

```c
furi_hal_spi_bus_rx(spi, &data_ctrl, 1, TIMEOUT);

frame->data_lenght = data_ctrl & MCP_DLC_MASK;   // MCP_DLC_MASK == 0x0F -> 0..15
frame->req = (data_ctrl & MCP_RTR_MASK) ? 1 : 0;

for(uint8_t i = 0; i < frame->data_lenght; i++) {
    furi_hal_spi_bus_rx(spi, &frame->buffer[i], 1, TIMEOUT);   // frame->buffer ist uint8_t[8]
}
```

`MCP_DLC_MASK` ist `0x0F` — das DLC-Feld ist im CAN-Protokoll 4 Bit breit, `data_ctrl &
MCP_DLC_MASK` liefert also jeden Wert von 0 bis 15. `CANFRAME.buffer` (`mcp_can_2515.h`) ist aber
nur `uint8_t[MAX_LEN]` mit `MAX_LEN == 8`. Für ein DLC-Feld von 9–15 schreibt die Schleife
entsprechend 1–7 Byte über das deklarierte Array hinaus. Das DLC-Feld wird weder vom CAN-Protokoll
selbst noch vom MCP2515-Chip hardwareseitig auf 8 begrenzt — der Controller legt im
`RXBnDLC`-Register exakt das ab, was der Sender auf dem Bus übertragen hat; ein manipulierter
oder fehlerhafter Sender (klassisches CAN-Pentest-Szenario, für das diese App selbst gebaut ist)
kann ein DLC-Nibble von 9–15 übertragen.

`read_frame()` wird an allen drei Aufrufstellen mit einem lokalen `CANFRAME` auf dem Stack
aufgerufen, nie mit einer heap-/global allozierten Instanz:

- `scenes/SnifferOption.c:176/194` (`worker_sniffing()`): `CANFRAME frame = app->can_frame;` —
  der aktive Sniffing-Worker-Thread, der auch für Replay/Modify-Attack den Ausgangsrahmen liefert.
- `libraries/pid_library.c:86/109` (OBD-II-Antwortempfang, aktuell ohne eigene Szene angebunden).
- `libraries/uds_library.c:55` (UDS-Antwortempfang, ebenfalls aktuell ohne eigene Szene
  angebunden).

Die Schleife schreibt damit bei einem manipulierten DLC-Feld über das Ende eines lokalen
Stack-Objekts hinaus — ein Stack-Buffer-Overflow, dessen tatsächliche Reichweite vom
Speicherlayout des jeweiligen Aufrufers abhängt (bei `worker_sniffing()` z. B. direkt neben
`FuriString* label` und mehreren weiteren lokalen Stack-Variablen der Funktion). Per Host-Repro
(siehe unten) nachgewiesen: das Überschreiten des deklarierten Arrays beginnt bereits bei DLC=9
(1 Byte, auf dem Host-ABI noch innerhalb der eigenen Tail-Padding des 16-Byte-`CANFRAME`-Objekts,
siehe Repro-Ausgabe), ab DLC=10 reicht der Schreibzugriff nachweislich über das gesamte
Stack-Objekt hinaus (von AddressSanitizer als `stack-buffer-overflow` erkannt).

Die etablierte, verbreitete MCP2515-Host-Bibliothek, von der dieser Treiber erkennbar abstammt
(`coryjfowler`/Seeed `MCP_CAN`, Funktion `readMsg()`), begrenzt an exakt dieser Stelle explizit
`if (dlc > 8) dlc = 8;`, bevor die Datenbytes gelesen werden — dieser Port hat diese Begrenzung
beim Übertragen verloren. Dieselbe Datei (`libraries/mcp_can_2515.c`) liegt byteidentisch auch im
noch offenen `canbus`-Kandidaten desselben Clusters vor (per `diff` bestätigt) — betrifft also
potenziell auch jenen künftigen Port.

Praktische Auswirkung für ein Pentest-Werkzeug, dessen Zweck gerade das Verarbeiten von Rahmen
unbekannter/nicht vertrauenswürdiger Herkunft ist: ein einzelner manipulierter oder fehlerhafter
Rahmen auf dem Bus während des Sniffing kann den Sniffing-Worker-Thread zum Absturz bringen oder,
je nach Stack-Layout des jeweiligen Aufrufers, benachbarte lokale Variablen überschreiben.

## Host-Repro

Vier Szenarien, exakte Struktur-/Feldreihenfolge aus `mcp_can_2515.h`/`mcp_can_2515.c`
transkribiert, SPI-Bus per Mock auf einen vorbereiteten Byte-Strom nachgebildet: (1)
Regressionsfall — vollständiger, gültiger Standard-Frame mit DLC=8, ALT und NEU liefern
identisches, korrektes Ergebnis; (2) Fundfall 1 — DLC-Nibble=9, ALT liest `data_lenght=9` (bereits
außerhalb des Array-Vertrags) und schreibt 1 Byte über `buffer[7]` hinaus, das auf diesem Host-ABI
noch in die eigene Tail-Padding des 16-Byte-`CANFRAME`-Stack-Objekts fällt (`sizeof(CANFRAME) ==
16`, `offsetof(buffer) == 7`) und daher hier nicht von ASan erkannt wird — explizit als
Grenzfall dokumentiert statt übersehen; NEU begrenzt korrekt auf `data_lenght=8`; (3) Fundfall 2 —
DLC-Nibble=10, kleinster auf diesem Host von ASan tatsächlich erkannter Fall (`stack-buffer-
overflow`); (4) Fundfall 3 — DLC-Nibble=15, maximaler Fall; (5) vollständiger Sweep über alle 16
möglichen DLC-Nibble-Werte: für 0–8 verhalten sich ALT und NEU direkt verglichen identisch und
sicher (9/9), für 9–15 liefert NEU in allen 7 Fällen das erwartete, begrenzte Ergebnis, während
ALT in 6 von 7 Fällen (DLC 10–15) einen von ASan erkannten Stack-Overflow auslöst (DLC=9 fällt wie
in Fundfall 1 erläutert in die Tail-Padding). Unter `gcc -std=c11 -O0`/`-O1 -Wall -Wextra
-fsanitize=address,undefined -fno-sanitize-recover=undefined` verifiziert: alle Szenarien wie
beschrieben bestätigt, identischer Output beider Optimierungsstufen, keine Compiler-Warnungen —
in `firmware-mining/repro-can-bus-attack-dlc-overflow/repro.c`.

## Betroffene Datei

- `libraries/mcp_can_2515.c` — Funktion `read_frame()`.

## Port-Skizze

1. App unverändert aus `jorgepnaranjo25/CAN-Bus-Attack` (bzw. `applications/external/
   can_bus_attack/` im RogueMaster-Bündel, Branch `420`) in meine eigene `applications/external/
   can_bus_attack/`-Struktur übernehmen (reines FAP, keine Firmware-Core-Änderung).
2. Patch `firmware-mining/can-bus-attack-app-port/mcp_can_2515_c.patch` danach anwenden — fügt
   eine additive Begrenzungszeile direkt nach der DLC-Maskierung ein:
   `if(frame->data_lenght > MAX_LEN) frame->data_lenght = MAX_LEN;`, exakt nach dem Muster der
   Referenzbibliothek. Patch per `git apply --check` gegen eine frische, unabhängige Zweitkopie
   verifiziert, real angewendet, byte-für-byte mit der Arbeitskopie abgeglichen — identisch.

## Testhinweis (lokal, mit MCP2515-Hardware)

1. MCP2515-Transceiver-Modul am Erweiterungsport verkabeln (`CS=PA4`, `SCK=PB3`, `MOSI=PA7`,
   `MISO=PA6`, siehe `libraries/Spi_lib.h`), `ufbt`/`fbt`-Build gegen die eigene `mntm-012`-Basis
   mit `can_bus_attack` in `applications/external/`.
2. Regressionstest: "Sniffing" gegen eine zweite CAN-Quelle mit ausschließlich gültigen
   Rahmen (DLC 0–8) öffnen — Verhalten vor/nach Patch muss identisch sein.
3. Fundnachvollzug: einen CAN-Rahmen mit manipuliertem DLC-Feld (9–15) auf den Bus senden (z. B.
   über einen zweiten Mikrocontroller mit eigenem CAN-Controller/-Treiber, der das DLC-Nibble
   direkt im Steuerbyte setzt, oder einen Bus-Fault-Injector im Rahmen eines autorisierten Tests)
   — vor dem Patch: Absturz- bzw. Fehlverhalten des Sniffing-Worker-Threads je nach konkretem
   DLC-Wert und Stack-Layout möglich; nach dem Patch: Rahmen wird mit auf 8 begrenzter Datenlänge
   verarbeitet, kein Absturz.
4. Falls lokal kein Fault-Injector verfügbar ist: der Host-Repro deckt den Fund bereits
   vollständig und reproduzierbar ab (siehe oben) — der Hardware-Test ist in erster Linie eine
   Regressionsprüfung für den gültigen Bereich (DLC 0–8).

## Risiko

Niedrig. Rein additive Begrenzung, kein Verhaltensunterschied für den bereits korrekten,
protokollkonformen Bereich (DLC 0–8, per Host-Repro-Sweep belegt). Für DLC 9–15 ändert der Patch
das Verhalten bewusst von "undefiniert/Speicherfehler" zu "Rahmen wird mit auf 8 begrenzter
Datenlänge verarbeitet" — dieselbe bewusste Kappung wie in der Referenzbibliothek, kein
Informationsverlust für reale, protokollkonforme CAN-Rahmen (die nie DLC > 8 verwenden).

## Sicherheitsrahmen

Fund und Fix betreffen ausschließlich die Robustheit des eigenen Pentest-Werkzeugs gegenüber
Rahmen vom per MCP2515-Modul direkt verkabelten Bus (eigenes Fahrzeug/Testaufbau oder
autorisierter Pentest, CONSTRAINTS Punkt 5) — kein Bezug zu Angriffstechniken selbst (die App
bietet DoS/Replay/Modify bereits unverändert als deklarierter Zweck). Keine Kartendaten oder
echte Zugangsdaten beteiligt.

## Offen

- Lokale Bauprobe mit MCP2515-Hardware weiterhin offen (wie bei allen drei verbleibenden
  hardwareabhängigen CAN-Apps dieses Clusters).
- Aus demselben Cluster bleiben `can_commander`, `canbus` noch nicht einzeln geprüft — `canbus`
  teilt sich laut `diff` die identische, hier gefixte `mcp_can_2515.c` mit `can_bus_attack` und
  dürfte denselben Fund beim künftigen Prüfen reproduzieren.
- `uds_library.c`/`pid_library.c` sind aktuell ohne eigene Szene angebunden (nicht über das
  Hauptmenü erreichbar) — eigenständiger, kleinerer Rückstellpunkt für eine künftige Sitzung,
  falls sie in einer späteren App-Version angebunden werden.

## Quelle

- `jorgepnaranjo25/CAN-Bus-Attack` (direktes Repo) bzw. `applications/external/can_bus_attack/`
  im RogueMaster-Bündel (`RogueMaster/flipperzero-firmware-wPlugins`, Branch `420`).
