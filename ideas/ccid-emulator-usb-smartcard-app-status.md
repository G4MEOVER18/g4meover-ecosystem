# CCID Emulator — API-87-Status

Neue Recherche außerhalb bestehender Rückstellpunkte: gezielt nach einer bislang komplett
fehlenden Kategorie gesucht (USB-Smartcard-Emulation über ISO 7816-4/CCID, Gegenstück zu den
bereits zahlreichen NFC/RFID-Lesegerät-Kategorien, aber auf der USB-Hostseite statt RF).

## Quelle
- Ursprung laut `application.fam`: `PentestSuite & barkandbite/flipper_suite` (`fap_author`,
  `fap_weburl` zeigt auf `barkandbite/flipper_suite`) — dieselbe Quellfamilie wie UART Sniff,
  SubGHz Jammer Detector, SPI Flash Dump, NFC Fuzzer u. a. bereits katalogisierte Funde.
- Geprüfte Fassung: sparse geklonter `RogueMaster/flipperzero-firmware-wPlugins`
  (Branch `420`, `applications/external/ccid_emulator/`), sechs Quelldateien (`ccid_emulator.c/.h`,
  `ccid_handler.c/.h`, `ccid_usb.c/.h`) plus `card_parser.c/.h`, vollständig gelesen (2822 Zeilen).
  Zusätzlich das Ursprungsrepo `barkandbite/flipper_suite` separat geklont — dort fehlen
  `ccid_usb.c/.h` und `card_parser.c/.h` (nur `ccid_emulator.c/.h` + `ccid_handler.c/.h`
  vorhanden), zusätzlich liegen dort sieben statt zwei Beispielprofile unter
  `ccid_emulator_sample_cards/` (`test_card`, `piv_emulator`, `openpgp_card`, `yubikey_oath`,
  `emv_mastercard_contact`, `emv_visa_contactless`, `iso7816_memory`) — die RogueMaster-Fassung ist
  der vollständigere, eigenständig baubare Stand und damit die hier geprüfte Grundlage.

## Was
Macht den Flipper über USB zu einem Standard-CCID-Smartcard-Lesegerät mit virtuell gesteckter
Karte. Der Host sieht einen gewöhnlichen CCID-Reader (kein Treiberwechsel nötig, PC/SC-/
WinSCard-kompatibel) und bekommt auf jede ISO-7816-4-APDU eine Antwort aus einer geladenen
`.ccid`-Kartenprofildatei (INI-artiges Format: `[card]` mit Name/Beschreibung/ATR, `[rules]` mit
Kommando→Antwort-Zuordnungen inkl. `??`-Byte-Wildcard, `[default]`-Fallback). Mitgeliefert: ein
Live-APDU-Monitor (Kommando/Antwort-Verlauf auf dem Display, Export als Log-Datei auf SD) sowie
beim ersten Start automatisch angelegte Beispielprofile (einfache Testkarte, NIST-PIV-Teilmenge,
generische Java-Card-ISD-Auswahl).

## Nutzen für mich
Bislang deckt mein Katalog nur die RF-Seite von Smartcards/Zutrittsmedien ab (PicoPass/Seader für
iClass/SE/Desfire, MFKey/Nested/NFC Magic für MIFARE, Access Audit für Risikoscoring) — alles
Flipper-als-Leser bzw. Flipper-als-Tag. CCID Emulator dreht die Rolle um: der Flipper simuliert die
Karte gegenüber einem fremden USB-Host/einer Middleware (PIV/CAC/OpenPGP/PC-SC-Software). Direkte
Anwendungen im eigenen autorisierten Rahmen: Smartcard-Middleware eines Zielsystems gegen
selbstgebaute Antwortsequenzen testen (Fehlerpfade, unerwartete APDU-Reihenfolgen, Grenzwerte ohne
echte Produktionskarte), eigene `.ccid`-Profile für Lehr-/Demo-Zwecke bauen, bevor eine echte
Smartcard beschafft wird. Neue, bisher komplett fehlende Katalog-Kategorie.

## API-87-Status
✅ Kompatibel, geprüft — und mit einem für mich besonders aussagekräftigen Fund zur Herkunft der
benötigten Header.

