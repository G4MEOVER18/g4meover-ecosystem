# Bambu RFID — API-87-Status

Neue Recherche außerhalb bestehender Rückstellpunkte: gezielt nach einer bislang fehlenden
Unterkategorie innerhalb NFC/RFID gesucht — Dekodierung proprietärer Hersteller-Tag-Formate über
eine bekannte, UID-abgeleitete Schlüsselherleitung, statt reiner Schlüsselwiederherstellung
(MFKey/Nested), Klon-/Reset-Funktion (NFC Magic) oder Risikoscoring (Access Audit).

## Quelle
- `DanTheMan827/bambu-rfid-flipper` (eigenständiges GitHub-Repo, `fap_weburl` in
  `application.fam`), aktueller Stand Commit `7709f2a` (2026-08-15), Version `0.2.0`. Zusätzlich
  über RogueMaster verbreitet (`RogueMaster/flipperzero-firmware-wPlugins`, Branch `420`,
  `applications/external/bambu_rfid/`).
- Beide Fassungen unabhängig geklont und alle zwölf Quelldateien per `diff -b -B` (Whitespace-
  unabhängig) gegenübergestellt: durchgängig identisch bis auf `clang-format`-Zeilenumbrüche
  (RogueMaster formatiert strenger) — keine funktionale Abweichung. Das Ursprungsrepo ist damit
  die primäre, RogueMaster lediglich eine unverändert mitgeführte Zweitquelle.
- Credits im eigenen README: `Bambu-Research-Group/RFID-Tag-Guide` (KDF- und Tag-Speicherlayout-
  Dokumentation, öffentlich reverse-engineered), `queengooborg/Bambu-Lab-RFID-Library`
  (Felddumps/Formatumwandlung), `RfidResearchGroup/proxmark3` (Referenzimplementierung der
  Schlüsselherleitung), `DanTheMan827/AmiiboZero` (UI-/Build-Vorlage desselben Autors).

## Was
Liest Bambu-Lab-Filamentspulen-RFID-Tags (MIFARE Classic 1K) über die eingebaute Antenne, ohne
Zusatzhardware. Leitet aus der 4-Byte-UID über HMAC-SHA256/HKDF (fester, öffentlich dokumentierter
Master-Schlüssel plus Kontext `"RFID-A"`/`"RFID-B"`) alle 16 Sektor-Key-A/-B-Paare her, liest alle
64 Blöcke, dekodiert Material, Filamenttyp, Primär-/Sekundärfarbe, Spulengewicht, Filamentdurchmesser
und -länge, Trocknungs-/Bett-/Düsentemperaturen, Produktionsdatum und Tray-UID. Speichert jeden
Scan als Fünf-Datei-Bündel (Rohdump `.bin`, Proxmark-artiges `mfc v2`-JSON inkl. Zugriffsbedingungen-
Klartext, `.nfc`-Datei im eigenen Flipper-Format, Schlüsseldatei, `README.md`-Zusammenfassung) in
einem UID-benannten Ordner unter `/ext/apps_data/bambu_rfid/tags/`, mit Browser zum erneuten
Durchsehen gespeicherter Scans. Versucht an keiner Stelle, die zusätzliche 2048-Bit-RSA-Signatur zu
fälschen, mit der Bambu eigene Tags gegen Drittanbieter-Nachbauten validiert — reine Lese-/
Export-Funktion, kein Klon-/Schreibpfad.

## Nutzen für mich
Ergänzt die bestehende MIFARE-Classic-Werkzeugkette (MFKey/Nested für unbekannte Schlüssel, NFC
Magic für Klon-/Reset-Medien, Mifare Fuzzer für UID-Grenzwerttests) um eine Herstellerformat-
*Dekodierungs*-Funktion mit bereits bekannter Schlüsselherleitung — kein Angriff auf ein
Zugangskontrollsystem, sondern Inventar-/Kompatibilitätsprüfung eigener 3D-Druck-Verbrauchsmaterial-
Spulen (Materialtyp/Temperaturprofil vor dem Einlegen verifizieren, eigene Spulen katalogisieren,
Dump als Referenz für eine spätere eigene Drittanbieter-Spulen-Kompatibilitätsauswertung sichern).
Fügt dem Katalog eine neue Zeile in der bestehenden NFC/RFID-Kategorie hinzu.

## API-87-Status
✅ Kompatibel, vollständig gegen `G4MEOVER18/G4MEOVER-FW` (`main`, `b359f6f`) verifiziert.

