# LoRa Relay (ElectronicCats) — API-87-Status + Fund

Neue Kategorie "LoRa" gesucht — bislang keine eigene LoRa-App im Katalog, obwohl
`g4meover-deck` bereits `lorix_link.py` (Pi-seitige LoRa-Anbindung) führt. **LoRa Relay**
gefunden: SX1262-basierter Sniffer/Sender für ein eigenes Erweiterungsmodul, über das
RogueMaster-Bündel referenziert (App-ID `lora_relay`).

## Quelle
- Repo: `ElectronicCats/flipper-SX1262-LoRa` (direktes GitHub-Repo, Repo-HEAD-Commit `430db6c`)
  bzw. `applications_user/lora_app/` im RogueMaster-Bündel (App-ID `lora_relay`,
  `fap_category="GPIO"`).
- Zwei Kerndateien vollständig gelesen: `lora.c` (1064 Zeilen, SX1262-Low-Level-Treiber, Port
  der Arduino-Bibliothek `thekakester/Arduino-LoRa-Sx1262`) und `lora_relay.c` (2732 Zeilen,
  App-Logik/UI). Dazu ein vendortes `text_input`-Eingabemodul und `validators.c` (54 Zeilen)
  überflogen — reines Flipper-Scene-/Eingabe-Boilerplate, kein eigener Fund.

## Was
Benötigt das "Electronic Cats Flipper Add-On: Sub-GHz" (SX1262-LoRa-Modul am
Erweiterungsport). Konfigurierbare LoRa-Parameter (Frequenz 150–960 MHz, Bandbreite,
Spreading Factor SF5–SF12, Coding Rate, Syncword) plus vorgefertigte LoRaWAN-US915/
EU868-Kanalmenüs. Sniffer-Ansicht zeigt empfangene Pakete live (Hex, RSSI, SNR) und reicht sie
zusätzlich als binäres Frame (`@S`+Länge+Payload+RSSI+SNR+`@E\r\n`) über einen zweiten
USB-CDC-Kanal weiter (daher "Relay"). Optional werden Sniff-Sitzungen als JSON-Zeilen
(Datum/Zeit/Frequenz/BW/SF/RSSI/Payload-Hex) auf die SD-Karte geloggt. Separate
Transmitter-Ansicht liest eine zuvor erzeugte Log-Datei zeilenweise ein und sendet die darin
enthaltenen `"payload":"<hex>"`-Felder erneut über LoRa (Replay).

## Nutzen für mich
Füllt eine komplett fehlende Kategorie (LoRa/LoRaWAN) im eigenen Katalog, mit demselben
Erweiterungsport-Modul-Muster wie die bereits dokumentierten GPIO/Serial-Bus-Funde (gleicher
Hersteller wie RS485 Modbus/CANBUS). Ergänzt `g4meover-deck`s `lorix_link.py` um ein
eigenständiges Flipper-Gegenstück ohne Pi-Abhängigkeit — für schnelles LoRa-Recon direkt am
Gerät, bevor eine vollständige Pi-gestützte Analyse nötig wird.

## API-87-Status
✅ Kompatibel, geprüft. Gegen eine frische, unabhängige Zweitkopie von `G4MEOVER18/G4MEOVER-FW`
(Tag-Commit `b359f6f`, `mntm-012`) verifiziert: alle vier genutzten Erweiterungsport-Pins
(`gpio_ext_pa4/pc0/pc1/pc3`) sowie die drei zweckentfremdeten internen Pins (`gpio_swclk` als
LED-Beacon, `gpio_usart_tx`/`gpio_usart_rx` als Antennenschalter/Busy-Leitung) byteidentisch
vorhanden — dasselbe "fremde GPIO-Pins für ein Zusatzmodul zweckentfremden"-Muster wie bei
anderen SPI-Erweiterungsport-Apps. `furi_hal_spi_bus_handle_external`,
`furi_hal_usb_set_config()`/`usb_cdc_dual`/`usb_cdc_single`/`furi_hal_cdc_send()` (zweiter
USB-CDC-Kanal, identisches Muster zu GPIO Logic Analyzer), Standard-GUI-/Storage-/
`DialogsFileBrowserOptions`-APIs sowie `furi_hal_rtc_get_datetime()` — alle byteidentisch. Kein
Port-Hindernis, reines externes FAP.

## Quelltext-Review (Speichersicherheit)
`lora.c` (SX1262-SPI-Treiber): durchgehend fest dimensionierte SPI-Kommandopuffer
(`spiBuff[32]`), jede Nutzlastlänge wird vor dem eigentlichen Radio-Zugriff auf 255 Byte
(`transmit()`) bzw. auf die übergebene `buffMaxLen` (`lora_receive_async()`) geklemmt — kein
eigenständiger Fund in dieser Datei.

`lora_relay.c` (App-Logik): ein Fund.
- **Stack-Buffer-Overflow** (behoben, Port-Vorschlag inkl. Fix siehe
  `firmware-mining/lora-relay-app-port.md`): `lora_view_sniffer_draw_callback()` baut beim
  Logging eine JSON-Zeile in `final_string[400]` per `snprintf(final_string, 666, ...)` —
  die `666` ist ein Literal statt `sizeof(final_string)` (== 400). Der dominante variable Anteil
  des Formatstrings ist die Hex-Darstellung des zuletzt per LoRa empfangenen, vollständig
  fremdbestimmten Pakets (bis zu 255 Byte Nutzlast → bis zu 510 Hex-Zeichen) — ausgelöst allein
  durch den Empfang eines hinreichend langen LoRa-Pakets während aktivem "Recording", ganz ohne
  Pairing/Authentifizierung (LoRa kennt keinen Verbindungsaufbau). Mit Host-Repro (fünf
  Szenarien, fork-isolierte ASan-Tests, Regressions- und Äquivalenztest) unter ASan/UBSan bei
  `-O0`/`-O1` bestätigt — identisches Muster wird von GCCs `_FORTIFY_SOURCE`-Prüfung bereits
  statisch erkannt.
- Der Transmitter-/Replay-Pfad (`tx_payload()`, `send_data()`) liest Log-Dateien zeilenweise in
  einen festen `char buffer[256]` (korrekt gegen `buffer_index >= 256 - 1` geklemmt) und
  extrahiert das `"payload":"..."`-Feld per `strstr()`/`strchr()` in einen dynamisch (VLA)
  dimensionierten `payload[length + 1]`-Puffer — Länge kommt aus derselben, zuvor gelesenen
  Zeile, kein externer Einfluss über die reine Dateigröße hinaus. Setzt bereits physischen
  Zugriff auf die SD-Karte (eigene oder autorisiert geprüfte Log-Datei) voraus, eigenständig
  nicht als Fund dokumentiert.

## Risiko / Sicherheitsrahmen
Reines Empfangs-/Logging-Werkzeug für ein eigenes Zusatzmodul, kein Angriffswerkzeug gegen
fremde LoRa-Netze. Der dokumentierte Speicherfehler ist über jeden in Reichweite empfangenen
LoRa-Funkverkehr auslösbar (kein Pairing nötig) — für autorisierte Tests relevant, um die eigene
Sniff-Hardware/-Firmware auf Robustheit gegen lange Pakete zu prüfen, bevor sie im Feld
eingesetzt wird.
