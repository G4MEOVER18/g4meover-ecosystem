# NFC: DESFire-Dateisettings- und MFUL-FAST_READ-Stack-Overflow-Fixes (Official-dev vs. mntm-012)

## Was
Ich habe den `dev`-Branch von `flipperdevices/flipperzero-firmware` seit dem Stand, auf dem
mntm-012 (2025-12-31) basiert, auf Sicherheits-/Speicher-Fixes durchgesehen (offener
Backlog-Punkt „Official-dev seit mntm-012: Bugfixes/Sicherheits-/Stabilitäts-Commits, die ich
übernehmen sollte"). Zwei zusammengehörige Stack-Buffer-Overflow-Fixes aus PR #4369
(@qp-x-qp, gemerged 30.06.2026) sind für mich relevant, weil beide Codepfade in meiner
mntm-012-Basis unverändert vorhanden sind und beide über fremd kontrollierte NFC-Eingaben
(Kartendaten bzw. Reader-Kommandos gegen meine Emulation) auslösbar sind:

**1. DESFire-Dateisettings-Parser** (`lib/nfc/protocols/mf_desfire/mf_desfire_i.c`,
`mf_desfire_file_settings_parse()`): Die bisherige Bounds-Prüfung für
`additional_access_rights_len` vergleicht die Länge fälschlich gegen eine mit
`sizeof(MfDesfireFileAccessRights)` **hochskalierte** Schwelle statt gegen die tatsächliche
Kapazität des Ziel-Arrays:

```c
// mntm-012 (fehlerhaft):
if(additional_access_rights_len > MF_DESFIRE_MAX_KEYS * sizeof(MfDesfireFileAccessRights))
    break;
...
memcpy(
    &file_settings_temp.access_rights[1],
    &data_ptr[bytes_processed],
    additional_access_rights_len * sizeof(MfDesfireFileAccessRights));
```

`file_settings_temp.access_rights[]` hat Platz für `MF_DESFIRE_MAX_KEYS` Einträge, ab Index 1
also nur `MF_DESFIRE_MAX_KEYS - 1` freie Slots. Die Schwelle lässt aber Werte bis
`MF_DESFIRE_MAX_KEYS * sizeof(MfDesfireFileAccessRights)` durch — bei jeder Struct-Größe über
1 Byte ein Vielfaches der echten Kapazität. Eine präparierte DESFire-Karte (oder eine als
`.nfc`-Datei eingespielte, manipulierte Response beim `GET_FILE_SETTINGS`-Response-Parsing)
kann so `additional_access_rights_len` so wählen, dass der anschließende `memcpy()` weit über
`file_settings_temp.access_rights[]` hinausschreibt — `file_settings_temp` liegt auf dem Stack
der Parser-Funktion.

**2. MFUL-`FAST_READ`-Handler** (`lib/nfc/protocols/mf_ultralight/mf_ultralight_listener.c`,
`mf_ultralight_listener_fast_read_handler()`): Beim Emulieren einer MIFARE-Ultralight-/NTAG-Karte
berechnet der Handler die angeforderte Seitenzahl aus zwei vom externen Reader gesendeten Bytes
(`start_page`, `end_page`) und schreibt sie in ein **festes** Stack-Array:

```c
// mntm-012 (fehlerhaft — kein Cap-Check vor der Nutzung von page_cnt):
MfUltralightPage pages[64] = {};
uint8_t page_cnt = (end_page - start_page) + 1;
mf_ultralight_listener_perform_read(pages, instance, start_page, page_cnt, do_i2c_check);
bit_buffer_copy_bytes(instance->tx_buffer, (uint8_t*)pages, page_cnt * 4);
```

Die einzige vorherige Grenzprüfung ist `end_page > pages_total - 1` — und `pages_total` ist bei
größeren NTAG-Typen (z. B. NTAG216 mit 231 Seiten) deutlich größer als die 64 Slots von
`pages[]`. Ein externer Reader, der meinen Flipper beim Emulieren einer NTAG216-Karte mit
`FAST_READ(start=0, end=230)` anspricht, lässt `page_cnt = 231` zu — `perform_read()` und der
anschließende `bit_buffer_copy_bytes()` greifen dann weit über das 64-Element-Array hinaus.
Das ist über die reine Denial-of-Service-Wirkung hinaus interessant: Ich nutze meine
NFC-Emulation aktiv in autorisierten Pentest-Szenarien (Badge-Klonen/Ersatz-Tag gegen fremde
Reader) — genau dort ist der auslösende Codepfad (Emulation gegen einen fremden Reader)
Kernfunktion, nicht Randfall.

## Nutzen
- Schließt zwei über NFC-Eingaben fremd auslösbare Stack-Buffer-Overflows in Codepfaden, die ich
  aktiv nutze: DESFire-Dateisettings-Parsing (beim Lesen/Einlesen fremder DESFire-Karten) und
  MFUL/NTAG-Emulation (mein Standard-Pentest-Anwendungsfall: eigenes Gerät emuliert eine Karte
  gegenüber einem fremden Reader).
- Beide Fixes sind winzig und rein additiv/korrigierend (eine Vergleichsformel, ein zusätzlicher
  Bounds-Check) — kein Strukturumbau, kein API-Bruch, direkt gegen meine mntm-012-Basis
  anwendbar, da beide Dateien seit dem mntm-012-Stand in dieser Hinsicht unverändert sind.
- Stärkt die Robustheit meines eigenen Geräts genau in dem Moment, in dem es am meisten exponiert
  ist: aktive Emulation gegenüber einem nicht vertrauenswürdigen Reader bzw. Einlesen einer nicht
  selbst erzeugten Karte — beides Standardsituationen in autorisierten Pentest-Einsätzen.

## Betroffene Dateien (lokal)
Änderung (je 1 Zeile, kein neuer Code):
- `lib/nfc/protocols/mf_desfire/mf_desfire_i.c` — Bounds-Vergleich in
  `mf_desfire_file_settings_parse()` von
  `additional_access_rights_len > MF_DESFIRE_MAX_KEYS * sizeof(MfDesfireFileAccessRights)`
  auf `additional_access_rights_len > MF_DESFIRE_MAX_KEYS - 1` ändern.
- `lib/nfc/protocols/mf_ultralight/mf_ultralight_listener.c` — in
  `mf_ultralight_listener_fast_read_handler()` direkt nach der Berechnung von `page_cnt` und vor
  dem Aufruf von `mf_ultralight_listener_perform_read()` einfügen:
  ```c
  if(page_cnt > COUNT_OF(pages)) {
      command = MfUltralightCommandNotProcessedNAK;
      break;
  }
  ```

Kein neuer Include, kein neuer State, keine Registry-Änderung nötig — beide Fixes sind rein
lokal in ihrer jeweiligen Funktion.

## Risiko
- Sehr niedrig: beide Änderungen verschärfen ausschließlich eine bestehende Bounds-Prüfung bzw.
  ergänzen eine fehlende — es wird kein gültiger Fall enger als vorher erlaubt (DESFire-Dateien
  mit korrekt ≤ `MF_DESFIRE_MAX_KEYS - 1` zusätzlichen Access-Rights und `FAST_READ`-Anfragen
  innerhalb der 64-Seiten-Pufferkapazität verhalten sich identisch zu vorher).
- Vor dem Übernehmen `MF_DESFIRE_MAX_KEYS`-Definition und Layout von
  `file_settings_temp.access_rights[]` in meiner mntm-012-Basis gegenlesen (Momentum kann bei
  DESFire-Structs von Stock/Unleashed abweichen) — falls die Konstante oder das Array-Layout
  divergiert, den Vergleich entsprechend anpassen statt blind zu übernehmen.
- Bei `mf_ultralight_listener.c` prüfen, ob meine Basis `COUNT_OF()` bereits im Scope hat (in
  Stock/Unleashed/Momentum Standard-Makro aus `core/common_defines.h`, in aller Regel bereits
  eingebunden, da im selben File für andere Handler verwendet).
- Der Build (`fbt`/`ufbt`) deckt jede Signatur-/Konstantenabweichung als Compile-Fehler auf.
- Funktionale Prüfung ohne Hardware nur eingeschränkt möglich (reine Bounds-Logik lässt sich mit
  einem host-kompilierbaren Mini-Repro der Vergleichsformel nachrechnen); der eigentliche
  Bug-Pfad (fremd kontrollierte NFC-Daten gegen Parser bzw. Emulation) braucht einen echten Test
  mit einer NTAG216-Emulation gegen einen Reader bzw. einer präparierten DESFire-Testkarte —
  lokal, nicht Teil dieses Repos.

## Port-Skizze
1. Beide Funktionen in meiner mntm-012-Basis öffnen:
   `lib/nfc/protocols/mf_desfire/mf_desfire_i.c::mf_desfire_file_settings_parse()` und
   `lib/nfc/protocols/mf_ultralight/mf_ultralight_listener.c::mf_ultralight_listener_fast_read_handler()`.
2. Gegenlesen, ob beide Codestellen identisch zum oben zitierten mntm-012-Stand sind (Momentum
   verändert NFC-Code gelegentlich eigenständig) — falls schon anders/bereits gefixt, diesen
   Eintrag im Backlog als erledigt/nicht-anwendbar vermerken.
3. DESFire: die eine Vergleichszeile wie oben ändern (`> MF_DESFIRE_MAX_KEYS - 1` statt
   `> MF_DESFIRE_MAX_KEYS * sizeof(MfDesfireFileAccessRights)`).
4. MFUL: den vierzeiligen Bounds-Check direkt nach `uint8_t page_cnt = (end_page - start_page) + 1;`
   und vor dem ersten Gebrauch von `page_cnt` in `mf_ultralight_listener_perform_read()` einfügen.
5. Host-seitiges Mini-Repro der reinen Vergleichslogik (ohne Flipper-SDK, nur die zwei
   Bedingungen mit Beispielwerten wie `MF_DESFIRE_MAX_KEYS=14`, `page_cnt` bis 231) durchrechnen,
   um zu bestätigen: alte Formel lässt den Overflow-Fall durch, neue Formel blockt ihn, beide
   verhalten sich für reguläre kleine Werte identisch.
6. Build: `ufbt` (bzw. `fbt firmware_all`).
7. Funktionstest lokal: NTAG216 (oder anderen >64-Seiten-Typ) emulieren und mit einem
   `FAST_READ` über den vollen Seitenbereich gegen die Emulation lesen (eigener Reader/zweiter
   Flipper) — vorher ggf. Crash/Hardfault, nachher sauberes NAK oder korrektes Read innerhalb der
   Kapazität. DESFire-Seite: mit einer eigenen Testkarte/eigenem `.nfc`-Dump mit bewusst
   überzähligem `additional_access_rights`-Feld gegenlesen, dass der Parser jetzt sauber
   abbricht statt zu kopieren.

## Quelle
- `flipperdevices/flipperzero-firmware`, `dev`-Branch, PR #4369 (@qp-x-qp, gemerged 30.06.2026):
  „NFC: Fix stack buffer overflows in MFUL FAST_READ and DESFire file settings parsers" —
  `lib/nfc/protocols/mf_desfire/mf_desfire_i.c`, `lib/nfc/protocols/mf_ultralight/mf_ultralight_listener.c`.
- Momentum-Firmware, Tag `mntm-012`: dieselben beiden Dateien als Vergleichsbasis (Fix dort noch
  nicht vorhanden).