- Der kritische Header `usb_ccid.h` (CCID-USB-Klassendeskriptoren, in keiner der beiden App-Quellen
  selbst mitgeliefert) kommt aus dem `lib/libusb_stm32`-Submodul. Dessen Pin in
  `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`) ist Commit `6ca2857`
  ("CCID: code formatting and doxygen docs") auf dem offiziellen, upstream
  `flipperdevices/libusb_stm32`-Repo selbst — **exakt derselbe Commit**, den mein eigenes
  `G4MEOVER-FW` (`main`, `b359f6f`) für `lib/libusb_stm32` pinnt. CCID-Unterstützung ist damit
  kein RogueMaster-Fork-Sonderfall, sondern bereits Teil des offiziellen USB-Stack-Submoduls, den
  ich selbst führe — der Header ist bei mir vorhanden, ohne dass ich irgendetwas ändern müsste.
- `targets/f7/api_symbols.csv` (mein API-87.1-Stand) listet sowohl `lib/libusb_stm32/inc/usb_ccid.h`
  als auch `lib/libusb_stm32/inc/usbd_core.h` als freigegebene (`Header,+`) App-SDK-Header — genau
  die beiden Header, über die `ccid_usb.c` den USB-Gerätestack direkt anspricht (eigene
  `FuriHalUsbInterface`, eigene Endpunkt-/Deskriptor-Konfiguration, kein Firmware-Patch nötig).
- Bemerkenswerter Nebenfund: mein API-87.1-Stand führt zusätzlich **noch** eine ältere,
  inzwischen entfernte native Firmware-HAL-Anbindung (`targets/furi_hal_include/furi_hal_usb_ccid.h`,
  Funktionen `furi_hal_usb_ccid_insert_smartcard()`/`_remove_smartcard()`/`_set_callbacks()`,
  Variable `usb_ccid` vom Typ `FuriHalUsbInterface`) — ein eigener Kommentar in `ccid_usb.h` der
  App bestätigt das ausdrücklich ("This used to live in the firmware HAL (furi_hal_usb_ccid). It
  now lives inside the app..."). Der RogueMaster-Firmwarestand selbst (Branch `420`, API 88.21,
  neuer als mein 87.1) hat diese native HAL-Anbindung bereits entfernt — nur noch der rohe
  `usb_ccid.h`-Header bleibt. Für mich folgenlos, da die App ohnehin ausschließlich ihren eigenen,
  SDK-Header-basierten Pfad nutzt und keine `furi_hal_usb_ccid_*`-Funktion aufruft — reine
  Dokumentation einer API-Historie, kein Hindernis.
- `application.fam`: `fap_category="USB"` (bei mir bereits existierende Kategorie, z. B. Bad USB),
  keine Erweiterungsport-/Zusatzhardware-Abhängigkeit — reine USB-Device-Rolle über den
  vorhandenen Flipper-USB-Anschluss.

## Quelltext-Review (Speichersicherheit) und Befund
Durchweg sorgfältig geprüfter Code — mehrere Kommentare verweisen bereits auf eine eigene frühere
Fehlerkorrekturrunde ("issue #59", Momentum-spezifische `furi_check`-Reihenfolgefixe bei
USB-Moduswechsel/Teardown, Timer-vs.-Mutex-Freigabereihenfolge beim App-Beenden). Im Einzelnen
geprüft, kein eigener Speicherfehler gefunden:
- `card_parser.c`: Hex-/Wildcard-Parser (`parse_hex_string()`/`parse_hex_pattern()`) brechen bei
  Pufferüberlauf explizit mit Fehler ab statt stillschweigend zu kürzen (genau der in den
  Kommentaren benannte frühere Fund "issue #59"); alle Soll-Längen (`CCID_EMU_MAX_CMD_LEN=64`,
  `CCID_EMU_MAX_RESP_LEN=128`, `CCID_EMU_MAX_ATR_LEN=33`) werden vor jedem Schreibzugriff geprüft.
