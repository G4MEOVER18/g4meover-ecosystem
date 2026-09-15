# MIFARE Plus SL3: Struct-Erweiterung + Serialisierung (Phase 2 des AES-Auth-Ports)

## Was

`firmware-mining/mfplus-mfultralight-aes-auth-port.md` hat den fehlenden MIFARE-Plus-SL3-/
MIFARE-Ultralight-AES-Support bei mir als mehrteiliges Portierungsvorhaben kartiert und in
sechs Phasen gestuft; Phase 1 (die reinen AES-128-Kryptografie-Primitive) ist bereits geliefert
und host-verifiziert. Diese Sitzung liefert **Phase 2** aus derselben Skizze: die
`MfPlusData`-Struct-Erweiterung um die SL3-Nutzlast (Blöcke, Sektor-/Admin-Schlüssel,
Konfigurationsblöcke, Originalitätssignatur, alle zugehörigen "bekannt"-Bitmasken) plus deren
vollständige (De-)Serialisierung (`mf_plus_sl3_data_save`/`_load`) und die dazugehörigen
Zugriffsfunktionen — reines Speicher-/Dateiformat, kein Karten-I/O, komplett ohne Hardware
nachrechenbar.

Ich habe die Quelle für Phase 2 aus demselben bereits gepinnten Unleashed-Stand entnommen wie
Phase 1 (`dev`, Commit `945234a76701d556346f131c88f29f0feeec4d52`, 2026-09-10, API 88.9):
`lib/nfc/protocols/mf_plus/mf_plus.h`, `mf_plus.c`, `mf_plus_i.h`, `mf_plus_i.c`. Gegen meine
eigene `mntm-012`-Basis (Tag-Commit `e1784e7418d8b074e971983ceb6fef0f37e52ae4`) frisch verglichen:
alle vier Dateien sind bis auf die SL3-Ergänzung sowie eine unabhängige, hier nicht angefasste
Änderung an `mf_plus_get_type_from_version()`/`mf_plus_get_type_from_iso4()` (Erkennungs-Probe
für SL0-vs-SL3 über SAK 0x20 plus ein auf ATS-Nibbles statt Volltreffer umgestelltes
Produkt-Matching — eigenständige, spätere Änderung, siehe "Nicht Teil dieses Patches")
byteidentisch zu meiner Basis. Der SL3-Teil selbst ist eine reine Ergänzung ohne jede Berührung
der bestehenden Identitätsfelder (`version`/`type`/`size`/`security_level`) oder ihrer
Lade-/Speicherfunktionen, die unverändert bleiben.

### Was die Erweiterung bringt

- **Struct-Felder** (`MfPlusData`, `mf_plus.h`): `signature_present`/`signature[56]`
  (Originalitätssignatur, level-unabhängig), `block[256]` + `block_read_mask[8]`
  (SL3-Datenblöcke mit Pro-Block-"gelesen"-Bitmap), `key_a[40]`/`key_b[40]` +
  `key_a_mask`/`key_b_mask` (Sektor-AES-Schlüssel, per Dictionary-Angriff wiederhergestellt),
  `admin_key[4]` + `admin_key_mask` (die vier wiederherstellbaren `0x90xx`-Admin-Schlüssel:
  Card Master/Card Config/L3 Switch/SL1 Card Auth — `0x9002`, der SL2→SL3-Schlüssel, wird
  bewusst nie wiederhergestellt und fehlt deshalb im Enum), `config_block[4]` +
  `config_read_mask` (die vier `0xB0xx`-Konfigurationsblöcke).
