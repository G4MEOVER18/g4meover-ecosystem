# FDX-B: vier Data-Block-Unterfelder werden ohne Bit-Reversal und mit falschen Feldgrenzen gezeigt — Unleashed vs. mntm-012

Quelle des Fundes: beim systematischen Diff aller `lib/lfrfid/protocols/*.c`-Dateien gegen
`DarkFlippers/unleashed-firmware` (`dev`) zum Auffinden eines neuen Firmware-Feature-Mining-
Kandidaten nach Abschluss der Texkom- und SubGhzBlockGenericGlobal-Portreihen (letzte beiden
Sitzungen). Dabei zunächst `protocol_pyramid_get_parity()` erneut aufgefallen — bereits als
`lfrfid-pyramid-wiegand-parity-oob-read-fix` dokumentiert, hier verworfen. Beim Weitersuchen in
`protocol_fdx_b.c` (größter Diff aller LFRFID-Protokolldateien, 180 Zeilen) zwei unabhängige
Änderungsblöcke gefunden: einen großen, hier bewusst nicht portierten Periodendecoder für
schwache/driftende Implantat-Chips (siehe "Nicht Teil dieses Ports" unten), und einen davon
unabhängigen, in einem eigenen Commit isolierten Anzeige-Fix in
`protocol_fdx_b_render_data()` — dieser Fund.

## Was

`protocol_fdx_b_render_data()` liest nach den beiden Einzelbits `block_status`/`rudi_bit` vier
weitere Unterfelder aus den letzten 16 Bit des 64-Bit-Datenblocks (Bit 48–63) und zeigt sie zur
reinen Information an (keine Prüfsumme, keine Schreib-/Sendefunktion betroffen). Bei mir
(mntm-012), unverändert seit der allerersten LFRFID-Fassung:

```c
// bei mir (mntm-012) so:
uint8_t reserved = bit_lib_get_bits(protocol->data, 50, 5);
uint8_t user_info = bit_lib_get_bits(protocol->data, 55, 5);
uint8_t replacement_number = bit_lib_get_bits(protocol->data, 60, 3);
```

Das ist aus zwei Gründen falsch:

1. **Falsche Feldgrenzen.** Die tatsächliche ISO-11784-Datenblockstruktur in diesen 13 Bit (nach
   den beiden Einzelbits, vor `animal_flag`) ist 3+2+5+3 Bit: ein eigenständiges 3-Bit
   "Visual Start Digit"-Feld, danach erst 2 Bit tatsächliches RFU/Reserved, dann 5 Bit User Info,
   dann 3 Bit Replacement Number. Meine Fassung kennt "Visual Start Digit" als eigenes Feld gar
   nicht — die ersten drei seiner Bits werden stillschweigend in ein 5 Bit breites "reserved"
   hineingelesen, das eigentlich nur 2 Bit breit sein dürfte.
2. **Falsche Bitreihenfolge.** Alle vier Felder werden bei mir MSB-zuerst gelesen
   (`bit_lib_get_bits()` liest das zuerst gelesene Bit als höchstwertiges Bit des Ergebnisses).
   Das tatsächliche Format dieser vier Unterfelder ist LSB-zuerst — jedes Feld muss nach dem
   Auslesen innerhalb seiner eigenen Breite bitweise gespiegelt werden.

Beide Effekte zusammen bedeuten: die drei angezeigten Werte (`reserved`, `user_info`,
`replacement_number`) sind für praktisch jede Karte, die diese optionalen Zusatzfelder überhaupt
belegt, falsch — und das eigentlich vorhandene "Visual Start Digit"-Feld wird nirgends angezeigt,
weil es bei mir als eigenständiges Feld schlicht nicht existiert (siehe Host-Repro, Szenario D).
Reiner Anzeige-/Auswertungsfehler beim Lesen einer fremden Karte — keine Auswirkung auf Schreiben,
Senden oder die ID-/Länder-/Temperaturfelder (`national_code`, `country_code`, Temperatur), die
alle unverändert bleiben.

