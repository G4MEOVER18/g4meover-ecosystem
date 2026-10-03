# NFC Tools — Fortsetzung des NFC-Werkzeug-Clusters: ICODE-NDEF-Schreibpfad liest über das Ende der lokalen CC-Variable hinaus

## Was
Ich habe den NFC-Tools-Cluster (`applications/external/nfctools/`, Autor `wakdev`, `fap_version` 1.3,
Quelle `github.com/wakdev/nfctools-fz`) erneut aufgegriffen. Laut `BACKLOG.md` ist der Quelltext-Umfang
des Clusters bereits vollständig durchgesehen (zuletzt bestätigt bei der Sektor-Analyse-Sitzung:
"restlicher NFC-Werkzeug-Cluster nur noch `chameleon`/`nfc_relay`, außerhalb reiner Quelltextprüfung")
— `ideas/pentest-app-katalog.md` ist an der entsprechenden Stelle nicht mehr aktuell (dort steht noch
"restliche ~10 100 Zeilen offen"), siehe Korrektur unten. Statt einen neuen Dateibereich zu lesen, habe
ich gezielt `core/nfc_tools_icode.c` (635 Zeilen) ein zweites Mal gegen genau die Fehlerklasse geprüft,
die den bereits dokumentierten Fund in `firmware-mining/nfctools-icode-cc-mlen-overflow-fix.md` ausgelöst
hat (eine Mehrdeutigkeit in derselben Funktion, `iso15693_write_ndef_blocks()`) — und dabei einen
zweiten, von jenem Fund unabhängigen Bug in derselben Codestelle gefunden, den die vorherige Sitzung
nicht erfasst hatte.

## Fund: `cc_expected[4]` ist kleiner als die Anzahl Bytes, die tatsächlich daraus gesendet werden
`iso15693_write_ndef_blocks()` (`core/nfc_tools_icode.c:397`) erzeugt vor dem eigentlichen NDEF-Schreiben
die NFC-Forum-Type-5-Tag-Capability-Container (CC) und schreibt sie auf Block 0, falls noch keine
gültige CC vorhanden ist:

```c
uint8_t mlen = (uint8_t)(((uint32_t)(block_count - 1) * block_size) >> 3);
const uint8_t cc_expected[4] = {0xE1, 0x40, mlen, 0x00};
...
if(!cc_valid) {
    nfcv_build_write_blk(tx, 0, cc_expected, block_size);
    ...
```

`cc_expected` ist ein lokales **4-Byte**-Array — die kurze CC besteht laut NFC-Forum-Type-5-Tag-Spec
tatsächlich nur aus 4 Bytes. Der Aufruf übergibt dieses Array aber zusammen mit `block_size` an
`nfcv_build_write_blk()` (`core/nfc_tools_icode.c:388-395`):

```c
static void
    nfcv_build_write_blk(BitBuffer* tx, uint8_t blk, const uint8_t* data, uint8_t block_size) {
    bit_buffer_reset(tx);
    bit_buffer_append_byte(tx, ISO15693_3_REQ_FLAG_DATA_RATE_HI);
    bit_buffer_append_byte(tx, ISO15693_3_CMD_WRITE_BLOCK);
    bit_buffer_append_byte(tx, blk);
    bit_buffer_append_bytes(tx, data, block_size);
}
```

`bit_buffer_append_bytes()` kopiert genau `block_size` Bytes **ab** `data` — hier liest sie also über das
Ende der 4-Byte-`cc_expected`-Variable hinaus, sobald `block_size > 4` ist. `block_size` stammt aus
`iso15693_3_get_block_size(iso)`, der vom Tag selbst während des Pollings über die Systeminformationen
gemeldeten physischen Blockgröße — laut ISO/IEC 15693-3 (VICC Memory Size, 5-Bit-Feld, Wert 0–31 für
Blockgröße 1–32 Byte) ein regulärer, spezifikationskonformer Wert zwischen 1 und 32 Byte, nicht auf 4
beschränkt. Derselbe Sachverhalt ist bei mir bereits an anderer Stelle bestätigt: die eigene
`firmware-mining/nfc-iso15693-read-multi-blocks-and-parser-overrun-fix.md`-Recherche zeigt, dass die
Firmware selbst `bytes_per_block` als variablen, aus `system_info` gelesenen Wert behandelt (dort mit dem
Beispielwert 4, aber ausdrücklich nicht fest codiert).

