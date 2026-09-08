# ProtoPirate — Fiat V0: geladene Aufnahme zeigt Key/Hop/Sn/EndByte als 0 statt der echten Werte

## Was

`protocols/fiat_v0.c` (Fiat/Lancia, älteres 71-Bit-Protokoll, `min_count_bit_for_found = 64`,
Payload `Key = Hop(32) | Fix(32)` plus separates 7-Bit `EndByte`-Feld) speichert die
angezeigten Werte NICHT in `instance->generic.data`/`generic.serial`/`generic.btn`/
`generic.cnt`, sondern ausschließlich in drei eigenen Decoder-Struct-Feldern:
`instance->hop`, `instance->fix`, `instance->endbyte`. Genau diese drei Felder liest
`subghz_protocol_decoder_fiat_v0_get_string()` (die einzige Anzeigefunktion des Decoders)
für **jede** ausgegebene Zeile:

```c
furi_string_cat_printf(
    output,
    "%s %dbit\r\n"
    "Key:%08lX%08lX\r\n"
    "Hop:%08lX\r\n"
    "Sn:%08lX\r\n"
    "EndByte:%02X\r\n",
    instance->generic.protocol_name,
    instance->generic.data_count_bit,
    instance->hop, instance->fix,   // Key
    instance->hop,                  // Hop
    instance->fix,                  // Sn
    instance->endbyte & 0x3F);      // EndByte
```

Bei einem Live-Empfang werden `hop`/`fix`/`endbyte` korrekt in
`fiat_v0_finish_packet()`/`subghz_protocol_decoder_fiat_v0_feed()` gesetzt. Beim **Laden
einer zuvor gespeicherten Aufnahme** ruft der Decoder aber nur

```c
SubGhzProtocolStatus
    subghz_protocol_decoder_fiat_v0_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolDecoderFiatV0* instance = context;
    return subghz_block_generic_deserialize_check_count_bit(
        &instance->generic, flipper_format, subghz_protocol_fiat_v0_const.min_count_bit_for_found);
}
```

auf — ein reiner Ein-Zeilen-Durchreicher zur Stock-Firmware-Hilfsfunktion aus
`<lib/subghz/blocks/generic.h>`. Diese Hilfsfunktion liest laut ihrem eigenen, engen
Vertrag ausschließlich die Felder `"Bit"` (→ `generic.data_count_bit`) und `"Key"` (→
`generic.data`, als 64-Bit-Hex) und prüft die Bitanzahl — sie fasst `generic.serial`/
`generic.btn`/`generic.cnt` nicht an und kennt die protokolleigenen Felder `hop`/`fix`/
`endbyte` von `SubGhzProtocolDecoderFiatV0` naturgemäß gar nicht (dieselbe Hilfsfunktion
wird unverändert in rund 20 weiteren Decodern in diesem Repo verwendet). `instance` selbst
kommt frisch aus `calloc()` (`subghz_protocol_decoder_fiat_v0_alloc()`), `hop`/`fix`/
`endbyte` bleiben also nach dem Laden schlicht `0`.

Ergebnis: Jede über "Saved" → Datei öffnen (oder jeden anderen Pfad, der eine
gespeicherte `.sub`-Datei via `deserialize()`+`get_string()` anzeigt, z. B. Sub-Decode-
Protokollerkennung) angezeigte Fiat-V0-Aufnahme zeigt **immer**
`Key:0000000000000000`, `Hop:00000000`, `Sn:00000000`, `EndByte:00` — unabhängig vom
tatsächlichen Inhalt der Datei. `generic.data`/`generic.data_count_bit` selbst sind dabei
bereits korrekt geladen (das eigentliche `"Key"`-Hexfeld wird von der Stock-Hilfsfunktion
richtig gelesen), nur die zur Anzeige genutzten Kopien fehlen. Senden (Emulate) ist davon
NICHT betroffen: der Encoder hat einen eigenen, unabhängigen
`subghz_protocol_encoder_fiat_v0_deserialize()`-Pfad, der `hop`/`fix`/`endbyte` bereits
selbst aus `"Key"`/`"EndByte"`/`"Btn"` rekonstruiert (Zeilen 246–266 in der Originaldatei)
— nur der reine Anzeigepfad über den *Decoder* ist kaputt.

## Nutzen

Ohne den Fix ist "Details ansehen" für jede gespeicherte Fiat-V0-Aufnahme nutzlos (zeigt
ausnahmslos Nullen statt Hop/Seriennummer/EndByte), obwohl die Datei selbst intakt ist und
korrekt sendbar bleibt — ein rein kosmetischer, aber irreführender Anzeigefehler, der beim
Sichten/Vergleichen mehrerer gespeicherter Aufnahmen (z. B. um zwei Tastendrücke am
Rolling-Counter `Hop` zu unterscheiden) den eigentlichen Zweck der Detailansicht komplett
unterläuft.

## Betroffene Dateien

- `protocols/fiat_v0.c` — `subghz_protocol_decoder_fiat_v0_deserialize()`

## Port-/Patch-Skizze