Die neue Fassung (`bit_lib_reverse_8_fast(bit_lib_get_bits(...) << shift)`) behebt beides in einem
Zug: das Muster extrahiert zunächst die richtige Bitbreite an der richtigen Position, schiebt sie
an den oberen Rand eines Bytes und spiegelt danach das gesamte Byte — die ungenutzten Nullbits
wandern dabei an den unteren Rand, das Ergebnis ist das korrekt gespiegelte Feld, unverändert zur
bestehenden, auch sonst in `bit_lib` verwendeten Technik (siehe z. B. `bit_lib_reverse_16_fast()`
in `lfrfid_write_fill_em4305()`, Unleashed, nicht Teil dieses Fundes).

Zusätzlich wird die Textausgabe von einer einzigen `"Bits: %hhX-%hhX-%hhX-%hhX-%hhX"`-Zeile auf
fünf benannte Zeilen umgestellt (`Visual Start Digit`, `Replacement Number`, `User Info`,
`Data Block` statt `block_status` als rohes `%hhX` einer `bool`, `RUDI Bit`, `RFU` statt
`reserved`) — macht die ohnehin korrigierten Werte für mich am Gerät lesbar, statt sie nur als
nackte Hexziffern ohne Beschriftung aufzulisten.

## Nutzen

- Betrifft jede FDX-B-Tier-/Implantatkarte, die die optionalen Zusatzfelder (Visual Start Digit,
  User Info, Replacement Number) belegt — bei mir zeigt das Gerät für diese Karten aktuell falsche
  Werte an und unterschlägt das Visual-Start-Digit-Feld vollständig.
- Reine Anzeigekorrektur beim Lesen: keine Rückwirkung auf ID/Länder-/Temperaturanzeige, auf das
  Schreiben (T5577/EM4305) oder auf irgendeine Sendefunktion — die betroffenen vier Zeilen werden
  nur in `protocol_fdx_b_render_data()` ausgewertet, nirgends sonst im Protokoll verwendet.
- Klar von dem im selben Datei-Diff gefundenen, deutlich größeren Periodendecoder-Feature
  abgegrenzt (siehe unten) — eigener, in sich abgeschlossener Fund mit eigenem Upstream-Commit.

## Betroffene Dateien (Portumfang)

- `lib/lfrfid/protocols/protocol_fdx_b.c`
  - `protocol_fdx_b_render_data()`: die drei Feldauslese-Zeilen (`reserved`/`user_info`/
    `replacement_number`) durch vier Zeilen ersetzt (neues Feld `visual_start_digit`, `reserved`
    jetzt 2 statt 5 Bit, je mit `bit_lib_reverse_8_fast()`-Spiegelung); den abschließenden
    `furi_string_cat_printf()`-Aufruf auf fünf benannte Zeilen statt der einzelnen
    `"Bits: ..."`-Zeile umgestellt.

Keine Änderung an Headern, Signaturen, Decoder-/Encoder-Zustandsmaschine oder Schreibpfad (T5577/
EM4305) — ausschließlich die reine Anzeigefunktion.

## Risiko

- Minimaler Umfang: eine Funktion, eine Datei, keine neuen Felder in `ProtocolFDXB` selbst (nur
  lokale Variablen innerhalb der Funktion), keine API-/Struct-Änderung.
- Verhaltensänderung ausschließlich in der angezeigten Textausgabe beim Lesen einer Karte; Decoder,
  Encoder, Schreibpfad, Kurzanzeige (`protocol_fdx_b_render_brief_data()`, unverändert) und alle
  übrigen Felder (ID, Länder-Code, Temperatur, `animal_flag`) bleiben unberührt (siehe Host-Repro,
  Regressionsfälle).
- `bit_lib_reverse_8_fast()` ist bereits vorhandene, auch sonst in meiner Basis genutzte
  `bit_lib`-Funktion (siehe `protocol_keri.c`, `protocol_indala26.c` u. a.) — keine neue
  Abhängigkeit.
- Der Build (`fbt`/`ufbt`) deckt jede Signaturabweichung als Compile-Fehler auf.

## Nicht Teil dieses Ports