Der einzige Guard vor dem Aufruf (`core/nfc_tools_icode.c:518`/`561`,
`if(block_count < 2 || block_size < 4) { ... return false; }`) prüft nur die **Untergrenze** — ein Tag,
der eine laut Standard gültige Blockgröße über 4 Byte meldet (z. B. 8 oder 32 Byte), passiert diesen Guard
unverändert und löst beim CC-Schreiben einen Stack-Overread von bis zu 28 Byte aus. Die ausgelesenen,
Programmzustand-abhängigen Stack-Bytes (Rückgabeadressen, andere lokale Variablen, je nach
Compiler/Optimierung) werden anschließend als Teil des WRITE_BLOCK-Frames über das HF-Feld an den Tag
gesendet — auf dem eingebetteten Ziel (STM32WB55, keine MMU/Seitenschutz) im günstigsten Fall ein
funktionslogischer Fehler (eine CC mit Füllbytes aus fremdem Stack-Inhalt statt der vorgesehenen Nullen),
im ungünstigsten Fall je nach Stack-Layout eine kleine Informationslücke (Bytes aus dem eigenen
Programmzustand werden per Funkfeld abgestrahlt und sind für jedes mithörende Lesegerät in Reichweite
sichtbar).

Auslösbar durch jeden ISO15693-Tag mit einer gültigen, aber von 4 abweichenden Blockgröße, auf den "NFC
Tools" → ICODE → Write NDEF angewendet wird, solange der Tag noch keine gültige CC trägt (frischer/
unformatierter Tag oder ein Tag mit abweichendem CC-Inhalt) — der mit Abstand häufigste praktische
Anwendungsfall dieser Funktion (NDEF auf einen ISO15693-Tag schreiben, der noch keine NDEF-Struktur hat).

Bemerkenswert: Die vorherige, bereits sehr gründliche Sitzung
(`firmware-mining/nfctools-icode-cc-mlen-overflow-fix.md`) hat genau diese Funktion und sogar genau
dieselbe Zeile (`mlen`-Berechnung direkt über `cc_expected`) bereits geprüft und einen anderen, ebenfalls
realen Bug gefunden (CC.MLEN läuft bei großer Tag-Kapazität unbemerkt um) — dabei aber nicht erfasst, dass
`cc_expected` selbst zu klein für den nachfolgenden `nfcv_build_write_blk()`-Aufruf ist. Im "Zusätzlich
geprüft, kein Fund"-Abschnitt jener Sitzung wird zwar die generelle Frage einer von 4 abweichenden
Blockgröße aufgeworfen, dort aber ausdrücklich nur für die *Format*-Callbacks
(`iso15693_build_write_frame_fmt()`, die lokal eine feste `zeros[4]`-Konstante verwenden) als
"spekulativer Kandidat" zurückgestellt — die NDEF-Schreibfunktion, die `block_size` bereits korrekt
parametrisiert (daher dort als unauffällig eingestuft), hat denselben Fehler aber tatsächlich, nur eine
Ebene tiefer in derselben Funktion, an der `cc_expected`-Variable selbst statt an den umgebenden Aufrufen.

## Host-Repro
Eigenständiges Host-Repro (kein Flipper-/NFC-Poller-Bezug), das `nfcv_build_write_blk()` auf die
fundrelevante Kopieroperation reduziert und den CC-Schreibzweig 1:1 aus `iso15693_write_ndef_blocks()`
transkribiert (ALT = wie im Original, NEU = `cc_expected` auf 32 Byte dimensioniert plus defensive
Klammerung):

```
cd firmware-mining/repro-nfctools-icode-cc-write-oob-read
gcc -O0 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro
gcc -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro
```

Tatsächliche Ausgabe (lokal ausgeführt, bei -O0 und -O1 identisch):
```
=== ALT (iso15693_write_ndef_blocks CC-Schreibzweig, verbatim) ===
[OK  ] ALT regression (bs=4)      block_count=64   block_size=4   -> survived
[OK  ] ALT FUND (bs=8)            block_count=32   block_size=8   -> CRASHED (ASan stack overread)
[OK  ] ALT FUND (bs=32)           block_count=16   block_size=32  -> CRASHED (ASan stack overread)

=== NEU (auf Blockgroesse dimensionierter CC-Puffer) ===
[OK  ] NEU regression (bs=4)      block_count=64   block_size=4   -> survived
[OK  ] NEU (bs=8)                 block_count=32   block_size=8   -> survived
[OK  ] NEU (bs=32)                block_count=16   block_size=32  -> survived

=== Aequivalenz ALT/NEU der CC-Bytes bei bs=4 (Regressionsfall) ===
[OK  ] ALT und NEU identisch fuer alle block_count bei bs=4

ALLE CHECKS BESTANDEN
```

