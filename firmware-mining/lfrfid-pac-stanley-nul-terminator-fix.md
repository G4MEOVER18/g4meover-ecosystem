# LFRFID: PAC/Stanley-Decoder ohne NUL-Terminator (Stack-Over-/Unterlesen) — Official-dev vs. mntm-012

## Was
Ich habe den offenen Backlog-Punkt „Official-dev seit mntm-012: weitere Sicherheits-/
Stabilitäts-Commits" weiterverfolgt (bisher nur PR #4369, NFC-DESFire/MFUL, geprüft) und den
`dev`-Branch von `flipperdevices/flipperzero-firmware` ab dort (Commit `0dd3681f`) bis zum
aktuellen Stand (`9148d50c`, Stand 2026-08-04) durchgesehen. PR #4427 (@munzzyy, gemerged
27.07.2026) behebt einen Bug im **LFRFID-PAC/Stanley-Decoder**
(`lib/lfrfid/protocols/protocol_pac_stanley.c`), der bei mir in der mntm-012-Basis unverändert
vorhanden ist (gegengeprüft gegen Tag `mntm-012`, Datei identisch zum hier zitierten
Vorher-Stand):

```c
// mntm-012 (fehlerhaft):
static void protocol_pac_stanley_decode(ProtocolPACStanley* protocol) {
    uint8_t asciiCardId[8];
    for(size_t idx = 0; idx < 8; idx++) {
        uint8_t byte = bit_lib_reverse_8_fast(bit_lib_get_bits(
            protocol->encoded_data,
            PAC_STANLEY_DATA_START_INDEX + (PAC_STANLEY_BYTE_LENGTH * idx),
            8));
        asciiCardId[idx] = byte & 0x7F; // discard the parity bit
    }
    hex_chars_to_uint8((char*)asciiCardId, protocol->data);
}
```

`hex_chars_to_uint8()` (`lib/toolbox/hex.c`, bei mir ebenfalls unverändert) kennt keine
Längenangabe, sondern läuft, bis es auf ein ungültiges Hex-Zeichen oder ein `\0`-Byte trifft:

```c
bool hex_chars_to_uint8(const char* value_str, uint8_t* value) {
    ...
    while(*value_str && value_str[1]) {
        parse_success = hex_char_to_uint8(*value_str, value_str[1], value++);
        if(!parse_success) break;
        value_str += 2;
    }
    return parse_success;
}
```

`asciiCardId[8]` ist genau 8 Byte groß und **nicht** NUL-terminiert. Nach den 8 gültigen
7-Bit-ASCII-Hex-Ziffern (`byte & 0x7F`, jede von den geparsten Wiegand-Bits abgeleitet, also
verlässlich ein Druckzeichen ≠ 0) folgt im Stack-Frame nicht kontrollierter Speicher. Landen
dort zufällig zwei weitere Bytes, die als Hex-Ziffernpaar durchgehen, liest die Schleife über
`asciiCardId[]` hinaus **und** schreibt das daraus resultierende fünfte Byte über
`protocol->data[PAC_STANLEY_DECODED_DATA_SIZE]` hinaus (`PAC_STANLEY_DECODED_DATA_SIZE` = 4,
`protocol->data` ist exakt 4 Byte groß). Das ist kein rein theoretischer Angriffs-Pfad: der Bug
tritt bei **jedem erfolgreichen Decode** einer PAC/Stanley-Karte auf, abhängig vom zufälligen
Inhalt des Stacks an dieser Stelle — also potenziell auch im normalen Lesebetrieb, nicht nur
bei präparierten Eingaben.