- Alle verwendeten NFC-/MIFARE-Classic-Symbole (`nfc_alloc/_free`, `nfc_poller_alloc/_detect/_free`,
  `nfc_poller_get_data`, `nfc_device_alloc/_free/_load/_get_data/_get_protocol`,
  `mf_classic_alloc/_free/_get_uid/_is_block_read`, `mf_classic_poller_sync_detect_type`,
  `mf_classic_poller_sync_read`) sowie alle Storage-Symbole
  (`storage_common_mkdir`, `storage_dir_open/_read/_close`, `storage_file_alloc/_free/_open/_close/
  _read/_write/_size`) per `grep` gegen den eigenen Quellbaum bestätigt — jedes vorhanden.
- Die beiden Kern-Signaturen zusätzlich wörtlich gegen `mf_classic_poller_sync.h` abgeglichen:
  `mf_classic_poller_sync_detect_type(Nfc*, MfClassicType*)` und
  `mf_classic_poller_sync_read(Nfc*, const MfClassicDeviceKeys*, MfClassicData*)` — exakte
  Parameterreihenfolge/-typen, keine API-Drift seit `0.2.0`.
- Krypto-Abhängigkeit (`bambu_crypto.cpp`) ruft nur `mbedtls_md_info_from_type(MBEDTLS_MD_SHA256)`
  und `mbedtls_md_hmac()` auf (eigene manuelle HKDF-Expansion über wiederholtes HMAC, kein Aufruf
  von `mbedtls_hkdf()` selbst) — `lib/mbedtls_cfg.h` listet `MBEDTLS_MD_C` und `MBEDTLS_SHA256_C`
  bereits als aktiviert; kein zusätzliches mbedTLS-Feature nötig, kein Firmware-Patch.
- `application.fam`: `fap_category="NFC"` (bei mir bereits bestehende Kategorie), `fap_libs=
  ["mbedtls"]`, `targets=["f7"]`, kein Erweiterungsport, keine Zusatzhardware.

## Quelltext-Review (Speichersicherheit) und Befund
Alle zwölf Quelldateien (`main.cpp`, `bambu_reader.cpp/.h`, `bambu_tag.cpp/.h`,
`bambu_storage.cpp/.h`, `bambu_export.cpp/.h`, `bambu_crypto.cpp/.h`, `bambu_rfid.h`,
`ui/ui_manager.cpp/.h`) gelesen. Durchweg sorgfältig: jeder `snprintf()`-Aufruf prüft Rückgabewert
und Zielgröße vor Weiterverwendung, Pfad-Namen werden vor Dateizugriffen gegen `safe_name()`/
`safe_relative_path()` (keine `/`, `\`, `..`) gefiltert, die UID-Stabilität wird nach dem Lesen
erneut mit der vor dem Scan erfassten UID verglichen (`"Tag changed during scan"`), ein
`MfClassicErrorPartialRead` wird nur akzeptiert, wenn zusätzlich alle 64 Blöcke tatsächlich als
gelesen markiert sind. Kein eigener Speicherfehler gefunden — anders als bei mehreren vorherigen
Katalog-Funden (Mifare Fuzzer, CAN-Bus-Cluster, RS485 Modbus) liegt hier keine fehlende
Längenprüfung, kein Leck und kein unbegrenzter Schleifenpfad vor. Einzige Beobachtung, kein Fehler:
Mehrfarben-Spulen mit `color_count` größer als 2 (laut Tag-Format theoretisch möglich) werden
korrekt als `color_count` angezeigt, aber ohne zusätzliche Farbwerte jenseits der zweiten Farbe —
eine dokumentierte Funktionsgrenze, keine Fehlinterpretation vorhandener Daten.

## Aufwand
Gering. Reine externe `.fap`, keine Firmware-Änderung nötig, keine Zusatzhardware — läuft über die
eingebaute NFC-Antenne.

## Sicherheitsrahmen
Rein lesend gegenüber eigenen Filamentspulen, keine Interaktion mit einem fremden System und keine
Signaturfälschung — deckungsgleich mit dem eigenen defensiven/autorisierten Rahmen (CONSTRAINTS
Punkt 5), da ausschließlich eigene Verbrauchsmaterial-Tags betroffen sind, keine
Zugangskontroll- oder Identifikationsmedien.

## Offen
- Lokale Bauprobe (`ufbt`) und Testlauf gegen eine eigene Bambu-Lab-Filamentspule.
- Neue Katalog-Zeile unter "NFC / RFID" ergänzt.