Regressionsfall (Blockgröße 4, der weit überwiegende reale Fall bei ICODE/SLIX-Tags): ALT und NEU liefern
identische CC-Bytes für jeden geprüften `block_count`-Wert (2–256) — der Fix ändert am normalen
Anwendungsfall nichts. Die beiden Fund-Szenarien (Blockgröße 8 bzw. die laut Standard maximale Blockgröße
32) lösen bei ALT deterministisch einen von AddressSanitizer erkannten `stack-buffer-overflow` beim
`memcpy` in `nfcv_build_write_blk()` aus, exakt am Ende der 4-Byte-`cc_expected`-Variable; NEU übersteht
beide ohne Overread.

## Betroffene Dateien
- `core/nfc_tools_icode.c` — `iso15693_write_ndef_blocks()` (Zeile ~417–440), einziger Fix-Ort; betrifft
  beide Aufrufstellen (generischer ISO15693-3-Zweig und SLIX/SLIX2-Zweig, die dieselbe gemeinsame
  Funktion nutzen).

## Risiko
Gering. `cc_expected` wird von 4 auf 32 Byte vergrößert (volle laut ISO/IEC 15693-3 mögliche
Blockgrößenspanne) und mit führenden Nullen vorbelegt statt per Initialisierungsliste befüllt — reine
Puffervergrößerung, keine geänderte Semantik der ersten vier (einzig relevanten) Bytes. Zusätzlich eine
defensive Klammerung der tatsächlich gesendeten Länge auf `sizeof(cc_expected)`, falls ein nicht
konformer Tag einen noch größeren Wert meldet (schützt auch vor einem hypothetischen, aus der
Spezifikation herausfallenden `block_size`, ohne dass dafür ein reales Tag bekannt sein muss). Verhalten
beim weit überwiegenden realen Fall (Blockgröße 4, siehe Regressionsfall im Repro) bleibt unverändert.

Kompatibel mit dem in `firmware-mining/nfctools-icode-cc-mlen-overflow-fix.md` vorgeschlagenen, bisher
nicht als Patch vorliegenden Fix: jener ändert nur die `mlen`-Berechnung direkt davor (Bereichsprüfung vor
dem Cast auf `uint8_t`), dieser hier nur die Deklaration/Befüllung von `cc_expected` selbst sowie die
Aufrufstelle — beide Änderungen lassen sich ohne Konflikt von Hand zusammenführen (die `mlen`-Prüfung läuft
vor den hier geänderten Zeilen und ändert an ihnen nichts).

## Port-Skizze / Fix
1. `cc_expected` von einer 4-Byte-Initialisierungsliste auf ein 32-Byte-Array mit führenden Nullen
   umstellen, die ersten vier Bytes wie bisher setzen:
   ```c
   // Sized for the full ISO/IEC 15693-3 VICC block size range (1-32 bytes),
   // not just the 4-byte CC itself: nfcv_build_write_blk() below sends
   // `block_size` bytes starting at this buffer, and block_size is reported
   // by the tag, not fixed at 4.
   uint8_t cc_expected[32] = {0};
   cc_expected[0] = 0xE1;
   cc_expected[1] = 0x40;
   cc_expected[2] = mlen;
   cc_expected[3] = 0x00;
   ```
2. An der Aufrufstelle die tatsächlich gesendete Länge defensiv auf die Puffergröße klammern:
   ```c
   // Defensive clamp in case a non-conformant tag reports a block size
   // outside the ISO/IEC 15693-3 range: never read past cc_expected.
   uint8_t cc_write_size =
       (block_size <= sizeof(cc_expected)) ? block_size : (uint8_t)sizeof(cc_expected);
   nfcv_build_write_blk(tx, 0, cc_expected, cc_write_size);
   ```
3. Kein Fix an `nfcv_build_write_blk()`/`bit_buffer_append_bytes()` selbst nötig — beide verhalten sich
   korrekt für die ihnen übergebene Länge, der Fehler liegt ausschließlich in der zu kleinen
   Quellvariable am Aufrufer.

Fertiger, geprüfter Patch: `firmware-mining/nfctools-icode-cc-write-oob-read-fix/nfc_tools_icode_c.patch`
(`git apply --check` vorab gegen eine frische, unabhängige Kopie von
`RogueMaster/flipperzero-firmware-wPlugins` Branch `420` verifiziert, real angewendet und byte-für-byte
mit der beabsichtigten Endfassung abgeglichen).