```c
SubGhzProtocolStatus
    subghz_protocol_decoder_fiat_v0_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolDecoderFiatV0* instance = context;

    SubGhzProtocolStatus ret = subghz_block_generic_deserialize_check_count_bit(
        &instance->generic, flipper_format, subghz_protocol_fiat_v0_const.min_count_bit_for_found);
    if(ret != SubGhzProtocolStatusOk) return ret;

    // generic.data/generic.data_count_bit sind jetzt korrekt geladen, aber
    // get_string() liest ausschliesslich hop/fix/endbyte -- dieselbe Aufteilung
    // wie in fiat_v0_finish_packet() beim Live-Empfang wiederherstellen.
    instance->hop = (uint32_t)(instance->generic.data >> 32);
    instance->fix = (uint32_t)(instance->generic.data & 0xFFFFFFFFU);

    // EndByte ist kein Teil von generic.data (nur Hop/Fix = 64 der 71 Bit) --
    // genau denselben EndByte-oder-Btn-Fallback lesen, den
    // subghz_protocol_encoder_fiat_v0_deserialize() fuer denselben Zweck bereits nutzt.
    uint32_t eb_read = 0;
    flipper_format_rewind(flipper_format);
    bool have_endbyte = flipper_format_read_uint32(flipper_format, "EndByte", &eb_read, 1);
    if(have_endbyte) {
        instance->endbyte = (uint8_t)(eb_read & 0x7FU);
    } else {
        uint32_t btn_read = 0;
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, FF_BTN, &btn_read, 1)) {
            instance->endbyte = (uint8_t)(btn_read & 0x7FU);
        }
    }

    // generic.serial/btn/cnt im selben Zug mitziehen (haelt beide Ansichten
    // konsistent mit dem, was ein Live-Decode ueber fiat_v0_finish_packet() setzt).
    instance->generic.serial = instance->fix;
    instance->generic.btn = instance->endbyte;
    instance->generic.cnt = instance->hop;

    return SubGhzProtocolStatusOk;
}
```

`FF_BTN` ist bereits über das ohnehin eingebundene `protocols_common.h` sichtbar (siehe
`FF_SERIAL`/`FF_BTN`/`FF_CNT`-Deklarationen dort), keine zusätzlichen Includes nötig. Der
`"EndByte"`-Fallback auf `FF_BTN` ("Btn") ist unverändert aus dem bereits vorhandenen
Encoder-Pfad in derselben Datei übernommen (dort mit identischer Begründung: ältere/fremde
Dateien könnten nur `"Btn"` statt `"EndByte"` tragen) — `fiat_v0`s eigener `serialize()`
schreibt beide Felder immer mit demselben Wert, der Fallback greift also nur bei extern
erzeugten oder handgefertigten Dateien.

## Risiko

Sehr gering. Die Änderung ist rein additiv (liest zwei zusätzliche, bereits im eigenen
`serialize()` geschriebene Felder zurück und leitet zwei Werte deterministisch aus dem
bereits korrekt geladenen `generic.data` ab); der bisherige Rückgabewert/Kontrollfluss bei
Fehlern (`ret != SubGhzProtocolStatusOk`) bleibt unverändert. Kein Einfluss auf den
Sendepfad (Encoder bleibt unverändert) und keinen auf den Live-Empfang (Decoder-`feed()`
bleibt unverändert) — betrifft ausschließlich die Detailanzeige einer geladenen Datei.

## Testhinweis

Lokal in `protocols/fiat_v0.c` `subghz_protocol_decoder_fiat_v0_deserialize()` durch die
Fassung oben ersetzen, bauen/flashen. Dann: ein eigenes Fiat/Lancia-V0-Signal empfangen,
über "Save" sichern, App verlassen und die Datei erneut über "Saved" öffnen — `Key:`/
`Hop:`/`Sn:`/`EndByte:` müssen jetzt exakt die beim Empfang angezeigten Werte zeigen statt
durchgehend Nullen. Zusätzlich Host-Repro in
`repro-fiat-v0-decoder-deserialize-missing-field-restore/repro.c` (Modell der
Stock-Hilfsfunktion und beider Deserialize-Fassungen, unter
`gcc -O2 -Wall -Wextra -fsanitize=address,undefined` gebaut) verifiziert: ALT liefert für
jede nicht-Null-Aufnahme durchgehend Nullen in Key/Hop/Sn/EndByte, NEU liefert in allen
Testfällen (inkl. Randfällen `0x00000000`/`0xFFFFFFFF`) exakte Round-Trips.

## Quelle

Eigener Fund beim vollständigen Quelltextaudit von `protocols/fiat_v0.c`
(`G4MEOVER18/ProtoPirate`, `main`, Commit `e3d9da5`) im Rahmen der laufenden
ProtoPirate-Restcode-Durchsicht, ausgelöst durch den strukturellen Vergleich mit dem
zuvor gefundenen `fiat_v1.c`-Speicher/Lade-Bug (dort verlor das Laden Rohdaten-Bytes;
hier verliert das Laden die zur Anzeige genutzten Felder komplett) — derselbe
"Decoder-`deserialize()` vergisst protokolleigene Felder, die `get_string()` aber
braucht"-Fehlerklasse, hier erstmals für `fiat_v0` bestätigt.
