# g4meover-magspoof — Stack-Buffer-Overflow in `mag_encode()` bei überlangen Track-Daten

## Was
Meine MagSpoof-App basiert auf `zacharyweiss/magspoof_flipper` (verfolgt über den
`astro-cyberpaws`-Fork). Die eigentliche Sendefunktion `mag_spoof()`
(`helpers/mag_helpers.c`) legt für jeden Track feste, stack-allozierte Zielpuffer an:

```c
uint8_t bits_t1_raw[64] = {0x00};        // 68 chars max track 1 + 1 char crc * 7 approx =~ 483 bits
uint8_t bits_t1_manchester[128] = {0x00}; // twice the above
uint16_t bits_t1_count = mag_encode(
    data1, (uint8_t*)bits_t1_manchester, (uint8_t*)bits_t1_raw, bitlen[0], sublen[0]);
```

(identisch für Track 2/3 mit `bits_t2_*`/`bits_t3_*`). Der Kommentar geht selbst von einer
Obergrenze von „68 chars max track 1" aus — das ist aber nirgends durchgesetzt.
`mag_encode()` läuft einfach bis zum Nullterminator von `data`:

```c
for(int i = 0; *(data + i) != 0; i++) {
    ...
    raw_bits_count = add_bit(tmp & 0x01, out_raw, raw_bits_count);
    output_count = add_bit_manchester(tmp & 0x01, out_manchester, output_count);
    ...
}
```

`add_bit()` schreibt ungeprüft nach `out[count / 8]` — die Funktion kennt die tatsächliche
Puffergröße überhaupt nicht, `mag_encode()` bekommt sie nicht als Parameter übergeben. Bei
Track 1 (`track_bits = 7`) passen `7 × (n + 1)` Bits (n Zeichen + 1 LRC-Byte) in die 512 Bit
(64 Byte) von `bits_t1_raw[64]` — ab `n = 73` Zeichen wird `out_raw[64]` bereits
außerhalb der Allokation beschrieben; `out_manchester` (128 Byte = 1024 Bit, exakt das
Doppelte) läuft bei derselben Zeichenzahl parallel über. Bei Track 2/3 (`track_bits = 5`)
liegt die Grenze bei `n = 102` Zeichen.

Diese Zeichengrenze ist bei mir an **mehreren** Stellen erreichbar, nicht nur bei einer
Ausnahme-Eingabe:
- **UART-Kartenlesung** (`mag_scene_read.c`): `mag_device_parse_card_string()` übernimmt die
  komplette per UART empfangene Zeichenkette ungekürzt in `dev_data.track[i].str`
  (dynamisch wachsender `FuriString`, kein Limit). Begrenzt wird nur der UART-Sammelpuffer
  selbst, auf `UART_TERMINAL_TEXT_BOX_STORE_SIZE = 4096` Byte — weit über der
  73/102-Zeichen-Grenze von `mag_encode()`.
- **`.mag`-Datei laden** (`mag_device_load_data()` in `mag_device.c`): liest die
  `Track %d`-Felder per `flipper_format_read_string()` ohne jede Längenprüfung — eine
  fehlerhafte oder absichtlich präparierte `.mag`-Datei mit überlangem Track-Feld reicht.