## Testhinweis (lokal nachvollzogen)
`firmware-mining/repro-nfctools-icode-cc-write-oob-read/repro.c`, host-kompilierbar, keine
Flipper-/NFC-Abhängigkeiten (Befehl und tatsächliche Ausgabe siehe Host-Repro oben).

Für den lokalen Funktionstest an echter Hardware: einen eigenen ISO15693-3-Tag mit einer über
`GET_SYSTEM_INFO` gemeldeten Blockgröße ungleich 4 (falls kein passendes reales Tag vorhanden ist, wie
beim bereits dokumentierten CC.MLEN-Fund über einen zweiten Flipper/ISO15693-Emulator mit entsprechend
konfigurierten Systeminformationen nachstellbar) mit "NFC Tools" → ICODE → Write NDEF eine kurze
Testnachricht schreiben. Der eigentliche Overread selbst ist am Gerät ohne ASan nicht direkt sichtbar
(kein garantierter Absturz); als Regressionstest reicht ein Standard-ICODE/SLIX-Tag mit Blockgröße 4 —
Verhalten muss vor/nach dem Patch identisch bleiben.

## Korrektur an `ideas/pentest-app-katalog.md`
Die dortige Zeile "Restlicher Cluster: nur noch `nfctools` (~11 200 Zeilen) offen" (NFC-Comparator-
Sitzung) sowie die direkt folgende "restliche ~10 100 Zeilen offen" (NFC-Tools-Einstiegssitzung) sind
nicht mehr aktuell — laut `BACKLOG.md` ist der Quelltext-Umfang von `nfctools` seit der
Sektor-Analyse-Sitzung vollständig durchgesehen (einzig `chameleon`/`nfc_relay` bleiben als
Fremd-/Zweitgerät-Fälle offen). Diese Sitzung selbst zeigt, dass "vollständig durchgesehen" einen erneuten,
gezielten zweiten Blick auf eine bereits geprüfte Datei nicht ausschließt, wenn er auf eine andere
Fehlerklasse zielt (hier: der zuvor nicht geprüfte Zusammenhang zwischen `cc_expected`s Puffergröße und
der an sie gebundenen `block_size`-Parametrisierung).

## Zusätzlich geprüft, kein neuer Fund
- `cc_current[4]`, die Variable für die *gelesene* CC: unabhängig von `block_size`, da die CC laut
  Spezifikation immer in den ersten 4 Bytes von Block 0 liegt — der Lesepfad kopiert bewusst nur 4 feste
  Bytes aus der Antwort (`memcpy(cc_current, rx_data + 1, 4)`), unabhängig von der tatsächlichen
  physischen Blockgröße. Kein Fund, korrektes Verhalten.
- `padded`-Puffer für die eigentlichen NDEF-Datenblöcke (`malloc(num_blocks * block_size)`): bereits in
  `firmware-mining/nfctools-icode-cc-mlen-overflow-fix.md` als unauffällig bestätigt, hier erneut
  gegengeprüft (Allokationsgröße ist exakt an `block_size` gekoppelt, kein Mismatch wie bei
  `cc_expected`) — kein neuer Fund.

## Quelle
- `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`, Sparse-Checkout, Stand dieser Sitzung,
  identischer Commit-Stand wie die vorherigen NFC-Tools-Sitzungen): `applications/external/nfctools/`,
  `fap_author` "wakdev", Original-Quelle `github.com/wakdev/nfctools-fz`, `fap_version` 1.3.
- API-87-Einordnung: der Fund betrifft ausschließlich appeigene Puffer-/Längenlogik (eine lokale
  C-Variable, keine Firmware-API) — kein firmwareversionsabhängiges Kompatibilitätsrisiko; die
  zugrundeliegenden `iso15693_3_get_block_size()`/`bit_buffer_append_bytes()`-Aufrufe selbst bereits in
  vorherigen Sitzungen gegen `mntm-012` als unauffällig eingestuft.
- Eigener Fund: `core/nfc_tools_icode.c`, `iso15693_write_ndef_blocks()`, mit Host-Repro bestätigt (siehe
  `firmware-mining/repro-nfctools-icode-cc-write-oob-read/repro.c`), Patch verifiziert (siehe
  `firmware-mining/nfctools-icode-cc-write-oob-read-fix/nfc_tools_icode_c.patch`).