Im selben Datei-Diff (gegen Unleashed `dev`) zusätzlich gefunden, hier bewusst **nicht**
übernommen: ein deutlich umfangreicherer Periodendecoder (`protocol_fdx_b_feed_period()` u. a.,
Unleashed-Commit `525dcce0e`, "improve fdx-b reading") für Implantat-Chips mit verschobenem
Tastverhältnis (Duty-Cycle-Skew), der Flanke-zu-Flanke-Perioden statt einzelner Low-/High-Dauern
auswertet und dabei einen laufenden Bias nachführt. Eigener Zustand (vier neue Felder in
`ProtocolFDXB`), eigene Zeitfenster-Konstanten, eigenständiges Feature mit RF-Dekodier-Auswirkung
(nicht nur Anzeige) — ein eigener, größerer Port-Kandidat für eine spätere Sitzung, hier nur
vermerkt, um die Abgrenzung zu diesem rein anzeigebezogenen Fund klar zu halten.

## Port-Skizze

1. `lib/lfrfid/protocols/protocol_fdx_b.c` gegen meine mntm-012-Basis gegenlesen — bei der Prüfung
   für diesen Vorschlag unverändert zum zitierten Vorher-Stand (`git show
   97d6628f325...~1:lib/lfrfid/protocols/protocol_fdx_b.c`, byte-identisch zu meiner Fassung in
   der betroffenen Funktion).
2. Patch anwenden (aus dem Firmware-Wurzelverzeichnis):
   `patch -p1 < fdx-b-data-block-field-bit-order-fix/protocol_fdx_b_c.patch`
3. Host-Repro (`repro.c`, unten) mit
   `gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -O0`
   (bzw. `-O1`) bauen und laufen lassen — bereits von mir lokal ausgeführt, siehe Testhinweis.
4. Build: `ufbt` (bzw. `fbt firmware_all`).
5. Funktionstest lokal: eine FDX-B-Testkarte/-Transponder (ISO 11784/11785, z. B. ein Tieres-/
   Glasimplantat-Transponder) einlesen. Vorher/Nachher vergleichen: `RFU`/`User Info`/
   `Replacement Number` ändern sich (sofern die Karte diese Felder belegt), zusätzlich erscheint
   eine neue Zeile `Visual Start Digit`; `ID`, `Country Code`/`Country`, `Temperature` und
   `Animal`/`Data Block`/`RUDI Bit` bleiben unverändert zur alten Anzeige. Ohne eine zweite,
   unabhängige Quelle für den Klartext dieser Karte (z. B. ein Händler-/Tierarzt-Lesegerät) lässt
   sich der *richtige* Wert an echter Hardware nicht unabhängig verifizieren — das Host-Repro
   bestätigt stattdessen die Bit-Logik selbst gegen eine unabhängig berechnete Ground Truth (siehe
   Testhinweis).

## Testhinweis (lokal, ohne Hardware — reine Bit-Logik)

Host-kompilierbares Mini-Repro
(`repro-fdx-b-data-block-field-bit-order-fix/repro.c`) transkribiert die vier Feldauslese-Zeilen
wörtlich (ALT = bei mir vorhandene Fassung, NEU = gebundene Fassung aus Commit `97d6628f3`), dazu
`bit_lib_get_bit()`, `bit_lib_get_bits()` und `bit_lib_reverse_8_fast()` byte-genau aus
`lib/bit_lib/bit_lib.c`. Die Ground Truth wird **unabhängig** von diesen Funktionen über eine
direkte Bit-für-Bit-Definition berechnet (nicht durch erneuten Aufruf derselben
`bit_lib`-Funktionen), um eine zirkuläre Verifikation auszuschließen. Vier Szenarien:

