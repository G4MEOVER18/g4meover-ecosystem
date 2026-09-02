# Evil BLE — externe App aus RogueMaster-Bündel (BLE-Device-Cloning, letzter Kandidat aus b8fe671)

## Was
Aus dem Commit `b8fe671` ("Add 6 new security tools: uart_sniff, subghz_jammer, ble_scanner,
evil_ble, rogue_ap_detector, rayhunter_client") im RogueMaster-Bündel habe ich `uart_sniff`,
`subghz_jammer`, `rogue_ap_detector`, `rayhunter_client` und `ble_scanner` bereits abgearbeitet.
Als letzten offenen Kandidaten aus derselben Commit-Reihe habe ich **Evil BLE** geprüft
(`applications/external/evil_ble/`, `fap_author` „PentestSuite / barkandbite", Quelle laut
`application.fam`/`fap_weburl` `barkandbite/flipper_suite` — dieselbe Quelle wie die fünf bereits
geprüften Apps).

Die App scannt wie BLE Scanner über ein per UART (115200 Baud, Erweiterungsport) angebundenes
ESP32-Board mit Marauder-Firmware nach BLE-Advertisements (`scanbt`-Kommando), lässt mich ein
Zielgerät aus der gefundenen Liste auswählen und broadcastet danach über die native
`furi_hal_bt_extra_beacon_*`-API einen Klon mit der MAC-Adresse und einem synthetisierten
Advertisement-Payload des Zielgeräts (README: „BLE security assessments — test if systems
properly validate device identity beyond name/MAC"). Codebasis ist in vier Dateien getrennt
(`evil_ble.c` GUI/Menü/Clone-Steuerung über `furi_hal_bt_extra_beacon_*`, `evil_ble_scanner.c`
Marauder-Zeilen-Parser/Gerätetabelle, `evil_ble_uart.c` UART-Abstraktion — strukturell identisch
zu BLE Scanner) plus zugehörige Header.

Beim Quelltext-Review (UART-Layer identisch robust zum bereits geprüften BLE Scanner: alle
Kopien über `strncpy`/`snprintf` mit expliziter Zielgrößenangabe, Geräteliste fest auf
`EVIL_BLE_MAX_DEVICES` (32) begrenzt mit Prüfung vor jedem Einfügen) einen **Logikfehler in der
Clone-Payload-Erzeugung** gefunden, der die eigentliche Zweckbestimmung der App für einen ganzen
Geräte-Fall unterläuft.

`evil_ble_scanner.c`, `parse_scanbt_line()`, unterscheidet zwei Fälle für die von Marauder
gemeldete Zeile (`<RSSI> Device: <name-oder-MAC>`, nie beides zusammen):
- Ist der Text nach „Device: " eine MAC-Adresse (kein Name bekannt), setzt die Funktion
  `dev->name` auf die reine Anzeige-Platzhalterform `"(AA:BB:CC:DD:EE:FF)"` und **explizit**
  `dev->adv_data_len = 0` — als Signal „kein echter Name, keine synthetisierte
  Payload verfügbar".
- Ist der Text ein echter Gerätename, wird `dev->name` auf den echten Namen gesetzt und
  `build_adv_from_name()` liefert eine echte `adv_data_len > 0`.

Für den Anzeige-/Dedup-Pfad wertet die App diesen Unterschied korrekt aus — sowohl in
`parse_scanbt_line()` selbst als auch beim Auffrischen eines bereits bekannten Geräts in
`evil_ble_scanner_rx_cb()` steht davor jeweils dieselbe Prüfung `dev->name[0] != '\0' &&
dev->name[0] != '('`, bevor `build_adv_from_name()` aufgerufen wird.

Genau diese Prüfung fehlt aber in `evil_ble.c`, `evil_ble_start_clone()`, im
Fallback-Zweig für `dev.adv_data_len == 0`:

```c
// evil_ble.c, ALT — kein Placeholder-Check vor dem Broadcast-Payload-Bau:
if(dev.adv_data_len > 0) {
    memcpy(adv, dev.adv_data, dev.adv_data_len);
    adv_len = dev.adv_data_len;
} else {
    /* Fallback: bare Complete Local Name from raw name string. */
    uint8_t name_len = (uint8_t)strlen(dev.name);
    if(name_len > EXTRA_BEACON_MAX_DATA_SIZE - 2) {
        name_len = EXTRA_BEACON_MAX_DATA_SIZE - 2;
    }
    adv[0] = name_len + 1;
    adv[1] = 0x09; /* AD type: Complete Local Name */
    memcpy(&adv[2], dev.name, name_len);
    adv_len = (uint8_t)(2 + name_len);
}
```

Für ein Zielgerät, das Marauder ausschließlich per MAC gemeldet hat (kein echter Name je
erfasst), trifft `dev.adv_data_len == 0` zu, und `dev.name` enthält zu diesem Zeitpunkt exakt den
Anzeige-Platzhalter `"(AA:BB:CC:DD:EE:FF)"`. Der Fallback-Zweig behandelt diesen String
ungeprüft als echten, sendefertigen Gerätenamen und baut daraus eine vollständige
„Complete Local Name"-Advertisement-Struktur (AD-Typ `0x09`). Der geklonte Flipper broadcastet
dadurch buchstäblich den Namen `(AA:BB:CC:DD:EE:FF)` — einen Text, den kein reales BLE-Gerät je
sendet, und der mit der tatsächlichen Identität des Zielgeräts nichts zu tun hat. Genau für den
Gerätetyp, der laut Definition keinen Namen broadcastet, erzeugt der Klon dadurch ein Namensfeld,
das im Original gar nicht existierte — beim exakt in der App-Beschreibung genannten
Testfall „prüft ein System die Geräte-Identität über Name/MAC hinaus" ist das Namensfeld selbst
das erste, offensichtliche Unterscheidungsmerkmal zwischen Original und Klon, noch bevor die MAC
überhaupt verglichen wird.

## Nutzen
- Deckt die letzte noch fehlende Kategorie aus derselben Quelle ab: **aktives BLE-Device-Cloning**
  als direktes Gegenstück zur bereits geprüften passiven BLE-Rekognoszierung (BLE Scanner) —
  beide nutzen dieselbe ESP32/Marauder-Hardwarekette und ergänzen sich (erst mit BLE Scanner
  aufspüren, dann mit Evil BLE gezielt für einen autorisierten Identitätsprüfungstest klonen).
- Rein per Software-Broadcast über die native `furi_hal_bt_extra_beacon_*`-API, keine
  Zusatzhardware für den Klon-Teil selbst (nur für den vorgelagerten Scan wie bei BLE Scanner).
- README benennt den Verwendungszweck explizit als autorisierten Identitätsvalidierungstest
  („test if systems properly validate device identity beyond name/MAC") — passt in den
  defensiven/autorisierten Rahmen aus CONSTRAINTS Punkt 5, analog zu bereits vorhandenen
  Spoofing-/Replay-Werkzeugen im eigenen Toolkit (MagSpoof, RollJam/RollForge, natives BLE Spam).
- Der gefundene Fehler ist mit einer kleinen, lokal auf `evil_ble.c` begrenzten Änderung behebbar
  (siehe Port-Skizze) — dieselbe Prüfung, die die App an zwei anderen Stellen bereits selbst
  verwendet, muss nur an der dritten, fehlenden Stelle ergänzt werden. Ohne den Fix würde ich mir
  beim ersten Test gegen ein reales namenloses Gerät einen funktional falschen Klon einbauen, der
  den eigentlichen Testzweck der App unterläuft.

## Betroffene Dateien (lokal)
Neu, aus der Quelle übernehmbar (eigenständiger externer App-Ordner):
- `applications_user/evil_ble/application.fam`
- `applications_user/evil_ble/evil_ble.c` / `.h` — **mit dem unten beschriebenen Fix**
- `applications_user/evil_ble/evil_ble_scanner.c` / `.h`
- `applications_user/evil_ble/evil_ble_uart.c` / `.h`
- `applications_user/evil_ble/README.md`

Meine CI-Vorlage `ci-templates/build-fap.yml` passt unverändert (reine `.fap`, keine
Firmware-Kernänderung).

## Risiko
- Niedrig: additiver externer App-Ordner ohne Firmware-Kernänderung. Fehlschlag zeigt sich beim
  Build (`ufbt`/APPCHK) als Compile-Fehler.
- **API-87-Kompatibilität**: nutzt exakt dieselbe UART-Erweiterungsport-API wie mein bereits
  verifizierter BLE-Scanner-Port (`expansion_enable/disable`, `furi_hal_serial_control_acquire/
  release`, `furi_hal_serial_init/deinit`, `_async_rx_start/_stop`, `_async_rx`, `_tx`,
  `_tx_wait_complete`, `furi_stream_buffer_*`) — dort bereits gegen meinen `mntm-012`-Header-Stand
  verifiziert, hier identisch übernommen. Zusätzlich genutzt: `furi_hal_bt_extra_beacon_set_config
  /_set_data/_start/_stop` sowie `GapExtraBeaconConfig`/`GapAdvChannelMapAll`/
  `GapAdvPowerLevel_0dBm`/`GapAddressTypeRandom` — Standard-BLE-Beacon-API, bereits produktiv in
  meiner eigenen Firmware genutzt vom nativen BLE-Spam-Feature, keine zusätzliche API-Fläche.
- **Hardwarekette für den Scan-Teil**: wie bei BLE Scanner ein ESP32-Board mit BLE-fähigem Chip
  (WROOM/S3) und laufender Marauder-Firmware nötig — vor lokalem Test klären, welches ESP32-Board
  ich einsetze (siehe bereits dokumentierte Einschränkung in `firmware-mining/ble-scanner-app-port.md`).
- **Der gefundene Fehler selbst ist kein Speicher-/Sicherheitsproblem** (keine Buffer-Overflows,
  alle Kopien größenbegrenzt), sondern ein Logikfehler mit funktionaler Konsequenz: die Klon-
  Payload für namenlose Zielgeräte ist ohne den Fix inhaltlich falsch. Der reguläre Fall (Zielgerät
  mit echtem erfasstem Namen) ist bereits ohne Fix korrekt — durch den Host-Repro bestätigt
  (Testhinweis unten), ALT und NEU liefern dort byte-identische Advertisement-Daten.
- **Bekannte, nicht mitgefixte Einschränkung** (Architekturgrenze der Quelle, kein Fehler dieser
  App): Marauders `scanbt`-Ausgabe liefert laut Quellcode-Kommentar in `evil_ble_scanner.c` nie
  Name und MAC gemeinsam für dieselbe Zeile. Meldet ein Gerät mal mit, mal ohne Namen, erzeugt die
  App dafür zwei getrennte Listeneinträge (echte MAC vs. aus dem Namen abgeleitete
  `DE:AD:xx:xx:xx:xx`-Platzhalter-MAC) statt eines einzigen — ein reiner Marauder-Protokoll-
  Formatierungs-Datenverlust, nicht in der Flipper-App selbst behebbar, vorgemerkt für den Fall,
  dass ich künftig direkt gegen eine andere ESP32-Firmware mit vollständigeren Scan-Zeilen
  spreche.

## Port-Skizze
1. App-Ordner `evil_ble` (alle unter „Betroffene Dateien" gelisteten Dateien) aus
   `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`,
   `applications/external/evil_ble/`) nach `applications_user/evil_ble/` in meiner Firmware
   kopieren.
2. In der kopierten `evil_ble.c`, Funktion `evil_ble_start_clone()`, den Fallback-Zweig um
   dieselbe Placeholder-Prüfung ergänzen, die `evil_ble_scanner.c` an den zwei anderen Stellen
   bereits verwendet:

   ```c
   /* Fallback: bare Complete Local Name from raw name string -- nur wenn ein
    * echter Name erfasst wurde. dev.name kann sonst den Anzeige-Platzhalter
    * "(AA:BB:CC:DD:EE:FF)" aus parse_scanbt_line() enthalten (MAC-only-
    * Geraet); dieselbe Pruefung wie dort und in evil_ble_scanner_rx_cb(). */
   if(dev.name[0] == '\0' || dev.name[0] == '(') {
       adv_len = 0; /* kein Name bekannt -> MAC-only-Klon, kein AD-0x09-Feld */
   } else {
       uint8_t name_len = (uint8_t)strlen(dev.name);
       if(name_len > EXTRA_BEACON_MAX_DATA_SIZE - 2) {
           name_len = EXTRA_BEACON_MAX_DATA_SIZE - 2;
       }
       adv[0] = name_len + 1;
       adv[1] = 0x09; /* AD type: Complete Local Name */
       memcpy(&adv[2], dev.name, name_len);
       adv_len = (uint8_t)(2 + name_len);
   }
   ```

   Für `adv_len == 0` sendet `furi_hal_bt_extra_beacon_set_data(adv, 0)` ein leeres
   Advertisement-Payload — der Klon übernimmt dann ausschließlich die MAC-Adresse
   (`config.address`, bereits vorher aus `dev.mac_bytes` gesetzt), passend zu einem Zielgerät,
   das selbst kein Namensfeld broadcastet.
3. `ufbt launch_app APPSRC=applications_user/evil_ble` bzw.
   `fbt launch_app APPSRC=applications_user/evil_ble` — deckt etwaige Header-Abweichungen als
   Compile-Fehler auf.
4. Host-seitiges Mini-Repro (`repro.c`, unten) mit `gcc -O2 -Wall -Wextra
   -fsanitize=address,undefined` bauen und laufen lassen, um das ALT/NEU-Verhalten des Fehlers
   zu bestätigen — bereits von mir lokal ausgeführt, siehe Testhinweis.
5. Hardwarekette wie bei BLE Scanner aufbauen: ESP32-Board (WROOM/S3, Marauder-Firmware mit
   BLE-Support) per UART an den Flipper-Erweiterungsport.
6. Funktionstest (**nur gegen eigene Geräte, autorisiertes Testumfeld**):
   - Ein eigenes Gerät mit bekanntem Namen (z. B. eigenes Handy) scannen, klonen, mit einem
     zweiten eigenen BLE-Scanner (z. B. eine Scanner-App auf einem zweiten eigenen Handy)
     gegenprüfen, dass MAC und Name des Klons dem Original entsprechen.
   - **Regressionstest für den Fix**: ein eigenes Gerät ohne broadcasteten Namen (oder ein
     Gerät, das Marauder nur per MAC gemeldet hat) klonen — vor dem Fix erscheint im
     Scan-Ergebnis des zweiten Geräts ein Name `(AA:BB:CC:DD:EE:FF)` o. ä., nach dem Fix
     erscheint gar kein Namensfeld, nur die geklonte MAC.
   - Klon nach Test sofort per BACK/Stop-Funktion beenden (`furi_hal_bt_extra_beacon_stop()`),
     Broadcast-Dauer auf den eigenen Testzeitraum begrenzen.
7. Bei erfolgreichem Test: zusammen mit BLE Scanner als Scan-/Clone-Paar für autorisierte
   BLE-Identitätsvalidierungstests führen.

## Testhinweis (lokal nachvollzogen)
Host-kompilierbares Mini-Repro (`repro.c`) des exakten Byte-Konstruktionscodes aus dem
Fallback-Zweig von `evil_ble_start_clone()`: ALT (unverändert aus der Quelle übernommen) vs. NEU
(mit der oben beschriebenen Placeholder-Prüfung). Vier Fälle geprüft, gebaut/ausgeführt mit
`gcc -O2 -Wall -Wextra -fsanitize=address,undefined`:

```
== Case 1: MAC-only-Geraet (nie ein Name erfasst) ==
  dev.name = "(AA:BB:CC:DD:EE:FF)"  dev.adv_data_len = 0
  ALT: len=21 bytes -> AD type 0x09 (Complete Local Name) = "(AA:BB:CC:DD:EE:FF)"
  NEU: len=0 bytes  -> kein AD-Feld (MAC-only-Klon)

== Case 3 (Regression): echtes erfasstes Geraet ==
  dev.name = "MyPhone"  dev.adv_data_len = 0
  ALT: len=9 bytes -> AD type 0x09 (Complete Local Name) = "MyPhone"
  NEU: len=9 bytes -> AD type 0x09 (Complete Local Name) = "MyPhone"
  (ALT und NEU byte-identisch)

== Case 4 (Regression): leerer dev.name ==
  ALT: len=2 bytes -> AD type 0x09 (Complete Local Name) = ""
  NEU: len=0 bytes -> kein AD-Feld

ALLE PRUEFUNGEN BESTANDEN.
```

Bestätigt: ALT baut für ein Zielgerät ohne echten Namen ein Advertisement mit dem
Anzeige-Platzhalter als fabriziertem Namensfeld; NEU broadcastet für denselben Fall korrekt gar
kein Namensfeld. Für ein Zielgerät mit echtem erfasstem Namen (der Regelfall, den die App primär
adressiert) verhalten sich ALT und NEU byte-identisch — der Fix ändert ausschließlich den
zuvor falschen Namenlos-Fall.

## Quelle
- RogueMaster-Firmware, `flipperzero-firmware-wPlugins`, Branch `420`,
  `applications/external/evil_ble/` (Teil von Commit `b8fe671`, „Add 6 new security tools").
- Ursprungsquelle laut `application.fam`/`fap_weburl`: `barkandbite/flipper_suite` (dieselbe
  Quelle wie SubGHz Jammer Detector, Rogue AP Detector, Rayhunter Client, UART Sniff und BLE
  Scanner).
- Damit ist die gesamte Commit-Reihe `b8fe671` ("Add 6 new security tools") jetzt vollständig
  abgearbeitet.