- **Geometrie-/Zugriffsfunktionen**: `mf_plus_get_block_count`/`_get_sector_count` (1K/2K/4K →
  64/128/256 Blöcke bzw. 16/32/40 Sektoren), `mf_plus_sector_get_first_block`/
  `_get_block_count` (4K-Layout: 32 kleine Sektoren zu 4 Blöcken, dann 8 große zu 16 Blöcken —
  exakt 256 Blöcke in Summe), `mf_plus_is_block_read`/`_is_key_found`/`_is_admin_key_found`/
  `_is_config_block_read` (öffentliche READ-Zugriffe, die zugehörigen `set_*`-Varianten bleiben
  intern für Poller/Serialisierer), `mf_plus_is_card_read` (vollständig vs. teilweise gelesen,
  block-basiert statt zähler-basiert — analog zu `mf_classic_is_card_read`),
  `mf_plus_get_read_sectors_and_keys` (Fortschrittsanzeige), `mf_plus_get_admin_key_name`/
  `_get_admin_key_address`/`_admin_key_type_from_address` (einzige Quelle für die
  Admin-Schlüssel-`0x90xx`-Adresstabelle, von Poller *und* Listener nutzbar).
- **`mf_plus_merge_update`**: überlagert nur tatsächlich neu wiederhergestellte Felder von
  `fresh` auf `base`, löscht nie ein bereits vorhandenes Feld — für "Update from Initial Card"
  nach einem abgebrochenen Nachlesen, ohne bereits gefundene Schlüssel/Blöcke zu verlieren.
- **Serialisierung** (`mf_plus_sl3_data_save`/`_load`, `mf_plus_i.c`): schreibt/liest einen
  `"Data format version"`-Schlüssel (aktuell `1`) gefolgt vom SL3-Nutzlastblock (nur bei
  `security_level == SL3`) und zuletzt, nur falls vorhanden, die Signatur. Jedes Byte-Feld wird
  als `"AA BB .."`-Hexstring geschrieben, ein unbekanntes Feld als gleich langer `"?? .."`-Lauf
  (`mf_plus_bytes_to_str`/`_str_to_bytes`) — ein alter, rein Identitäts-Dump ohne
  `"Data format version"`-Schlüssel lädt unverändert erfolgreich mit leeren SL3-Feldern (Rückwärts-
  kompatibilität zu vor Phase 2 gespeicherten Dumps), eine von einem *neueren*, mir unbekannten
  Format geschriebene Datei wird dagegen explizit abgelehnt statt als v1 fehlinterpretiert.

### Nicht Teil dieses Patches

- **`mf_plus_get_type_from_version()`/`_get_type_from_iso4()`** (Erkennung/Klassifizierung beim
  Poll) sind bei Unleashed inzwischen ebenfalls erweitert (SAK-0x20-Probe für SL0 vs. SL3, ATS-
  Nibble-Matching statt Volltreffer-Tabelle) — eine von der SL3-Struct-/Serialisierungsfrage
  unabhängige, spätere Änderung an der Karten-*Erkennung*. Ich lasse meine bestehende
  Erkennungslogik unverändert; sie liefert bereits denselben `security_level`/`size`, den die
  SL3-Nutzlast hier als Gate verwendet, nur (noch) ohne die feinere SL0-vs-SL3-Unterscheidung
  über SAK 0x20 — separat vorzumerken, falls relevant.
- **MIFARE-Ultralight-AES** (`MfUltralightAesKey`-Feld/-Serialisierung in `mf_ultralight.h`):
  bewusst auf eine eigene, künftige Sitzung verschoben, um diesen Patch auf eine Kartenfamilie
  zu begrenzen und prüfbar zu halten.
- **Phasen 3-6** aus der ursprünglichen Skizze (Poller-/Listener-Zustandsautomat, UI,
  Schlüsselwörterbuch-Anbindung) bleiben unverändert offen — sie bauen auf den hier gelieferten
  Feldern/Zugriffsfunktionen auf, sind aber ohne echte Hardware nicht sinnvoll weiterzuführen.

## Nutzen

- Schafft die Datengrundlage, auf der Phase 3 (Poller-Zustandsautomat) direkt aufsetzen kann:
  `mf_plus_set_block_read`/`_set_key_found`/`_set_admin_key_found`/`_set_config_block_read` sind
  bereits die exakten Schreibstellen, die ein künftiger SL3-Poller nach jeder erfolgreichen
  Authentifizierung/jedem gelesenen Block aufrufen wird.