- `ccid_handler.c`: Antwortlängen werden vor `memcpy()` zusätzlich gegen `CCID_MAX_DATA_BLOCK_LEN`
  (261 Byte, deckungsgleich mit dem USB-Transportpuffer aus `ccid_usb.c`) geklemmt — die
  App-eigene 128-Byte-Grenze liegt komfortabel innerhalb der tatsächlich verfügbaren
  USB-Paketgröße, kein Überlaufrisiko zwischen beiden Schichten.
- `ccid_emulator.c`: ATR-/Log-/Pfad-Pufferbau (`build_card_info_widget()`,
  `ccid_emulator_export_log()`, `discover_card_files()`) durchweg mit vorab berechneten
  Maximalgrößen und `snprintf()`/Restplatz-Prüfung vor jedem Teilschritt; der einzige
  Scroll-Balken-Berechnungscode mit Division (`apdu_monitor_draw()`) ist durch die umschließende
  `if(total > visible_lines)`-Bedingung vor Division durch Null geschützt.
- Die in `ccid_handler.c` sichtbare USB-VID/PID-Preset-Auswahl (`ccid_usb_presets[]`, u. a.
  "Yubikey") ist auf den ersten Blick wie nicht verdrahtet wirkende Totlogik (`(void)preset`) —
  bei genauerem Blick aber bereits selbst ehrlich gemacht: die Einstellungsseite
  (`settings_build()` in `ccid_emulator.c`) zeigt dafür nur ein festes, nicht wählbares
  "USB Device: Default (SDK fixed)"-Label mit Kommentar "SDK usb_ccid interface does not support
  custom descriptors" — kein Fund, sondern bereits dokumentierte Einschränkung.
- Einzige Doku-Ungenauigkeit (kein Code-Fehler): das `README.md` nennt als mitgelieferte
  Beispielprofile `test_card.ccid`/`piv_emulator.ccid`; die tatsächlich beim ersten Start
  automatisch erzeugten Dateien (`ccid_card_write_sample()`) heißen `test_card.ccid`,
  `piv_card.ccid` (nicht `piv_emulator.ccid`) und `javacard.ccid`. Die im Ursprungsrepo
  mitgelieferten, nicht automatisch erzeugten Zusatzprofile (`openpgp_card`, `yubikey_oath`, zwei
  EMV-Profile, `iso7816_memory`) sind in dieser RogueMaster-Fassung nicht enthalten und müssten für
  deren Nutzung separat von dort übernommen werden.

## Aufwand
Gering. Reine externe `.fap`, keine Firmware-Änderung nötig (siehe API-87-Status), keine
Zusatzhardware — läuft über den ohnehin vorhandenen Flipper-USB-Anschluss.

## Sicherheitsrahmen
Passiv aus Sicht des eigenen Geräts (antwortet nur auf eingehende APDUs vom Host), aktiv aus Sicht
des angeschlossenen Hosts/der Middleware — nur gegen eigene Testsysteme oder ausdrücklich für den
Test freigegebene Middleware einsetzen, genau der im README selbst benannte Rahmen ("For
authorized security research, smartcard testing, and educational use only"), deckungsgleich mit
CONSTRAINTS Punkt 5.

## Offen
- Lokale Bauprobe (`ufbt` gegen die eigene Firmware-Basis) und Test gegen eine echte PC/SC-fähige
  Gegenstelle (z. B. `pcsc_scan`/`opensc-tool`, wie im README vorgeschlagen): erwartete
  Kartenerkennung mit dem mitgelieferten `test_card.ccid`, danach eine selbst geschriebene
  Regelzeile gegen eine bekannte APDU prüfen.
- Bei Bedarf die sieben zusätzlichen Beispielprofile aus `barkandbite/flipper_suite`
  (`ccid_emulator_sample_cards/`, insb. `openpgp_card.ccid`/`yubikey_oath.ccid`/die beiden
  EMV-Profile) manuell auf die SD-Karte übernehmen — in der RogueMaster-Fassung nicht automatisch
  mitgeliefert.
- Neue Katalog-Kategorie "USB Smartcard (CCID/ISO 7816-4)" ergänzt — kein weiterer Kandidat
  aus dieser Suche bekannt, erster Fund in dieser Kategorie.
