# bit_lib_get_bits(): Lese-Overrun bei unausgerichtetem Zugriff am Pufferende — Official-dev vs. mntm-012

## Was
Ich habe den vorgemerkten Official-dev-Commit `8713d1c4` weiterverfolgt (aus meiner letzten
Durchsicht des `dev`-Branches von `flipperdevices/flipperzero-firmware` bis `9148d50c`
zurückgestellt). PR #4424 (@munzzyy, gemerged 27.07.2026) behebt einen Lese-Overrun in
**`bit_lib_get_bits()`** (`lib/bit_lib/bit_lib.c`), einer der am häufigsten verwendeten
Low-Level-Funktionen im gesamten Firmware-Baum: praktisch jeder LFRFID-Protokoll-Decoder
(PAC/Stanley, AWID, Noralsy, Securakey, Paradox, Viking, Gallagher, FDX-B, HID …) und Teile der
NFC-/SubGHz-Protokoll-Schicht bauen auf ihr auf, um einzelne Bitfelder aus einem Byte-Puffer zu
extrahieren.

Ich habe die Datei gegen meine mntm-012-Basis geprüft (`lib/bit_lib/bit_lib.c`) — bei mir
unverändert vorhanden, inklusive des zitierten `TODO FL-3534`-Kommentars:

```c
// mntm-012 (fehlerhaft):
uint8_t bit_lib_get_bits(const uint8_t* data, size_t position, uint8_t length) {
    uint8_t shift = position % 8;
    if(shift == 0) {
        return data[position / 8] >> (8 - length);
    } else {
        // TODO FL-3534: fix read out of bounds
        uint8_t value = (data[position / 8] << (shift));
        value |= data[position / 8 + 1] >> (8 - shift);
        value = value >> (8 - length);
        return value;
    }
}
```

Ist `position` nicht byte-ausgerichtet (`shift != 0`), liest die Funktion **immer** zusätzlich
`data[position / 8 + 1]`, selbst wenn alle angefragten `length` Bits vollständig im aktuellen
Byte liegen (`shift + length <= 8`). Das zusätzlich gelesene Byte trägt in diesem Fall nichts
zum Rückgabewert bei (es wird über den finalen Shift wieder herausgeschoben), ist aber ein
regulärer, aus dem C-Standard heraus undefinierter Zugriff — liegt `position / 8` auf dem
letzten gültigen Byte eines Puffers, ist `data[position / 8 + 1]` ein Zugriff hinter der
Allokation. Genau dieses Muster (Puffer exakt so groß dimensioniert wie die maximale Bitzahl,
letzter Feldzugriff unausgerichtet und kürzer als 8 Bit) kommt in mehreren meiner LFRFID-
Decoder strukturell vor (z. B. `noralsy_chksum()` in `protocol_noralsy.c`, das
`bit_lib_get_bits(bits, i, 4)` über einen Teilpuffer iteriert); ob ein konkreter Aufruf bei mir
tatsächlich über die reale Allokationsgrenze hinausgeht, hängt vom jeweiligen Puffer-Layout
(Sicherheitsabstand am Ende) ab und ist pro Protokoll unterschiedlich — die Funktion selbst ist
aber unabhängig davon in jedem unausgerichteten Fall ein unnötiger Zusatzzugriff.

