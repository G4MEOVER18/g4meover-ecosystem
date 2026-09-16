# SubGHz-Encoder: zwei unabhängige Off-by-eins-Überschreibungen in `subghz_protocol_blocks_get_upload_from_bit_array()`

## Was
Ich habe den offenen Rückstellpunkt "`dev` weiter gegenprüfen, sobald neue Commits vorliegen"
aufgegriffen und zusätzlich Unleashed erneut gegen den zuletzt geprüften Stand (`unlshd-092`)
gegengeprüft. Dabei ist zwischenzeitlich `unlshd-093` erschienen (55 neue Commits). Aus dieser
Reihe habe ich `cc2d85278` ("SubGHz: guard final upload buffer write", #1105) geprüft — eine
gemeinsame, von mehreren SubGHz-Protokoll-Encodern genutzte Bibliotheksfunktion in
`lib/subghz/blocks/encoder.c`, bei mir (`mntm-012`) byteidentisch zum Unleashed-Vorher-Stand
vorhanden.

## Fund: `subghz_protocol_blocks_get_upload_from_bit_array()` schreibt in zwei unabhängigen Fällen einen `LevelDuration`-Eintrag hinter das Ende des Ziel-Puffers

Die Funktion wandelt ein Bit-Array in eine Folge von `LevelDuration`-Einträgen (Pegel+Dauer) für
den Funk-Sendepuffer um. Bei jedem Pegelwechsel im Bit-Array wird ein Eintrag geschrieben:

```c
size_t index_bit = bias_bit;
bool last_bit = subghz_protocol_blocks_get_bit_array(data_array, index_bit++);
for(size_t i = 1 + bias_bit; i < count_bit_data_array + bias_bit; i++) {
    if(last_bit == subghz_protocol_blocks_get_bit_array(data_array, index_bit)) {
        duration += duration_bit;
    } else {
        if(size_upload > max_size_upload) {                       // (1) Off-by-eins
            furi_crash("SubGhz: Encoder buffer overflow");
        }
        upload[size_upload++] = level_duration_make(/* ... */);
        last_bit = !last_bit;
        duration = duration_bit;
    }
    index_bit++;
}
upload[size_upload++] = level_duration_make(/* ... */);            // (2) völlig ungeprüft
return size_upload;
```

Zwei voneinander unabhängige Stellen schreiben potenziell über das Ende des vom Aufrufer mit
genau `max_size_upload` Elementen allozierten `upload[]`-Arrays hinaus:

1. **Off-by-eins in der Schleifen-Prüfung (Zeile "(1)"):** Die Bedingung lautet
   `size_upload > max_size_upload` statt `size_upload >= max_size_upload`. Wenn `size_upload`
   bereits exakt `max_size_upload` erreicht hat, lässt die Prüfung den Schreibzugriff trotzdem
   durch — `upload[max_size_upload]` liegt aber schon einen Index hinter dem gültigen Bereich
   (`upload[0]` … `upload[max_size_upload - 1]`). Erst beim *nächsten* Durchlauf greift der
   `furi_crash()` — der Schreibzugriff darüber hinaus ist zu diesem Zeitpunkt aber schon
   passiert.
2. **Der abschließende Flush-Schreibzugriff nach der Schleife (Zeile "(2)") hat überhaupt keine
   Prüfung.** Sammelt die Schleife exakt `max_size_upload` Einträge ein (ohne dass die
   Off-by-eins-Prüfung aus (1) je auslöst, weil `size_upload` innerhalb der Schleife nie über
   `max_size_upload` hinausgeht), schreibt dieser letzte, für jeden Aufruf einmal ausgeführte
   Flush trotzdem bedingungslos einen weiteren Eintrag an Index `max_size_upload` — ohne jede
   Prüfung und ohne dass vorher irgendein `furi_crash()` gegriffen hätte. Dieser Fall ist der
   gefährlichere der beiden: er hinterlässt keinerlei Spur (kein Absturz, keine Log-Zeile),
   sondern beschädigt still einen `LevelDuration`-großen Speicherbereich direkt hinter dem
   Puffer.

Beide Fälle sind mit realistischen, vom Nutzer ladbaren Daten erreichbar: `subghz_protocol_blocks_get_upload_from_bit_array()`
wird bei mir aktuell von zwei Protokoll-Encodern aufgerufen — `bin_raw.c` (Protokoll "BinRAW",
gewählt für unbekannte/nicht klassifizierte SubGHz-Aufnahmen, `size_upload`/`upload[]` mit
`BIN_RAW_BUF_DATA_SIZE * 5 = 2560` Elementen alloziert, `count_bit_data_array` aus den geladenen
`Bit_RAW`/`Data_RAW`-Feldern einer `.sub`-Datei bis zu `BIN_RAW_BUF_DATA_SIZE * 8 = 4096` Bit) und
`chamberlain_code.c`. Eine `.sub`-Datei mit Protokoll `BinRAW` und einem Bitmuster, das nahe am
theoretischen Maximum zwischen zwei Pegeln wechselt (z. B. eine lange `0101…`-Sequenz oder ein
verrauschtes/adversarial aufgezeichnetes Signal), erzeugt genügend Pegelwechsel, um `size_upload`
bis an bzw. über `max_size_upload` zu treiben — keine echte Sendehardware nötig, das Laden und
Abspielen einer präparierten `.sub`-Datei genügt.

## Host-Repro
Eigenständiges Host-Repro, kompiliert die reale, unveränderte
`subghz_protocol_blocks_get_upload_from_bit_array()` aus meiner mntm-012-Basis (`encoder_base.c`,
ALT) sowie dieselbe Datei mit dem unten skizzierten Fix (`encoder_fixed.c`, NEW) unter je
eigenem Funktionsnamen (per `-D`-Umbenennung beim Kompilieren) in dasselbe Binary, sodass beide
echten, kompilierten Funktionen direkt miteinander verglichen werden können. Jeder Testfall läuft
fork-isoliert, `furi_crash()` ist auf `_exit(42)` gestubbt (statt echtem Halt-and-Catch-Fire), damit
"Guard hat ausgelöst" (Exit 42), "ASan hat einen Schreibzugriff außerhalb des Puffers erkannt"
(ASan-eigener Fehler-Exit) und "sauber zurückgekehrt" (Exit 0) im Elternprozess unterscheidbar
sind:

```
cd firmware-mining/repro-subghz-encoder-upload-buffer-overflow
gcc -std=gnu11 -O1 -Wall -Wextra -fsanitize=address,undefined -g -I. \
    -Dsubghz_protocol_blocks_set_bit_array=alt_set_bit_array \
    -Dsubghz_protocol_blocks_get_bit_array=alt_get_bit_array \
    -Dsubghz_protocol_blocks_get_upload_from_bit_array=alt_get_upload_from_bit_array \
    -c encoder_base.c -o encoder_base.o
gcc -std=gnu11 -O1 -Wall -Wextra -fsanitize=address,undefined -g -I. \
    -Dsubghz_protocol_blocks_set_bit_array=new_set_bit_array \
    -Dsubghz_protocol_blocks_get_bit_array=new_get_bit_array \
    -Dsubghz_protocol_blocks_get_upload_from_bit_array=new_get_upload_from_bit_array \
    -c encoder_fixed.c -o encoder_fixed.o
gcc -std=gnu11 -O1 -Wall -Wextra -fsanitize=address,undefined -g -I. -c stub_furi_crash.c -o stub_furi_crash.o
gcc -std=gnu11 -O1 -Wall -Wextra -fsanitize=address,undefined -g -I. -c repro.c -o repro.o
gcc -std=gnu11 -O1 -Wall -Wextra -fsanitize=address,undefined -g \
    encoder_base.o encoder_fixed.o stub_furi_crash.o repro.o -o repro
./repro
```

Tatsächliche Ausgabe (lokal ausgeführt, ASan-Report gekürzt):
```
Case B (final unguarded upload[size_upload++] after the loop, 9 bits into an 8-slot buffer):
==...==ERROR: AddressSanitizer: heap-buffer-overflow ... WRITE of size 4 ...
    #0 ... in alt_get_upload_from_bit_array encoder_base.c:53
  ALT: ASan: heap-buffer-overflow (OOB write happened, see report above) (exit 1)
  NEW: furi_crash() guard fired before any OOB write (exit 42)

Case A (mid-loop 'size_upload > max_size_upload' off-by-one, 10 bits into an 8-slot buffer):
==...==ERROR: AddressSanitizer: heap-buffer-overflow ... WRITE of size 4 ...
    #0 ... in alt_get_upload_from_bit_array encoder_base.c:46
  ALT: ASan: heap-buffer-overflow (OOB write happened, see report above) (exit 1)
  NEW: furi_crash() guard fired before any OOB write (exit 42)

Regression (realistic 24-bit code, 64-slot buffer, nowhere near the limit):
  OK: ALT and NEW agree byte-for-byte (15 upload entries)

ALL CHECKS PASSED
```

Beide Fälle sind unabhängig konstruiert (Case B mit `max_size_upload + 1` alternierenden Bits, die
die Schleifen-Prüfung nie auslösen, aber den ungesicherten Abschluss-Schreibzugriff treffen; Case A
mit einem Bit mehr, das genau die Off-by-eins-Schleifenprüfung selbst trifft) — ALT löst in beiden
Fällen exakt an der jeweils erwarteten Zeile einen von ASan erkannten
`heap-buffer-overflow`-Schreibzugriff aus, NEW löst stattdessen sauber den (bereits vorhandenen bzw.
neu ergänzten) `furi_crash()`-Guard aus, bevor geschrieben wird. Der Regressionsfall (ein
realistischer 24-Bit-Code, Puffer großzügig bemessen) liefert bei ALT und NEW byte-für-byte
identische Ergebnisse — der Fix ändert das Verhalten ausschließlich in den beiden Überlauffällen.

## Betroffene Dateien
- `lib/subghz/blocks/encoder.c` — `subghz_protocol_blocks_get_upload_from_bit_array()`, einziger
  Fix-Ort. Wirkt sich auf alle aktuellen Aufrufer aus (bei mir `lib/subghz/protocols/bin_raw.c` und
  `lib/subghz/protocols/chamberlain_code.c`); die übrigen ~130 SubGHz-Protokoll-Encoder bauen ihre
  `upload[]`-Arrays bei mir über andere, eigene Pfade auf und sind von diesem konkreten Fund nicht
  betroffen.

## Risiko
Gering. Zwei zusätzliche/verschärfte Prüfungen unmittelbar vor den beiden betroffenen
Schreibzugriffen, keine geänderte Signatur, keine neue Abhängigkeit. Für jeden Aufruf, bei dem die
Anzahl der Pegelwechsel unterhalb von `max_size_upload` bleibt (der weit überwiegende Normalfall,
siehe Regressionsfall oben), ist das Verhalten unverändert — betroffen ist ausschließlich der bisher
ungeprüfte bzw. falsch geprüfte Grenzfall selbst, der vorher entweder unbemerkt Speicher beschädigt
oder (im günstigeren der beiden Fälle) ohnehin schon abgestürzt wäre, nur eben erst nach dem
schädlichen Schreibzugriff statt davor.

## Port-Skizze / Fix
In `subghz_protocol_blocks_get_upload_from_bit_array()`: die bestehende Schleifen-Prüfung von `>`
auf `>=` verschärfen und dieselbe Prüfung zusätzlich vor dem bisher ungeprüften
Abschluss-Schreibzugriff einfügen:

```c
} else {
    if(size_upload >= max_size_upload) {
        furi_crash("SubGhz: Encoder buffer overflow");
    }
    upload[size_upload++] = level_duration_make(
        subghz_protocol_blocks_get_bit_array(data_array, index_bit - 1), duration);
    last_bit = !last_bit;
    duration = duration_bit;
}
index_bit++;
}
if(size_upload >= max_size_upload) {
    furi_crash("SubGhz: Encoder buffer overflow");
}
upload[size_upload++] = level_duration_make(
    subghz_protocol_blocks_get_bit_array(data_array, index_bit - 1), duration);
return size_upload;
```

Fertiger Unified-Diff gegen meine unveränderte `main`-Basis (`patch --dry-run -p1` vorab gegen eine
frische Basiskopie verifiziert, real angewendet und Byte-für-Byte gegen die host-getestete Fassung
abgeglichen) in
`firmware-mining/subghz-encoder-upload-buffer-overflow/encoder_c.patch`. Identisch zum
Unleashed-Fix (`cc2d85278`, PR #1105) — bewusst 1:1 übernommen, kein eigener Fix-Ansatz nötig, da
die Ursache und die richtige Grenzbedingung eindeutig sind.

## Testhinweis (lokal nachvollzogen)
`firmware-mining/repro-subghz-encoder-upload-buffer-overflow/repro.c`, host-kompilierbar, keine
Flipper-/Funk-Abhängigkeiten (Befehl und tatsächliche Ausgabe siehe Host-Repro oben).

Für den lokalen Funktionstest an echter Hardware: das Patch in `lib/subghz/blocks/encoder.c`
einbauen, `ufbt`/`fbt firmware_all` bauen, anschließend ein normales, bereits vorhandenes
SubGHz-Signal (KeeLoq/CAME/Nice o. ä.) senden — Verhalten muss unverändert bleiben (Regressionsfall).
Für den eigentlichen Grenzfall selbst ist am Gerät kein einfacher, ungefährlicher Weg ersichtlich,
ihn gezielt auszulösen (er braucht ein Signal mit ungewöhnlich vielen Pegelwechseln nahe am
2560-Element-Limit von `bin_raw.c`); der Host-Repro oben deckt die eigentliche Logik bereits
vollständig und unabhängig von Hardware ab. Nach dem Fix bleibt als sichtbare Änderung im
Fehlerfall nur, dass ein zu dichtes/zu langes BinRAW-Signal jetzt zuverlässig mit
`furi_crash("SubGhz: Encoder buffer overflow")` abbricht, statt (je nach Zufall der
Speicherbelegung) still Heap-Speicher zu beschädigen oder eine Zeile zu spät abzustürzen.

## Quelle
- `DarkFlippers/unleashed-firmware`, Tag `unlshd-093` (neu erschienen seit dem zuletzt geprüften
  `unlshd-092`), Commit `cc2d852787374fab6f9072f483ccb4602f83da4b` (PR #1105, "SubGhz: guard final
  upload buffer write").
- Gegen `Next-Flip/Momentum-Firmware`, Tag `mntm-012` (meine Basis) verglichen:
  `lib/subghz/blocks/encoder.c` vor dem Patch byteidentisch zum Unleashed-Vorher-Stand.
- API-87-Einordnung: reine appinterne Pufferarithmetik ohne Firmware-API-Bezug — kein
  firmwareversionsabhängiges Kompatibilitätsrisiko.
- Aus derselben `unlshd-093`-Commit-Reihe für eine künftige Sitzung vorgemerkt, noch nicht geprüft:
  `51a56fc40` (NFC: reject oversized FeliCa Lite dumps, #1106 — Datei-kontrollierte `Blocks
  total`/`Blocks read`-Felder beim `.nfc`-Laden ungeprüft, ähnlich zum bereits gelieferten
  EMV-`.nfc`-Loader-Fund), `4b3c614c8` (F7: reject sentinel for expansion callbacks, #1108),
  `c742dfe6c` (HID: bound stealth jiggler movement, #1111), `cc3a1bc79` (NFC: keep the CUID
  dictionary pass end marker out of the sector counter, #1115 — direkt verwandt zu meinem bereits
  gelieferten `nfc-unified-key-dictionary.md`-Port), `95c35fb82` (NFC: Save recovered MIFARE Classic
  keys to the user dictionary, #1118 — Feature, kein Sicherheitsfund), `104d96f86` (SubGHz: Add
  Superrollo (GW60) roller-shutter protocol, KeeLoq HCS361, #1068 — neues Protokoll, eigener
  Portaufwand analog zu Prastel/Telcoma-Cardin-EDGE).