Der Fix ist minimal (PR #4427):

```c
// Fix:
uint8_t asciiCardId[9] = {0};
```

Ein Byte mehr, komplett genullt — Index 8 bleibt `0x00` und terminiert die Hex-Parser-Schleife
zuverlässig nach genau 4 Byte, unabhängig vom Stackinhalt danach. Die Füllschleife (`idx < 8`)
bleibt unverändert, sie schreibt weiterhin nur in `asciiCardId[0..7]`.

## Nutzen
- Schließt einen Lese- **und** Schreib-Overrun im PAC/Stanley-LFRFID-Decoder, der bei jeder
  erfolgreichen Kartenlesung auftreten kann — nicht nur bei einer gezielt präparierten Karte.
  PAC/Stanley ist ein verbreitetes Wiegand-Badge-Format für physische Zutrittskontrolle; das
  Lesen fremder Zutrittsmedien ist bei mir Kernanwendungsfall (ergänzt die bereits vorhandenen
  Access-Audit-/Wiegand-Reader-Funde in diesem Repo um die LFRFID-Seite desselben
  Einsatzgebiets).
- Winzige, rein additive Änderung (ein Byte mehr, Null-Initialisierung) — kein API-Bruch, keine
  Struktur-/Verhaltensänderung für den regulären Fall (die ersten 4 decodierten Bytes bleiben
  identisch).
- Direkt gegen meine mntm-012-Basis anwendbar, da die Datei seit dem mntm-012-Stand in dieser
  Hinsicht unverändert ist.

## Betroffene Dateien (lokal)
Eine Zeile in einer Datei:
- `lib/lfrfid/protocols/protocol_pac_stanley.c` — in `protocol_pac_stanley_decode()`:
  ```c
  uint8_t asciiCardId[8];
  ```
  ersetzen durch
  ```c
  // hex_chars_to_uint8() liest bis zu einem Terminator, den Platz dafür reservieren
  uint8_t asciiCardId[9] = {0};
  ```
  Kein neuer Include, kein neuer State, keine Registry-Änderung nötig.

## Risiko
- Sehr niedrig: die Änderung vergrößert nur den lokalen Stack-Puffer um 1 Byte und nullt ihn
  vor Gebrauch — die Füllschleife (Index 0–7) und der Aufruf von `hex_chars_to_uint8()` sind
  unverändert, das Verhalten für den regulären Fall (gültige 8 Hex-Ziffern → 4 Byte
  `protocol->data`) ist identisch zu vorher.
- Vor dem Übernehmen `PAC_STANLEY_DECODED_DATA_SIZE`- und `protocol->data[]`-Layout in meiner
  mntm-012-Basis gegenlesen (Momentum kann LFRFID-Protokoll-Structs gegenüber Stock/Unleashed
  abweichend definieren) — bei mir identisch zum zitierten Stand, siehe Port-Skizze Schritt 2.
- Der Build (`fbt`/`ufbt`) deckt jede Signatur-/Konstantenabweichung als Compile-Fehler auf.
- Funktionale Prüfung ohne Hardware nur eingeschränkt möglich: die reine Parser-Logik
  (`hex_char_to_hex_nibble`/`hex_char_to_uint8`/`hex_chars_to_uint8`, alle host-kompilierbar,
  keine Flipper-SDK-Abhängigkeit) habe ich mit einem `gcc -fsanitize=address,undefined`-Mini-
  Repro nachgerechnet (siehe Testhinweis) — der eigentliche Bug-Pfad (realer Stack-Frame einer
  echten PAC/Stanley-Kartenlesung) braucht einen echten Test mit einer eigenen PAC/Stanley-Karte
  oder einem eigenen `.rfid`-Dump, lokal, nicht Teil dieses Repos.

## Port-Skizze
1. `lib/lfrfid/protocols/protocol_pac_stanley.c::protocol_pac_stanley_decode()` in meiner
   mntm-012-Basis öffnen.
2. Gegenlesen, ob `PAC_STANLEY_DECODED_DATA_SIZE` weiterhin `4` ist und `protocol->data` weiterhin
   `uint8_t data[PAC_STANLEY_DECODED_DATA_SIZE]` — falls Momentum das Struct-Layout geändert hat,
   Fix entsprechend anpassen statt blind zu übernehmen.
3. `uint8_t asciiCardId[8];` durch `uint8_t asciiCardId[9] = {0};` ersetzen (Kommentar wie oben
   optional).
4. Host-seitiges Mini-Repro (`repro.c`, unten) mit `gcc -O2 -Wall -Wextra
   -fsanitize=address,undefined` bauen und laufen lassen, um die Terminierung nach genau 4 Byte
   zu bestätigen — bereits von mir lokal ausgeführt, siehe Testhinweis.
5. Build: `ufbt` (bzw. `fbt firmware_all`).
6. Funktionstest lokal: eine eigene PAC/Stanley-Karte (oder ein eigener `.rfid`-Dump mit
   PAC/Stanley-Encoding) mehrfach hintereinander lesen/decodieren (idealerweise mit
   ASan-Build auf einem Host-Test der reinen Protokoll-Logik, falls im lokalen Test-Setup
   vorhanden) und prüfen, dass die decodierte Card-ID vor/nach dem Fix identisch bleibt.

## Testhinweis (lokal nachvollzogen)
Host-kompilierbares Mini-Repro der reinen Parser-Kette (`hex_char_to_hex_nibble`,
`hex_char_to_uint8`, `hex_chars_to_uint8` — 1:1 aus `lib/toolbox/hex.c` übernommen, keine
Flipper-SDK-Abhängigkeit) unter AddressSanitizer/UBSan gebaut und ausgeführt
(`gcc -O2 -Wall -Wextra -fsanitize=address,undefined`):

```
== 1) Lese-Overrun: asciiCardId[8] ohne Terminator (ALT) ==
data[0..5] = 12 34 56 78 AA 00
  (gelesen bis mindestens Index 9 — über die deklarierten 8 Byte hinaus)

== 2) Schreib-Overrun: Ziel data[4] wird ueberschrieben (ALT) ==
data[0..3] (deklarierter Bereich) = 12 34 56 78
data[4..5] (ausserhalb) = AA BB
-> BESTAETIGT: Schreibzugriff ueber PAC_STANLEY_DECODED_DATA_SIZE (4 Byte) hinaus

== 3) Gleiche Stackmuell-Situation, NEU (asciiCardId[9] = {0}) ==
data[0..3] = 12 34 56 78
data[4..5] = 00 00 -- bleibt 0x00, Schleife stoppt zuverlaessig bei Index 8
```

Aufbau: `asciiCardId`/`data` stehen jeweils am Anfang eines größeren, kontrollierten
Backing-Buffers (statt eines echten, aber im Testprogramm undefinierten Overflows), der den
Rest des realen Stack-Frames simuliert — dieselbe Codepfad-Logik wie im echten Decoder, aber
sicher beobachtbar. Ergebnis bestätigt: alte Version liest/schreibt nachweislich über die
deklarierten Array-Grenzen hinaus, sobald "Stackmüll" wie zwei gültige Hex-Ziffern danach
steht; neue Version tut das in derselben Situation nicht.

## Quelle
- `flipperdevices/flipperzero-firmware`, `dev`-Branch, PR #4427 (@munzzyy, gemerged
  27.07.2026): „LFRFID: NUL-terminate the PAC/Stanley card id before parsing it" —
  `lib/lfrfid/protocols/protocol_pac_stanley.c`.