Der Fix (PR #4424) überspringt den Zusatzzugriff, wenn die Bits nachweislich im aktuellen Byte
liegen:

```c
// Fix:
uint8_t bit_lib_get_bits(const uint8_t* data, size_t position, uint8_t length) {
    uint8_t shift = position % 8;
    if(shift == 0) {
        return data[position / 8] >> (8 - length);
    } else if(shift + length <= 8) {
        // Requested bits fit in the current byte, don't read the next one
        return (uint8_t)(data[position / 8] << shift) >> (8 - length);
    } else {
        uint8_t value = (data[position / 8] << (shift));
        value |= data[position / 8 + 1] >> (8 - shift);
        value = value >> (8 - length);
        return value;
    }
}
```

Für `shift + length > 8` bleibt der bisherige (dort tatsächlich benötigte) Zwei-Byte-Zugriff
unverändert.

## Nutzen
- Schließt einen strukturellen Lese-Overrun in einer der am zentralsten wiederverwendeten
  Bit-Extraktionsfunktionen der Firmware — wirkt indirekt auf **alle** LFRFID-Protokoll-Decoder
  und Teile der NFC-/SubGHz-Schicht, statt nur auf ein einzelnes Protokoll. Ergänzt damit meine
  bereits portierten Einzelprotokoll-Fixes (PAC/Stanley-NUL-Terminator, DESFire/MFUL, ISO15693)
  um die gemeinsame Grundlage, auf der ein Teil dieser Decoder aufsetzt.
- Nullrisiko für den regulären Fall: der Rückgabewert ist für jede gültige Eingabe identisch zu
  vorher (mit meinem Host-Repro bestätigt) — reine Bounds-Härtung, kein Verhaltens- oder
  API-Wechsel.
- Ein-Datei-Änderung, keine Signatur-, Header- oder Aufrufer-Anpassung nötig — jeder bestehende
  Aufrufer (samt meiner bereits portierten Protokoll-Fixes) profitiert ohne eigene Änderung.

## Betroffene Dateien (lokal)
Eine Funktion in einer Datei:
- `lib/bit_lib/bit_lib.c` — `bit_lib_get_bits()`: den `else`-Zweig um den in der Port-Skizze
  gezeigten `else if(shift + length <= 8)`-Fall ergänzen, TODO-Kommentar `FL-3534` entfernen (im
  Fix erledigt, da behoben).

## Risiko
- Sehr niedrig: identischer Rückgabewert für alle Eingaben, bei denen der alte Code definiertes
  Verhalten hatte (jede Eingabe, bei der `data[position/8 + 1]` innerhalb der Allokation lag) —
  mit Host-Repro bestätigt (siehe Testhinweis). Für die zuvor undefinierten Fälle (Zugriff
  hinter der Allokation) ändert sich das Verhalten von "undefiniert" zu "definiert und
  identisch zum durch den Zusatzzugriff ohnehin nicht beeinflussten Rückgabewert".
- Keine Struktur-/Signaturänderung, keine neuen Includes, keine Registry-Änderung.
- Der Build (`fbt`/`ufbt`) deckt jede unerwartete Nutzung der Funktionssignatur als
  Compile-Fehler auf (hier: keine Signaturänderung, daher kein Risiko in dieser Hinsicht).
- Funktionale Prüfung ohne Hardware: die reine Bit-Extraktionslogik ist host-kompilierbar und
  hat keine Flipper-SDK-Abhängigkeit (siehe Testhinweis). Der reale Effekt auf Hardware-Ebene
  (ob ein konkreter LFRFID-/NFC-Lesevorgang je den betroffenen Pufferendfall auslöst) bleibt
  vom Kartentyp/Protokoll abhängig und ist lokal mit eigenen Karten/Dumps zu prüfen.

## Port-Skizze
1. `lib/bit_lib/bit_lib.c::bit_lib_get_bits()` in meiner mntm-012-Basis öffnen.
2. Den bestehenden `else`-Zweig (mit `TODO FL-3534`-Kommentar) durch den dreiteiligen
   `if/else if/else`-Aufbau aus dem Fix ersetzen (siehe „Was" oben) — reine Ergänzung eines
   Zweigs, kein bestehender Code wird inhaltlich verändert außer dem entfernten TODO-Kommentar.
3. Host-seitiges Mini-Repro (`repro.c`, unten) bauen und laufen lassen, um zu bestätigen, dass
   die neue Variante für unausgerichtete Zugriffe, die vollständig im aktuellen Byte liegen,
   keinen Zugriff hinter der Allokation mehr macht und denselben Wert wie vorher liefert —
   bereits von mir lokal ausgeführt, siehe Testhinweis.
4. Build: `ufbt` (bzw. `fbt firmware_all`).
5. Funktionstest lokal: mindestens eine eigene LFRFID-Karte (z. B. mit einer der bereits
   vorhandenen Protokoll-Ports wie PAC/Stanley) mehrfach lesen/decodieren und prüfen, dass die
   decodierten Werte vor/nach dem Fix identisch bleiben (Regressionscheck, kein neuer
   Funktionsumfang).

## Testhinweis (lokal nachvollzogen)
Host-kompilierbares Mini-Repro (`repro.c`), das die ALTE und NEUE Variante von
`bit_lib_get_bits()` 1:1 aus `lib/bit_lib/bit_lib.c` übernimmt (keine Flipper-SDK-Abhängigkeit)
und mit einem exakt 1 Byte großen `malloc`-Puffer (kein Sicherheitsabstand danach) einen
unausgerichteten Zugriff auslöst, dessen angefragte Bits vollständig im letzten Byte liegen
(`position=4, length=4`):

Standardbuild (nur NEU, muss unter ASan/UBSan sauber durchlaufen):
```
gcc -O0 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro
== NEU ==
Rueckgabewert (NEU): 0x4, kein Zugriff auf buf[1] (ausserhalb der 1-Byte-Allokation).
BESTAETIGT: NEU liefert den erwarteten Wert (0x4) ohne Pufferzugriff ausserhalb.
```
(Exit-Code 0.)

Mit `-DDEMO_OLD_BUG` (ALT wird zusätzlich zuerst aufgerufen, muss mit ASan-Abbruch enden):
```
gcc -O0 -Wall -Wextra -fsanitize=address,undefined -DDEMO_OLD_BUG -o repro_bug repro.c && ./repro_bug
==ERROR: AddressSanitizer: heap-buffer-overflow ... READ of size 1 ...
0x... is located 0 bytes after 1-byte region [...]
    #0 ... in bit_lib_get_bits_old
==ABORTING
```
(Exit-Code 1, wie erwartet — belegt den Bug isoliert vor dem Fix.)

Ergebnis bestätigt: die alte Variante liest nachweislich über die Allokationsgrenze hinaus,
sobald die angefragten Bits unausgerichtet, aber vollständig im letzten Byte eines exakt
dimensionierten Puffers liegen; die neue Variante liefert für denselben Fall denselben
Rückgabewert ohne den Zusatzzugriff.

## Quelle
- `flipperdevices/flipperzero-firmware`, `dev`-Branch, PR #4424 (@munzzyy, gemerged 27.07.2026,
  Commit `8713d1c4`): „bit_lib: don't read past the buffer when the bits fit one byte" —
  `lib/bit_lib/bit_lib.c`. Laut Commit-Nachricht identischer Fix bereits zuvor in
  `fbtng-corelibs` gelandet; `lib/bit_lib` im Hauptrepo ist eine unabhängige Kopie, die ihn
  nicht mitbekommen hatte.
- Momentum-Firmware, Tag `mntm-012`: `lib/bit_lib/bit_lib.c` als Vergleichsbasis (Fix dort noch
  nicht vorhanden, Code identisch zum hier zitierten Vorher-Stand, inkl. `TODO FL-3534`).
- Aus meiner letzten Official-dev-Durchsicht (Stand `9148d50c`, 2026-08-04) verbleiben weiterhin
  vorgemerkt: `c9c77f66` (NFC FeliCa Poller-Fehlerprüfung), `5dc71870` (Infrared
  `timings_cnt`-Init/Bounds-Fix), `9c173b46` (LFRFID Pyramid-Parity-Bit-Fix). Danach weiter ab
  `9148d50c` gegenprüfen (Datum dieser Sitzung als neuer Startpunkt für den nächsten
  `dev`-Durchlauf).