```
== Szenario A: gemischtes Bitmuster ==
  ok: Ground Truth Visual Start Digit = reversed(110b) = 3
  ok: Ground Truth RFU = reversed(10b) = 1
  ok: Ground Truth User Info (korrigiert) = 13
  ok: Ground Truth Replacement Number = reversed(110b) = 3
  ok: NEU Visual Start Digit == Ground Truth
  ok: NEU RFU == Ground Truth
  ok: NEU User Info == Ground Truth
  ok: NEU Replacement Number == Ground Truth
  ok: ALT RFU liest MSB-zuerst 5 Bit ab Position 50 (verschluckt Visual Start Digit)
  ok: ALT RFU-Wert weicht vom (seit dem Fix) korrekten RFU-Wert ab
  ok: ALT User Info liest MSB-zuerst (keine Reversal)
  ok: ALT User-Info-Wert weicht vom korrekten Wert ab
  ok: ALT Replacement Number liest MSB-zuerst (keine Reversal)
  ok: ALT Replacement-Number-Wert weicht vom korrekten Wert ab
  ok: block_status unveraendert (keine Regression)
  ok: rudi_bit unveraendert (keine Regression)
  ok: animal_flag unveraendert (keine Regression)
== Szenario B: alle Bits 0 (Regression) ==
  ok: NEU: alle Felder 0 bei Eingabe 0
  ok: ALT: alle Felder 0 bei Eingabe 0 (keine Regression)
== Szenario C: alle Bits 1 (Regression) ==
  ok: NEU: alle Felder auf Maximalwert bei Eingabe 1
  ok: ALT: alte Feldgrenzen/-werte bei Eingabe 1 (reserved deckt 5 statt 2 Bit ab)
== Szenario D: nur Visual Start Digit = 100b (MSB-zuerst) gesetzt ==
  ok: NEU Visual Start Digit = reversed(100b) = 1 (korrekt als eigenes Feld sichtbar)
  ok: NEU RFU bleibt 0 (sauber von Visual Start Digit getrennt)
  ok: ALT zeigt diese Bits unter 'reserved' = 10000b = 16 -- Visual Start Digit existiert bei ALT gar nicht als Feld

24/24 Pruefungen bestanden.
ALL CHECKS PASSED
```

Gebaut/ausgeführt mit
`gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined` bei
`-O0`/`-O1`: identischer Output (per `diff` bestätigt), keine Compiler-Warnungen, keine
Sanitizer-Befunde. Testpuffer bewusst 11 Byte groß gewählt (`FDXB_DECODED_DATA_SIZE` in der
echten Firmware) statt nur der hier betrachteten 8 Byte (Bit 0–63) — ein erster Versuch mit einem
exakt 8 Byte großen Testpuffer löste einen ASan-`stack-buffer-overflow` in `bit_lib_get_bits()`
selbst aus (Zugriff auf `data[position/8 + 1]` bei `position=60`, unabhängig von der tatsächlich
benötigten Bitbreite) — das ist der bereits bekannte, separat dokumentierte Fund
`bit-lib-get-bits-read-past-buffer-fix` (eigene `TODO FL-3534`-Markierung im Quelltext), hier
nicht erneut verfolgt, nur durch einen realistisch großen Testpuffer umgangen, wie es die echte,
11 Byte breite `ProtocolFDXB.data`-Struktur ohnehin tut.

Patch gegen eine frische Zweitkopie der betroffenen Datei aus meiner eigenen mntm-012-Basis
verifiziert: einmal per `patch -p1` (`--dry-run` vorab, dann real), einmal per `git apply
--check`/real — beide Wege laut `md5sum` byte-identisch.

## Quelle

- `G4MEOVER18/G4MEOVER-FW`, `main`, Commit `b359f6fc32f282ce7217dd2d48a4e3478771852e`: eigene
  Basis für `lib/lfrfid/protocols/protocol_fdx_b.c`, in der betroffenen Funktion byte-identisch
  zum unten zitierten Unleashed-Vorher-Stand.
- `DarkFlippers/unleashed-firmware`, `dev`-Zweig, Commit `97d6628f32bf4cb9b71725686bd12bc536f39716`
  ("Make FDX-B readout more descriptive (#4297)", 2026-06-30): isolierter Anzeige-Fix, hier
  vollständig übernommen (eine Funktion, eine Datei, kein Bezug zum ebenfalls in diesem Zeitraum
  gelandeten Periodendecoder-Commit `525dcce0e` desselben Protokolls, siehe "Nicht Teil dieses
  Ports").