- Momentum-Firmware, Tag `mntm-012`: `lib/lfrfid/protocols/protocol_pac_stanley.c` und
  `lib/toolbox/hex.c` als Vergleichsbasis (Fix dort noch nicht vorhanden, Code identisch zum
  hier zitierten Vorher-Stand).
- Durchgesehener Commit-Bereich auf `dev` seit dem letzten geprüften Stand (PR #4369,
  `0dd3681f`) bis `9148d50c` (2026-08-04): 21 Commits, davon als sicherheits-/
  speicherrelevant eingestuft und für künftige Sitzungen vorgemerkt (noch nicht einzeln
  geprüft/portiert): `2fe56050` (NFC iso15693-Emulation verwirft Read-Multiple-Blocks bei
  Tags >64 Blöcke — analog zum bereits portierten MFUL-FAST_READ-Muster, gleiche Kategorie
  „eigene Emulation gegen fremden Reader"), `c9c77f66` (NFC FeliCa: Poller-Fehler vor
  System-Code-Response-Read prüfen), `5dc71870` (Infrared: `timings_cnt` bei Decoder-Alloc
  uninitialisiert plus fehlerhafte Bounds-Prüfung), `8713d1c4` (`bit_lib`: Lesen über Puffer
  hinaus bei Bit-Zugriffen innerhalb eines Bytes), `9c173b46` (LFRFID: Pyramid-Trailing-
  Wiegand-Parity-Bit-Fix).