- Macht einen bereits (z. B. über einen künftigen Poller oder manuell) teilweise gefüllten
  `MfPlusData`-Dump verlustfrei speicher-/ladbar — vorher hätte meine Basis jede SL3-Information
  beim Speichern stillschweigend verworfen, da die Felder schlicht nicht existierten.
  Ein "?? .."-Platzhalter macht dabei sichtbar, welche Bytes tatsächlich bekannt sind, statt
  einen unbekannten Wert als Null vorzutäuschen.
- Rückwärtskompatibel zu allen bereits vorhandenen, reinen Identitäts-Dumps (kein
  `"Data format version"`-Schlüssel → unverändertes Ladeverhalten) und vorwärts-sicher gegen ein
  künftiges Format v2 (explizite Ablehnung statt Fehlinterpretation).
- Reine additive Diff-Fläche: keine bestehende Funktion außer den fünf in "Betroffene Dateien"
  genannten Hook-Stellen wird geändert, keine bestehende Datenstruktur schrumpft oder ändert ihr
  Layout für bereits vorhandene Felder.

## Betroffene Dateien

Patches als Unified-Diffs gegen meine `mntm-012`-Basis (`e1784e7418d8b074e971983ceb6fef0f37e52ae4`)
in `mfplus-phase2-sl3-struct-serialization/`:

- `mf_plus.h.patch` — neue Defines/Structs/Enums (`MfPlusBlock`, `MfPlusKey`, `MfPlusKeyType`,
  `MfPlusAdminKeyType`), SL3-Felder in `MfPlusData`, zwölf neue öffentliche
  Zugriffsfunktions-Deklarationen.
- `mf_plus_i.h.patch` — sieben neue interne Deklarationen (`set_*`-Zugriffe,
  Admin-Adresstabellen-Helfer, `mf_plus_sl3_data_save`/`_load`).
- `mf_plus.c.patch` — fünf geänderte Hook-Stellen: `mf_plus_alloc` (ruft jetzt `mf_plus_reset`
  statt drei Felder einzeln zu setzen, damit die SL3-Felder beim Allozieren mit-genullt werden),
  `mf_plus_reset`/`_copy`/`_is_equal` (SL3-Felder ergänzt), `mf_plus_load`/`_save` (rufen
  `mf_plus_sl3_data_load`/`_save` nach den bereits vorhandenen Identitätsfeldern auf).
- `mf_plus_i.c.patch` — ein neuer Include (`<toolbox/hex.h>`, für `hex_char_to_uint8`) plus der
  komplette neue Abschnitt "SL3 payload" (Geometrie, Zugriffsfunktionen, `merge_update`,
  Serialisierung) ans Dateiende angehängt; keine bestehende Zeile im Rest der Datei geändert.

Lokale Anwendung: `patch -p0 < mfplus-phase2-sl3-struct-serialization/mf_plus.h.patch` (usw.) aus
`lib/nfc/protocols/mf_plus/` heraus, oder die vier Hunks von Hand einpflegen — jeder Patch wurde
per `patch --dry-run` gegen eine frische Kopie meiner eigenen Basisdateien verifiziert.

## Verifikation (auf dem Host, ohne Hardware)

`firmware-mining/repro-mfplus-phase2-sl3-struct-serialization/repro.c`
(`gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined`), Ergebnis `ALL CHECKS PASSED`
(0 Fehler, keine ASan-/UBSan-Meldung). Kompiliert und ausgeführt werden die **unveränderten**
`mf_plus.c`/`mf_plus_i.c` aus diesem Patch (nicht nur einzelne Funktionen herauskopiert) gegen
einen minimalen, aber realistischen Ersatz für `FlipperFormat`/`FuriString`/`Iso14443_4aData`
(eigene Header unter `stubs/`, dokumentiert im jeweiligen Dateikopf); die Erkennungsfunktionen
(`get_type_from_version`/`_iso4`/`version_parse`), die `<nfc/helpers/nxp_native_command.h>`/
`BitBuffer`/`simple_array` brauchen, sind bewusst ausgeklammert, da unverändert und von der SL3-
Nutzlast nicht berührt (siehe Dateikopf `mf_plus_i.h`/`mf_plus_i.c` im Repro-Verzeichnis).

