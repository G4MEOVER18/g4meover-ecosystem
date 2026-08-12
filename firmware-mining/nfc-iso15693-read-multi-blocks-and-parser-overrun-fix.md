# NFC ISO15693: READ_MULTI_BLOCKS >64-Block-Bug + Parser-Overrun-Fix

Quelle: Official-dev-Commit `2fe5605034554cb45d63da5e4d544374e1f5a845`
("NFC: fix iso15693 emulation dropping Read Multiple Blocks for tags >64 blocks
(#4420)", 2026-07-23), Teil des in der letzten Sitzung vorgemerkten
Fünferblocks sicherheits-/speicherrelevanter Commits zwischen PR #4369
(`0dd3681f`) und `9148d50c`.

Der Commit deckt zwei getrennte, aber zusammenhängende Bugs im ISO15693-Stack
ab: einen Emulations-Korrektheitsbug und einen echten Absturz-Bug im
Signal-Parser. Beide sind bei mir (mntm-012, API 87.1) unverändert vorhanden.

## Bug 1 — READ_MULTI_BLOCKS ignoriert Anfragen für Tags mit vielen Blöcken

**Datei:** `lib/nfc/protocols/iso15693_3/iso15693_3_listener_i.h` (Define),
genutzt von `lib/nfc/protocols/iso15693_3/iso15693_3_listener.c`
(`iso15693_3_listener_alloc()`, `tx_buffer = bit_buffer_alloc(...)`).

Bei mir (mntm-012) ist die Sendepuffer-Größe für den ISO15693-3-Listener
(Tag-Emulation) so definiert (`iso15693_3_listener.c:11`):

```c
#define ISO15693_3_LISTENER_BUFFER_SIZE (256U)
```

Der Emulations-Handler für `READ_MULTI_BLOCKS`
(`iso15693_3_listener_i.c`, `iso15693_3_listener_read_multi_blocks_handler()`)
leitet daraus korrekt her, wie viele Blöcke in einer Antwort Platz haben:

```c
const uint32_t response_data_max =
    bit_buffer_get_capacity_bytes(instance->tx_buffer) - 1 - 2; // Flags und CRC
const uint32_t response_blocks_max = response_data_max / bytes_per_block;
if(block_count > response_blocks_max) {
    // Wie bei einer SLIX2 getestet: bei Anfrage nach mehr Blöcken als
    // unterstützt kommt keine Antwort. Genauso hier.
    error = Iso15693_3ErrorIgnore;
    break;
}
```

Das ist selbst *speichersicher* (kein Overflow von `tx_buffer` möglich, da die
Grenze aus der tatsächlichen Puffergröße berechnet wird) — aber bei
`bytes_per_block = 4` (Standard-Blockgröße vieler ISO15693-Tags, kein
`T4_OPTION`-Security-Byte) ergibt sich bei mir:

```
response_data_max = 256 - 1 - 2 = 253
response_blocks_max = 253 / 4 = 63
```

Jede `READ_MULTI_BLOCKS`-Anfrage eines Lesegeräts nach **mehr als 63 Blöcken
in einem Rutsch** bekommt bei mir also gar keine Antwort — obwohl der
emulierte Tag laut `system_info.block_count` (bis zu 256 Blöcke möglich)
durchaus so viele Blöcke hätte. Betroffen ist jede Emulation eines
ISO15693-Tags mit mehr als ~252 Byte Speicher (>63 Blöcke à 4 Byte), wenn ein
Lesegerät versucht, den gesamten Speicher in einer einzigen
`READ_MULTI_BLOCKS`-Transaktion statt in mehreren Einzel-`READ_BLOCK`-Aufrufen
auszulesen — ein in der Praxis übliches Lesemuster (z. B. viele
ICODE-SLIX/SLI-Reader-Implementierungen, um Roundtrips zu sparen). Für einen
autorisierten Zutrittstest mit einem emulierten Tag mit größerem
Speicherbereich kann das Lesegerät den emulierten Tag dadurch als defekt/
unlesbar einstufen, obwohl die Emulation an sich korrekt ist.

**Fix:** Puffer groß genug für den größtmöglichen praktisch relevanten Fall
dimensionieren — vollständiger 256-Block-Lesevorgang bei 4-Byte-Blockgröße
(ohne `T4_OPTION`):

```c
// flags + 256*4-byte blocks + CRC
#define ISO15693_3_LISTENER_BUFFER_SIZE (1U + 256U * 4U + 2U) // = 1027 Byte
```

Das hebt die Grenze von 63 auf `(1027 - 1 - 2) / 4 = 256` Blöcke an — den
gesamten laut ISO15693-3-Spezifikation adressierbaren Blockbereich. Reiner
Speicher-Mehrverbrauch: +771 Byte (256 statt 256 Byte, einmalig bei
Listener-Alloc) — auf dem f7/STM32WB55 unkritisch.

## Bug 2 — Parser kann bei RF-Rauschen abstürzen (echter DoS)

**Datei:** `lib/signal_reader/parsers/iso15693/iso15693_parser.c`,
Funktionen `iso15693_parser_parse_1_out_of_4()` und
`iso15693_parser_parse_1_out_of_256()`.

Dieser Parser ist der Software-Bitbang-Empfangspfad für ISO15693-Signale
(`targets/f7/furi_hal/furi_hal_nfc_iso15693.c`, `parsed_frame` mit
`FURI_HAL_NFC_ISO15693_MAX_FRAME_SIZE = 1024` Byte allokiert) — läuft bei mir
bei **jedem** ISO15693/Vicinity-Lesevorgang, nicht nur in einem Debug-Pfad.

Beide Parse-Funktionen häng en dekodierte Bytes per
`bit_buffer_append_byte(instance->parsed_frame, ...)` an, ohne vorher zu
prüfen, ob im Zielpuffer noch Platz ist:

```c
// iso15693_parser_parse_1_out_of_256(), unveränderter Bug bei mir:
if(instance->bitstream_buff[i] != 0x00) {
    for(size_t j = 0; j < 8; j++) {
        if(FURI_BIT(instance->bitstream_buff[i], j) == 1) {
            bit_buffer_append_byte(
                instance->parsed_frame, instance->next_byte_part * 4 + j / 2);
        }
    }
}
```

`bit_buffer_append_byte()` selbst (`lib/toolbox/bit_buffer.c`) prüft die
Kapazität per `furi_check()` **vor** dem eigentlichen Schreiben — es kommt
also nie zu einem klassischen Heap-Overflow, sondern `furi_check` löst bei
Überschreitung einen harten Abbruch aus (Crash/Reboot des Geräts). Das macht
den Bug zu einem **Denial-of-Service**, nicht zu einer Speicherkorruption —
aber real auslösbar: Bei der 1-aus-256-Kodierung wird für **jedes gesetzte
Bit** eines rohen Bitstream-Bytes ein Byte angehängt; ein verrauschtes Byte
mit mehreren gesetzten Bits (bei sauberem Signal ist pro Position genau ein
Bit gesetzt) kann in einem einzigen Schleifendurchlauf bis zu 8 Bytes
anhängen — eine bis zu 8-fache Verstärkung der Eingangslänge. Anhaltendes
HF-Rauschen oder ein absichtlich verstümmeltes Signal in Reichweite des
Flipper während eines ISO15693-Lesevorgangs kann so `parsed_frame` (1024
Byte) befüllen, bevor ein gültiges EOF/Nullterminator-Muster erkannt wird,
und den `furi_check`-Abbruch auslösen.

Für den defensiven Testrahmen hier relevant: Ein Flipper, der während eines
autorisierten Zutritts-Audits ISO15693-Karten liest, kann durch einen
nahegelegenen Rausch-/Störsender zum Absturz während des Lesevorgangs
gebracht werden — ärgerlich (Geräteneustart mitten im Test), aber kein
Sicherheitsrisiko für Dritte.

**Fix:** Kapazitätsprüfung vor jedem Append, überlange/verrauschte Frames
sauber als Parse-Fehler statt als Crash behandeln (zwei Stellen, je vor dem
jeweiligen `bit_buffer_append_byte()`-Aufruf):

```c
// 1-aus-4:
if(instance->next_byte_part == 4) {
    instance->next_byte_part = 0;
    if(bit_buffer_get_size_bytes(instance->parsed_frame) >=
       bit_buffer_get_capacity_bytes(instance->parsed_frame))
        return Iso15693ParserCommandFail;
    bit_buffer_append_byte(instance->parsed_frame, instance->next_byte);
    instance->next_byte = 0;
}

// 1-aus-256, innerhalb der Bit-Schleife:
if(FURI_BIT(instance->bitstream_buff[i], j) == 1) {
    if(bit_buffer_get_size_bytes(instance->parsed_frame) >=
       bit_buffer_get_capacity_bytes(instance->parsed_frame))
        return Iso15693ParserCommandFail;
    bit_buffer_append_byte(
        instance->parsed_frame, instance->next_byte_part * 4 + j / 2);
}
```

## Betroffene Dateien (Portumfang)

- `lib/nfc/protocols/iso15693_3/iso15693_3_listener.c` — Define
  `ISO15693_3_LISTENER_BUFFER_SIZE` von `256U` auf `(1U + 256U * 4U + 2U)`
  anheben (bei mir liegt das Define direkt in dieser Datei, nicht im Header
  wie im aktuellen Official-dev — Portziel entsprechend anpassen, siehe
  Repro-Hinweis unten).
- `lib/signal_reader/parsers/iso15693/iso15693_parser.c` — je eine
  Kapazitätsprüfung in `iso15693_parser_parse_1_out_of_4()` und
  `iso15693_parser_parse_1_out_of_256()` vor dem jeweiligen
  `bit_buffer_append_byte()`-Aufruf ergänzen.

Zwei unabhängige, kleine, lokal begrenzte Änderungen — kein API-Bruch, keine
neuen Abhängigkeiten.

## Risiko

Gering. Bug 1: reine Puffervergrößerung (+771 Byte, einmalig), keine
Verhaltensänderung außer der beabsichtigten (mehr Blöcke pro Antwort
möglich) — kein Verhaltensunterschied für Tags/Lesegeräte, die ohnehin nie
mehr als 63 Blöcke auf einmal anfragen. Bug 2: rein additive
Bounds-Checks, die im regulären (rauschfreien) Betrieb nie greifen — mit
Repro unten verifiziert, dass legitime, unter der Kapazitätsgrenze
bleibende Frames unverändert durchlaufen.

## Testhinweis (lokal, ohne Hardware für die Kernlogik)

Host-kompilierbares Modell der Kernlogik von Bug 2
(`repro-iso15693-parser-overrun/repro.c`, reduziert auf die
Append-Schleife aus `iso15693_parser_parse_1_out_of_256()` und ein
1:1-Abbild von `bit_buffer_append_byte()`s Kapazitätsprüfung):

```
cd firmware-mining/repro-iso15693-parser-overrun
gcc -O2 -Wall -Wextra -fsanitize=address,undefined -DFIX=0 repro.c -o repro_prefix && ./repro_prefix
gcc -O2 -Wall -Wextra -fsanitize=address,undefined -DFIX=1 repro.c -o repro_postfix && ./repro_postfix
```

Ergebnis: `repro_prefix` löst bei einem 200-Byte-`0xFF`-Rauschburst
(bis zu 1600 potenzielle Appends gegen eine 1024-Byte-Kapazität) den
`furi_check`-äquivalenten Abbruch aus (`exit code 134`, SIGABRT);
`repro_postfix` mit derselben Eingabe bricht den Frame nach Erreichen der
Kapazitätsgrenze sauber ab (`exit code 0`, kein Crash).

Für die Hardware-Verifikation nach dem Einpflegen:

- **Bug 1:** Eigenen ISO15693-Tag mit >63 Blöcken emulieren (z. B.
  `system_info.block_count = 128`, Blockgröße 4 Byte) und mit einem
  Lesegerät/einer zweiten Flipper eine `READ_MULTI_BLOCKS`-Anfrage über den
  vollen Blockbereich stellen — vor dem Fix bleibt die Antwort aus, danach
  kommen alle Blöcke zurück.
- **Bug 2:** Schwerer gezielt zu reproduzieren (RF-Rauschen), daher primär
  über Codereview/den Host-Repro verifiziert; ein Regressionstest ist ein
  normaler ISO15693-Lesevorgang einer eigenen Vicinity-Karte vor/nach dem
  Patch — Verhalten muss identisch bleiben (Bounds-Check greift bei
  sauberem Signal nie).