- **Manuelle Eingabe** (`mag_scene_input_value.c`, „Enter track data (WIP)"): das Textfeld
  erlaubt laut `MAG_TEXT_STORE_SIZE` bis zu 149 Zeichen — mehr als die 73/102-Zeichen-Grenze
  beider Track-Typen.

`mag_spoof()` wird bei jedem „Emulate"-Vorgang direkt aufgerufen (`mag_scene_emulate.c`); ein
zuvor über einen dieser drei Wege geladener/eingegebener überlanger Track führt bei mir also
bei jedem Emulationsversuch zu einem Stack-Buffer-Overflow in der zentralen Sendefunktion —
kein Sonderfall, sondern der reguläre Pfad für jede Karte/Eingabe, die die stillschweigend
angenommene Zeichengrenze überschreitet. `debug_mag_string()` (Debug-Log-Ausgabe, gleiche
Datei) hat exakt dasselbe Problem mit identisch dimensionierten lokalen Puffern.

Ich habe das mit einer eigenständigen, hostseitigen Nachbildung der reinen Bit-Pack-Logik
(`add_bit()`, `add_bit_manchester()`, `mag_encode()` — vollständig ohne Furi-/Flipper-Typen
nachvollziehbar) unter AddressSanitizer bestätigt: ein 100 Zeichen langer Track-1-String
(deutlich unterhalb dessen, was UART-Erfassung, Dateiladen oder sogar die manuelle
150-Zeichen-Eingabe zulassen) löst zuverlässig einen
`stack-buffer-overflow`-Absturz in `add_bit()`/`mag_encode()` aus, aufgerufen genau wie in
`mag_spoof()`, mit denselben `bits_t1_raw[64]`/`bits_t1_manchester[128]`-Puffergrößen — siehe
Testhinweis.

## Nutzen
- Behebt einen reproduzierbaren Stack-Buffer-Overflow in der zentralen Sendefunktion, der über
  drei unabhängige, bei mir alle real erreichbare Eingabewege ausgelöst werden kann (UART-Lesung
  einer Karte, geladene `.mag`-Datei, manuelle Eingabe) — nicht nur ein theoretisches
  Speicherproblem, sondern ein direkter Absturzpfad beim normalen „Emulate"-Vorgang.
- Zentral behoben in `mag_encode()` selbst: beide Aufrufstellen (`mag_spoof()`,
  `debug_mag_string()`) profitieren automatisch, ohne dass an den drei Eingabewegen einzeln je
  eine eigene Längenprüfung ergänzt werden müsste.
- Sauber degradiertes Verhalten statt Absturz: überlange Track-Daten werden ab der
  Pufferkapazität abgeschnitten (inkl. weiterhin korrektem LRC-Byte über die tatsächlich
  gesendeten Zeichen) statt den Prozess zu beenden.
- Rein arithmetische/Bit-Pack-Änderung, vollständig ohne Hardware nachrechenbar (siehe
  Testhinweis).

## Betroffene Dateien (lokal, in meiner MagSpoof-App)
- `helpers/mag_helpers.c` — `mag_encode()`: neuer Parameter `out_raw_capacity_bytes`, Grenzprüfung
  vor jedem Zeichen (mit reserviertem Platz für das abschließende LRC-Byte). Beide Aufrufer im
  selben File (`mag_spoof()`, `debug_mag_string()`) entsprechend angepasst (übergeben
  `sizeof(bits_t*_raw)`).
- `helpers/mag_helpers.h` — Prototyp von `mag_encode()` um den neuen Parameter ergänzt.

Keine Änderung an `add_bit()`/`add_bit_manchester()`, an `MagDevice`/`MagState` oder am
`.mag`-Dateiformat nötig.

## Patch
```c
// helpers/mag_helpers.h
uint16_t mag_encode(
    char* data,
    uint8_t* out_manchester,
    uint8_t* out_raw,
    uint8_t track_bits,
    uint8_t track_ascii_offset,
    uint16_t out_raw_capacity_bytes); // NEU
```

```c
// helpers/mag_helpers.c
uint16_t mag_encode(
    char* data,
    uint8_t* out_manchester,
    uint8_t* out_raw,
    uint8_t track_bits,
    uint8_t track_ascii_offset,
    uint16_t out_raw_capacity_bytes) { // NEU
    uint16_t raw_bits_count = 0;
    uint16_t output_count = 0;
    int tmp, crc, lrc = 0;

    // NEU: Platz fuer das abschliessende LRC-Byte reservieren, bevor die
    // Zeichen-Schleife laeuft. out_manchester ist an beiden Aufrufstellen
    // exakt doppelt so gross wie out_raw, daher genuegt eine Kapazitaetsgrenze.
    const uint16_t raw_bit_capacity = (uint16_t)(out_raw_capacity_bytes * 8);
    const uint16_t max_data_bits =
        (raw_bit_capacity > track_bits) ? (uint16_t)(raw_bit_capacity - track_bits) : 0;

    for(int i = 0; *(data + i) != 0; i++) {
        // NEU: weitere Zeichen wuerden out_raw/out_manchester ueberschreiben
        // oder dem LRC-Byte den Platz nehmen -- Eingabe an dieser Stelle kappen.
        if((uint16_t)(raw_bits_count + track_bits) > max_data_bits) break;

        crc = 1;
        tmp = *(data + i) - track_ascii_offset;

        for(int j = 0; j < track_bits - 1; j++) {
            crc ^= tmp & 1;
            lrc ^= (tmp & 1) << j;
            raw_bits_count = add_bit(tmp & 0x01, out_raw, raw_bits_count);
            output_count = add_bit_manchester(tmp & 0x01, out_manchester, output_count);
            tmp >>= 1;
        }
        raw_bits_count = add_bit(crc, out_raw, raw_bits_count);
        output_count = add_bit_manchester(crc, out_manchester, output_count);
    }

    // LRC byte (unveraendert -- durch die Reservierung oben bleibt immer Platz dafuer)
    tmp = lrc;
    crc = 1;
    for(int j = 0; j < track_bits - 1; j++) {
        crc ^= tmp & 0x01;
        raw_bits_count = add_bit(tmp & 0x01, out_raw, raw_bits_count);
        output_count = add_bit_manchester(tmp & 0x01, out_manchester, output_count);
        tmp >>= 1;
    }
    raw_bits_count = add_bit(crc, out_raw, raw_bits_count);
    output_count = add_bit_manchester(crc, out_manchester, output_count);

    return output_count;
}
```

Beide Aufrufstellen in `mag_spoof()` ergänzen nur den vierten/fünften Parameter um die
tatsächliche Puffergröße:
```c
uint16_t bits_t1_count = mag_encode(
    data1, (uint8_t*)bits_t1_manchester, (uint8_t*)bits_t1_raw, bitlen[0], sublen[0],
    sizeof(bits_t1_raw)); // NEU
// analog fuer bits_t2_count/bits_t3_count
```
`debug_mag_string()` entsprechend mit `sizeof(bits_raw)`.

`play_track()` (liest anschließend genau `bits_t*_count` Bits aus dem jeweiligen
Manchester-Puffer) ist von dieser Änderung nicht betroffen: der zurückgegebene `output_count`
spiegelt nach dem Fix exakt die tatsächlich geschriebene, gekappte Bitmenge wider, sodass keine
ungeschriebenen Pufferbereiche gelesen werden.

## Testhinweis (ohne Hardware)
Eigenständiges, hostseitiges Reproduktionsprogramm der reinen Bit-Pack-Logik (identisch zu
`add_bit()`/`add_bit_manchester()`/`mag_encode()`, keine Furi-/Flipper-Typen nötig):
`app-improvements/g4meover-magspoof/repro-mag-encode-fixed-buffer-overflow/` —
`repro_old.c` (aktueller Stand, `mag_encode()` ohne Kapazitätsparameter) und `repro_new.c`
(mit Fix). Jeder Testfall läuft in einem eigenen `fork()`-Kindprozess (isoliert wegen des
`static toggle`-Zustands in `add_bit_manchester()`), mit denselben
`bits_t1_raw[64]`/`bits_t1_manchester[128]`-Puffergrößen wie in `mag_spoof()`:

```sh
gcc -g -O0 -Wall -Wextra -fsanitize=address,undefined -o repro_old repro_old.c
gcc -g -O0 -Wall -Wextra -fsanitize=address,undefined -o repro_new repro_new.c
./repro_old   # 60 Zeichen: sauber. 100 Zeichen: AddressSanitizer stack-buffer-overflow, Kind mit Signal beendet
./repro_new   # 60 Zeichen: sauber. 100 Zeichen: sauber (Eingabe intern gekappt)
```

Bei mir lokal reproduziert: `repro_old` läuft beim 60-Zeichen-Fall sauber durch, stürzt beim
100-Zeichen-Fall mit `AddressSanitizer: stack-buffer-overflow ... in add_bit ... in mag_encode`
ab (`READ of size 1` einen Byte hinter `bits_t1_raw[64]`, Kindprozess durch Signal beendet);
`repro_new` läuft in beiden Fällen sauber durch (Exit-Code 0).

Funktionaler Test nach dem Patch, lokal auf dem Flipper (kein Sender/Empfänger nötig):
1. `ufbt` bauen, App starten.
2. Über „Enter track data (WIP)" (oder Kartenlesung/Dateiladen) einen Track mit deutlich mehr
   als 73 (Track 1) bzw. 102 (Track 2/3) Zeichen eingeben/laden.
3. „Emulate" starten: die App darf nicht abstürzen/neu starten. Optional per Debug-Log
   (`bits_t1_count`/`bits_t2_count`/`bits_t3_count`) prüfen, dass die gemeldete Bitzahl bei der
   Kapazitätsgrenze endet statt weiter zu wachsen.
4. Regressionsfall: ein normal langer Track (< 73/102 Zeichen, wie jede reale ISO-7811-Karte)
   liefert exakt dieselbe Bitausgabe wie vor dem Fix (durch `repro_old`/`repro_new` beim
   60-Zeichen-Fall bereits als identisch bestätigt).
5. Erst danach der eigentliche Hardware-Funktionstest (Sender/Empfänger) — das ist der Teil,
   der bei mir lokal passiert.

## Risiko
- Niedrig bis moderat: die Änderung fügt ausschließlich eine Grenzprüfung hinzu und ändert die
  Funktionssignatur von `mag_encode()` (neuer Parameter) — beide Aufrufstellen sind bekannt und
  im selben Patch mit angepasst, keine weiteren Aufrufer im Projekt.
- Verhalten im Normalfall (Track-Daten unterhalb der Kapazitätsgrenze, jede reale ISO-7811-Karte)
  bleibt byteidentisch — durch den Host-Repro-Vergleich beim 60-Zeichen-Fall bestätigt.
- Überlange Track-Daten werden künftig stillschweigend gekappt statt abzustürzen; für eine
  spätere Sitzung wäre ergänzend eine sichtbare Warnung (Popup/Log) beim Kappen sinnvoll, ist
  aber für die reine Absturzvermeidung nicht notwendig.

## Quelle
- `zacharyweiss/magspoof_flipper` (main): `helpers/mag_helpers.c`/`.h` (`mag_encode`, `add_bit`,
  `add_bit_manchester`, `mag_spoof`, `debug_mag_string`), `mag_device.c`
  (`mag_device_parse_card_string`, `mag_device_load_data`), `scenes/mag_scene_read.c`
  (`UART_TERMINAL_TEXT_BOX_STORE_SIZE`), `scenes/mag_scene_input_value.c`/`mag_i.h`
  (`MAG_TEXT_STORE_SIZE`).
- `astro-cyberpaws/magspoof_flipper` (Fork, main): identische `mag_encode()`-Implementierung,
  bestätigt den Bug auch dort.
- AddressSanitizer/UndefinedBehaviorSanitizer als Nachweiswerkzeug für den
  Stack-Buffer-Overflow-Fund.