Geprüft:

1. **Geometrie**: Block-/Sektorzahlen für 1K/2K/4K, 4K-Sektor-Layout summiert über alle 40
   Sektoren exakt auf 256 Blöcke, Admin-Schlüssel-Adresse rundet für alle vier Typen korrekt,
   `0x9002` (übersprungener SL2→SL3-Schlüssel) und eine beliebige Fremdadresse werden korrekt als
   "kein Admin-Schlüssel" erkannt.
2. **Masken/Zugriffe**: jede `is_*`/`set_*`-Paarung einzeln (Nachbar-Block/-Sektor/-Admin-Typ/
   -Config-Index bleibt beim Setzen eines anderen unberührt), `mf_plus_is_card_read` vor/nach
   vollständigem Setzen aller Blöcke einer 1K-Karte, `mf_plus_get_read_sectors_and_keys` mit
   exakt einem gesetzten Schlüssel.
3. **`mf_plus_reset`**: ein zuvor über `populate_partial_sl3_1k()` (bewusst *teilweise* gefüllte
   1K-SL3-Karte: alle Blöcke, nur Schlüssel A pro Sektor, zwei von vier Admin-Schlüsseln, drei
   von vier Config-Blöcken, Signatur vorhanden) befüllter `MfPlusData` wird nach `reset` an jeder
   geprüften Stelle (Masken, `security_level`, `signature_present`, Blockdaten,
   `block_read_mask`-Worte) wieder auf den Nullzustand zurückgesetzt.
4. **`mf_plus_copy`/`_is_equal`**: eine frische und eine befüllte Karte sind ungleich, nach
   `copy` gleich; sieben unabhängige Einzelbyte-/Masken-Abweichungen (Blockdaten, Schlüssel-A-
   Daten, Schlüssel-, Admin-, Config-Maske, `signature_present`, Signaturbyte) werden jede für
   sich von `mf_plus_is_equal` erkannt.
5. **Voller Save→Load-Rundlauf über die `FlipperFormat`-Ersatzimplementierung** für die unter 3.
   beschriebene teilweise gefüllte 1K-SL3-Karte plus eine per `mf_plus_set_uid` gesetzte
   Basiskarten-UID: `mf_plus_is_equal(saved, loaded)` nach dem Rundlauf, dazu einzeln
   nachgerechnet: alle 16 Sektoren als gelesen, genau 16 gefundene Schlüssel (nur A, kein B pro
   Sektor), die zwei gespeicherten Admin-Schlüssel vorhanden und die zwei nicht gespeicherten
   weiterhin unbekannt, drei von vier Config-Blöcken vorhanden, Signatur inhaltsgleich, Basis-
   karten-UID (Länge und Bytes) über den ohnehin unveränderten `iso14443_4a`-Teil erhalten.
6. **Rundlauf für eine Nicht-SL3-Karte** (SL1, 4K): Speichern/Laden liefert eine identische Karte
   ohne jede SL3-Feld-Berührung, `signature_present` bleibt `false`.
7. **Legacy-Dump ohne `"Data format version"`-Schlüssel**: `mf_plus_sl3_data_load` liefert `true`
   und lässt alle SL3-Felder genullt, statt den gesamten Ladevorgang fehlschlagen zu lassen —
   exakt das im Code selbst dokumentierte Rückwärtskompatibilitäts-Verhalten.
8. **Zukunfts-Format-Version** (`2` statt `1`): `mf_plus_sl3_data_load` lehnt explizit ab
   (`false`), statt die Nutzlast als v1 fehlzuinterpretieren.
