# LFRFID: Pyramid-Wiegand-Paritätsbit liest hinter dem Quellpuffer — Official-dev vs. mntm-012

## Was
Ich habe den in mehreren vorherigen Sitzungen aus dem `dev`-Durchlauf von
`flipperdevices/flipperzero-firmware` vorgemerkten, aber noch nicht einzeln geprüften Commit
`9c173b46` (PR #4426, @munzzyy, gemerged 27.07.2026) untersucht. Er behebt einen Bug im
**Pyramid-LFRFID-Encoder** (`lib/lfrfid/protocols/protocol_pyramid.c`), der bei mir in der
mntm-012-Basis unverändert vorhanden ist (gegengeprüft gegen Tag `mntm-012`, Datei identisch
zum hier zitierten Vorher-Stand):

```c
// mntm-012 (fehlerhaft):
bool protocol_pyramid_get_parity(const uint8_t* bits, uint8_t type, int length) {
    int x;
    for(x = 0; length > 0; --length)
        x += bit_lib_get_bit(bits, length - 1);
    x %= 2;
    return x ^ type;
}

void protocol_pyramid_add_wiegand_parity(
    uint8_t* target, uint8_t target_position, uint8_t* source, uint8_t length) {
    bit_lib_set_bit(
        target, target_position, protocol_pyramid_get_parity(source, 0 /* even */, length / 2));
    bit_lib_copy_bits(target, target_position + 1, length, source, 0);
    bit_lib_set_bit(
        target,
        target_position + length + 1,
        protocol_pyramid_get_parity(source + length / 2, 1 /* odd */, length / 2));
}
```

Aufgerufen wird das aus `protocol_pyramid_encode()` mit `length = 24` und `source = wiegand`,
wobei `wiegand` ein lokales `uint8_t wiegand[3]` ist (24 Bit: 8 Bit Facility Code + 16 Bit
Card-Nummer). `length / 2` ist als **Bit-Offset** gemeint (die erste Hälfte der 24 Bit für die
even-, die zweite Hälfte für die odd-Parität), aber `source + length / 2` ist normale
Pointer-Arithmetik auf einem `uint8_t*` — verschiebt den Zeiger also um `length / 2 = 12`
**Byte**, nicht um 12 Bit. `wiegand[]` ist nur 3 Byte groß; der zweite Aufruf liest damit ab
9 Byte hinter dem Ende des Puffers, aus nicht dazugehörigem Stack-Speicher.

Zwei getrennte, unabhängig nachweisbare Folgen:
1. **Falsches Paritätsbit für reale Kartendaten.** Das odd-Paritätsbit im gesendeten/
   geschriebenen 26-Bit-Wiegand-Frame hängt gar nicht mehr von den echten Bits 12–23 der
   Kartendaten ab, sondern von zufälligem Speicherinhalt hinter `wiegand[]`. Ein Reader, der
   26-Bit-Wiegand-Parität validiert (Standardverhalten bei physischer Zutrittskontrolle),
   weist eine damit programmierte/gesendete Karte für ungefähr die Hälfte aller Facility-
   Code/Card-Nummer-Kombinationen ab (siehe Testhinweis, 50,0 % über den vollen 8-Bit-FC- x
   16-Bit-Card-Raum bei fester Speicherfüllung). Der eigene Decoder (`protocol_pyramid_decode`)
   prüft nur CRC-8 und Formatlänge, nicht die Wiegand-Parität selbst — beim Zurücklesen einer
   selbst geschriebenen Karte mit dem Flipper fällt der Fehler daher nicht auf.
2. **Echter Speicherzugriffsfehler.** Es handelt sich nicht nur um einen Logikfehler mit
   zufällig falschem Ergebnis, sondern um einen tatsächlichen Lesezugriff außerhalb der
   3-Byte-Allokation von `wiegand[]` — mit Host-Repro unter AddressSanitizer als
   `heap-buffer-overflow READ ... 10 bytes after 3-byte region` reproduziert (exakter
   Nachweis, dass `source + length / 2` als Byte- statt Bit-Offset wirkt: 12 Byte Verschiebung
   minus 3 Byte Puffergröße plus 1 Byte für den innerhalb der Funktion gelesenen letzten Bit-
   Index ergibt den beobachteten Versatz von 10 Byte). Auf dem echten Ziel (STM32WB55, `source`
   ein lokales Array im Stack-Frame von `protocol_pyramid_encode()`) ist das kein Absturz
   (kein Bounds-Checking auf dem Zielsystem), sondern liest lesbaren Stack-Inhalt benachbarter
   Variablen — mit dem Effekt aus Punkt 1.

Der Fix (PR #4426) gibt `protocol_pyramid_get_parity()` einen expliziten Bit-Offset-Parameter
statt Pointer-Arithmetik mit einer Bit-Zahl:

```c
// Fix:
bool protocol_pyramid_get_parity(const uint8_t* bits, size_t position, uint8_t type, int length) {
    int x;
    for(x = 0; length > 0; --length)
        x += bit_lib_get_bit(bits, position + length - 1);
    x %= 2;
    return x ^ type;
}

void protocol_pyramid_add_wiegand_parity(
    uint8_t* target, uint8_t target_position, uint8_t* source, uint8_t length) {
    bit_lib_set_bit(
        target, target_position, protocol_pyramid_get_parity(source, 0, 0 /* even */, length / 2));
    bit_lib_copy_bits(target, target_position + 1, length, source, 0);
    bit_lib_set_bit(
        target,
        target_position + length + 1,
        protocol_pyramid_get_parity(source, length / 2, 1 /* odd */, length / 2));
}
```

Der even-Aufruf (Bits 0–11) war schon vorher korrekt und bleibt unverändert (`position = 0`);
nur der odd-Aufruf liest jetzt korrekt Bits 12–23 desselben `source`-Puffers statt aus einem um
12 Byte verschobenen Pointer.

## Nutzen
- Behebt einen Speicherzugriffsfehler (Lesen außerhalb einer 3-Byte-Stack-Allokation) im
  Pyramid-LFRFID-Encoder — betrifft jeden Schreib-/Sendevorgang einer Pyramid-Karte
  (`protocol_pyramid_write_data()` für T5577-Beschreibung sowie den Emulations-/Sendepfad über
  `protocol_pyramid_encoder_yield()`), nicht nur einen theoretischen Randfall.
- Macht das gesendete/geschriebene 26-Bit-Wiegand-Signal für Pyramid/Farpointe-Karten
  tatsächlich paritätskorrekt — ohne den Fix werden von mir geschriebene/emulierte Pyramid-
  Karten von jedem paritätsprüfenden Reader für rund die Hälfte aller Facility-Code/Card-
  Nummer-Kombinationen abgelehnt, ohne dass der Fehler beim eigenen Rücklesen auffällt (siehe
  oben). Für autorisierte Zutrittskontroll-Tests (Badge-Kloning/-Emulation der eigenen bzw.
  freigegebenen Medien) ist ein korrektes, vom Reader akzeptiertes Signal die Grundvoraussetzung
  — ergänzt damit die bereits vorhandenen LFRFID-Funde (PAC/Stanley-NUL-Terminator,
  Wiegand-Reader-App, Access-Audit-App) um einen Encoder-seitigen Korrektheitsfix.
- Kleine, lokal begrenzte Änderung: neue Parameter-Signatur nur für eine intern genutzte
  Hilfsfunktion (`protocol_pyramid_get_parity`, in derselben Datei deklariert und nur dort
  aufgerufen — kein Header-/API-Bruch), kein neuer State, keine neuen Abhängigkeiten.

## Betroffene Dateien (lokal)
Eine Datei, zwei zusammengehörige Funktionsänderungen:
- `lib/lfrfid/protocols/protocol_pyramid.c`:
  - `protocol_pyramid_get_parity()`: neuer Parameter `size_t position` nach `bits`; die Schleife
    liest `bit_lib_get_bit(bits, position + length - 1)` statt `bit_lib_get_bit(bits, length - 1)`.
  - `protocol_pyramid_add_wiegand_parity()`: beide Aufrufe von `protocol_pyramid_get_parity()`
    bekommen den zusätzlichen Positionsparameter (`0` beim even-, `length / 2` beim
    odd-Aufruf), der odd-Aufruf verwendet dabei `source` statt `source + length / 2` als
    Pointer-Argument.

## Risiko
- Sehr niedrig: reine Signatur-/Aufrufänderung einer intern genutzten Hilfsfunktion, keine
  Änderung an Puffergrößen, State-Maschinen oder öffentlicher API. Der even-Aufruf (Bits 0–11)
  ist unverändert; nur der odd-Aufruf liest jetzt aus dem richtigen Bereich desselben,
  ohnehin schon vorhandenen Puffers statt aus fremdem Speicher.
- Verhaltensänderung ist beabsichtigt und sichtbar: das gesendete/geschriebene odd-Paritätsbit
  ändert sich gegenüber dem fehlerhaften Vorher-Stand für praktisch jede Karte (siehe
  Testhinweis) — das ist der Zweck des Fixes, nicht ein Nebeneffekt. Bereits mit der
  fehlerhaften Version geschriebene T5577-Tags haben ein falsches Paritätsbit und sollten nach
  dem Fix neu beschrieben werden, falls sie an einem paritätsprüfenden Reader verwendet werden
  sollen.
- Vor dem Übernehmen `protocol_pyramid_get_parity()`/`protocol_pyramid_add_wiegand_parity()`-
  Signaturen in meiner mntm-012-Basis gegenlesen, falls Momentum das Pyramid-Protokoll
  gegenüber Stock abweichend pflegt — bei mir identisch zum zitierten Vorher-Stand. Der Build
  (`fbt`/`ufbt`) deckt jede Signaturabweichung als Compile-Fehler auf, da beide Funktionen nur
  innerhalb derselben Datei verwendet werden.
- Funktionale Prüfung ohne Hardware nur eingeschränkt möglich: die reine Paritäts-/
  Bit-Adressierungslogik (host-kompilierbar, keine Flipper-SDK-Abhängigkeit außer den drei
  nachgebauten `bit_lib`-Grundfunktionen) habe ich mit einem Mini-Repro nachgerechnet (siehe
  Testhinweis); der vollständige Encode/Decode-Pfad inkl. FSK-Modulation und T5577-Schreiben
  braucht einen echten Test mit einem eigenen beschreibbaren T5577-Tag oder eigener
  Pyramid/Farpointe-Testkarte, lokal, nicht Teil dieses Repos.

## Port-Skizze
1. `lib/lfrfid/protocols/protocol_pyramid.c` in meiner mntm-012-Basis öffnen.
2. `protocol_pyramid_get_parity()` um den Parameter `size_t position` erweitern (nach `bits`,
   vor `type`) und die Schleife auf `bit_lib_get_bit(bits, position + length - 1)` umstellen.
3. In `protocol_pyramid_add_wiegand_parity()`:
   - even-Aufruf: `protocol_pyramid_get_parity(source, 0, 0 /* even */, length / 2)`.
   - odd-Aufruf: `protocol_pyramid_get_parity(source, length / 2, 1 /* odd */, length / 2)`
     (Pointer bleibt `source`, nicht mehr `source + length / 2`).
4. Host-seitiges Mini-Repro (`repro.c`, unten) bauen und laufen lassen, um Korrektheit der
   neuen Variante und den Speicherzugriffsfehler der alten Variante zu bestätigen — bereits von
   mir lokal ausgeführt, siehe Testhinweis.
5. Build: `ufbt` (bzw. `fbt firmware_all`).
6. Funktionstest lokal: eine Pyramid/Farpointe-26-Bit-Wiegand-Karte auf einen eigenen T5577-Tag
   schreiben (`protocol_pyramid_write_data()`-Pfad) und an einem paritätsprüfenden Wiegand-
   Reader (bzw. mit einer Logic-Analyzer-/Wiegand-Sniffer-Aufzeichnung der geschriebenen Bits)
   verifizieren, dass das odd-Paritätsbit jetzt konsistent mit Bits 12–23 der programmierten
   Facility-Code/Card-Nummer-Kombination ist. Alternativ Emulation (`protocol_pyramid_encoder_yield()`)
   gegen einen eigenen Reader testen.

## Testhinweis (lokal nachvollzogen)
Host-kompilierbares Mini-Repro (`repro.c`, drei Baumodi) — baut nur die betroffenen
`bit_lib`-Grundfunktionen (`bit_lib_get_bit`/`bit_lib_set_bit`/`bit_lib_copy_bits`, 1:1 aus
`lib/bit_lib/bit_lib.c`) sowie ALT/NEU von `protocol_pyramid_get_parity()`/
`protocol_pyramid_add_wiegand_parity()` nach, kein Firmware-Build nötig:

**Standardbuild (nur NEU, Korrektheit über vollen FC-Raum x CardNum-Stichprobe gegen eine
unabhängige Referenzimplementierung):**
```
gcc -O0 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro
```
```
== NEU: Korrektheitspruefung ueber vollen FC-Raum x CardNum-Stichprobe ==
Abweichungen von der Referenz-Paritaet: 0/65536
PASS: NEU liefert in allen 65536 Faellen das korrekte odd-Paritaetsbit.
```

**`-DDEMO_OLD_STATS` (ALT auf einem 15-Byte-Puffer mit bekanntem Füllmuster hinter `wiegand[3]`,
kein ASan nötig — quantifiziert die Falschrate):**
```
gcc -O0 -Wall -Wextra -DDEMO_OLD_STATS -o repro_stats repro.c && ./repro_stats
```
```
== ALT, wiegand[3] + 12 Byte Fuellmuster dahinter (kein ASan, Statistik) ==
Odd-Paritaetsbit falsch ggue. den echten Kartendaten (Bits 12..23 von wiegand[]): 32768/65536 (50.0%)
```

**`-DDEMO_OLD_BUG` (ALT auf einem exakt 3 Byte großen Heap-Puffer, wie `wiegand[3]` im echten
Aufrufer — belegt den Speicherzugriffsfehler direkt, nicht nur ein falsches Ergebnis):**
```
gcc -O0 -Wall -Wextra -fsanitize=address,undefined -DDEMO_OLD_BUG -o repro_bug repro.c && ./repro_bug
```
```
==ERROR: AddressSanitizer: heap-buffer-overflow on address ...
READ of size 1 at ... thread T0
    #0 ... in bit_lib_get_bit
    #1 ... in old_get_parity
    #2 ... in old_add_wiegand_parity
    #3 ... in main
0x... is located 10 bytes after 3-byte region [...,...)
allocated by thread T0 here:
    #0 ... in malloc
    #1 ... in main
SUMMARY: AddressSanitizer: heap-buffer-overflow ... in bit_lib_get_bit
==ABORTING
```

Ergebnis bestätigt beide Teile des Bugs unabhängig voneinander: die alte Variante liest
nachweislich außerhalb der 3-Byte-Allokation von `wiegand[]` (ASan-Abbruch, exakt 10 Byte
dahinter — passend zur berechneten Verschiebung), und liefert dabei für rund die Hälfte aller
Facility-Code/Card-Nummer-Kombinationen ein vom echten Karteninhalt abweichendes Paritätsbit;
die neue Variante liefert in allen 65536 getesteten Kombinationen exakt das erwartete Bit.

## Quelle
- `flipperdevices/flipperzero-firmware`, `dev`-Branch, Commit `9c173b46` / PR #4426
  (@munzzyy, gemerged 27.07.2026): „LFRFID: fix the Pyramid trailing Wiegand parity bit" —
  `lib/lfrfid/protocols/protocol_pyramid.c`.
- Momentum-Firmware, Tag `mntm-012`: `lib/lfrfid/protocols/protocol_pyramid.c` und
  `lib/bit_lib/bit_lib.c` als Vergleichsbasis (Fix dort noch nicht vorhanden, Code identisch
  zum hier zitierten Vorher-Stand).
- Dieser Commit war bereits in der Sitzung vom 2026-08-13 00:26 (Fund zu `8713d1c4`,
  `bit_lib_get_bits()`) als offener Folgepunkt vorgemerkt, zusammen mit `c9c77f66` (NFC FeliCa
  Poller-Fehlerprüfung) und `5dc71870` (Infrared `timings_cnt`-Init/Bounds-Fix) — beide noch
  offen für künftige Sitzungen. Danach weiter ab `9148d50c` (2026-08-04) auf `dev` gegenprüfen.