9. **`mf_plus_merge_update`**: `fresh` mit neuem Schlüssel/Block und ohne Signatur überlagert
   `base`, ohne dessen bereits vorhandenen Schlüssel/Block/Signatur zu löschen; ein zweiter
   `fresh` *mit* neuerer Signatur überschreibt sie danach korrekt.
10. **Korruptes Feld**: ein auf zwei Zeichen gekürztes `"Block 0"`-Feld in der gespeicherten
    Repräsentation lädt als "unbekannt" (kein Absturz, kein falsch akzeptierter Wert) — der
    restliche Ladevorgang läuft unbeeinflusst weiter.

## Risiko

- **Niedrig**: rein additive Struct-Felder (kein bestehendes Feld verschiebt sich, `MfPlusData`
  wächst nur) und additive Funktionen; die fünf geänderten Hook-Funktionen in `mf_plus.c` fügen
  jeweils nur zusätzliche, klar abgegrenzte Zeilen an bestehende `do {} while(false)`-Ketten an,
  ohne bestehende Bedingungen zu verändern.
- `MfPlusData` wächst um grob 256×16 (Blöcke) + 2×40×16 (Sektorschlüssel) + 4×16 (Admin-Schlüssel)
  + 4×16 (Config-Blöcke) + 56 (Signatur) Byte ≈ 5,6 KB pro Instanz — auf dem Heap alloziert
  (`mf_plus_alloc` nutzt bereits `malloc`), nicht auf dem Stack; für den lokalen Build-/RAM-
  Verbrauch zu beachten, aber kein Korrektheitsrisiko.
- Die Serialisierung selbst ist durch die zehn Host-Fälle oben (inkl. Legacy-Kompatibilität,
  Versions-Ablehnung und einem gezielt korrupten Feld) breiter abgedeckt als nur der
  Erfolgsfall — verbleibendes Risiko liegt ausschließlich in den noch nicht portierten Phasen
  3-6 (Poller-/Listener-Zustandsautomat an echter Hardware), nicht in diesem Patch.

## Testhinweis (lokal)

1. Die vier `.patch`-Dateien in `lib/nfc/protocols/mf_plus/` anwenden, `ufbt` (bzw.
   `fbt firmware_all`) bauen — jede Struct-/Signaturabweichung fiele sofort als Compile-Fehler
   auf (z. B. falls sich `Iso14443_4aData`/`FlipperFormat` seit dem gepinnten Referenzstand bei
   mir geändert haben sollten).
2. Host-Repro laufen lassen (Befehl im Dateikopf von `repro.c`) — sollte weiterhin
   `ALL CHECKS PASSED` liefern, bevor überhaupt geflasht wird.
3. Auf dem Gerät: eine bereits vorhandene, rein identitätsbasierte MIFARE-Plus-`.nfc`-Datei
   (Alt-Dump ohne SL3-Daten) laden — muss unverändert wie vor diesem Patch laden (Legacy-Pfad).
4. Da Phase 3 (Poller) noch nicht portiert ist, gibt es aktuell keinen Weg, die SL3-Felder über
   echtes Karten-Lesen zu befüllen; ein struktureller Rundlauf-Test am Gerät selbst (Karte lesen
   → speichern → laden → vergleichen) wird erst mit Phase 3 sinnvoll möglich. Bis dahin deckt der
   Host-Repro die komplette hier gelieferte Logik ab.

## Quelle

- Unleashed-Firmware, `dev`-Branch, Commit `945234a76701d556346f131c88f29f0feeec4d52`
  (2026-09-10, API 88.9): `lib/nfc/protocols/mf_plus/mf_plus.h`, `mf_plus.c`, `mf_plus_i.h`,
  `mf_plus_i.c`.
- Momentum-Firmware, Tag `mntm-012` (Commit `e1784e7418d8b074e971983ceb6fef0f37e52ae4`): dieselben
  vier Dateien, Ausgangszustand für den Diff.
- Vorangehende Sitzung/Phase 1: `firmware-mining/mfplus-mfultralight-aes-auth-port.md`,
  `firmware-mining/repro-mfplus-mfultralight-aes-crypto/`.
