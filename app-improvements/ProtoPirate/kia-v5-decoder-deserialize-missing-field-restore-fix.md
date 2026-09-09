# ProtoPirate — Kia V5: geladene Aufnahme zeigt Yek/Sn/Btn/Cnt/CRC als Speichermüll statt der echten Werte

## Was

`protocols/kia_v5.c` (Kia/Hyundai, 64-Bit-Manchester-Protokoll mit eigenem
Stream-Cipher-„Mixer" zur Zähler-Entschlüsselung, `min_count_bit_for_found = 64`) berechnet
bei einem Live-Empfang in `KiaV5DecoderStepData` (`kia_protocol_decoder_v5_feed()`) direkt
nach dem Manchester-Decode alle Anzeigefelder aus den 64 rohen Datenbits:

```c
instance->yek = 0;
for(int i = 0; i < 8; i++) {
    uint8_t byte = (instance->generic.data >> (i * 8)) & 0xFF;
    uint8_t reversed = 0;
    for(int b = 0; b < 8; b++) {
        if(byte & (1 << b)) reversed |= (1 << (7 - b));
    }
    instance->yek |= ((uint64_t)reversed << ((7 - i) * 8));
}
instance->generic.serial = (uint32_t)((instance->yek >> 32) & 0x0FFFFFFF);
instance->generic.btn = (uint8_t)((instance->yek >> 60) & 0x0F);
uint32_t encrypted = (uint32_t)(instance->yek & 0xFFFFFFFF);
instance->generic.cnt = mixer_decode(encrypted);
```

`instance->crc` wird im selben Aufruf aus den zusätzlichen 3 CRC-Bits des 67-Bit-Frames
gesetzt (`instance->crc = (uint8_t)(instance->decoded_data & 0x07);`).

Genau diese fünf Felder liest `kia_protocol_decoder_v5_get_string()` (die einzige
Anzeigefunktion) direkt und unverändert:

```c
void kia_protocol_decoder_v5_get_string(void* context, FuriString* output) {
    ...
    uint32_t yek_hi = (uint32_t)(instance->yek >> 32);
    uint32_t yek_lo = (uint32_t)(instance->yek & 0xFFFFFFFF);

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:%08lX%08lX\r\n"
        "Yek:%08lX%08lX\r\n"
        "Sn:%07lX Btn:%X Cnt:%04lX\r\n"
        "CRC:%X\r\n",
        ...
        yek_hi, yek_lo,
        instance->generic.serial,   // Sn
        instance->generic.btn,      // Btn
        instance->generic.cnt,      // Cnt
        instance->crc);             // CRC
}
```

(`Key:` ist rein aus `generic.data` berechnet, davon unbetroffen.)

Beim **Laden einer zuvor gespeicherten Aufnahme** liest `kia_protocol_decoder_v5_deserialize()`
aber ausschließlich `generic.data`/`generic.data_count_bit`:

```c
SubGhzProtocolStatus
    kia_protocol_decoder_v5_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolDecoderKiaV5* instance = context;
    return subghz_block_generic_deserialize_check_count_bit(
        &instance->generic, flipper_format, kia_protocol_v5_const.min_count_bit_for_found);
}
```

Dieselbe Stock-Firmware-Hilfsfunktion `subghz_block_generic_deserialize_check_count_bit`
wie bei den bereits bestätigten `fiat_v0`/`fiat_v1`/`kia_v2`/`kia_v3_v4`-Funden — sie füllt
ausschließlich `"Bit"` → `generic.data_count_bit` und `"Key"` → `generic.data`, prüft die
Bitanzahl, und fasst `generic.serial`/`btn`/`cnt` nicht an; `instance->yek`/`instance->crc`
sind protokolleigene Felder außerhalb von `generic` und werden hier an keiner Stelle
berührt. `kia_protocol_decoder_v5_serialize()` schreibt `Serial:`/`Btn:`/`Cnt:` (über
`pp_serialize_fields()`), `"CRC"`, `"DataHi"`/`"DataLo"` und `"YekHi"`/`"YekLo"` durchaus in
die Datei — beim Laden wird davon aber nichts wieder eingelesen, weder über
`pp_encoder_read_fields()` noch direkt.

`instance` kommt zudem aus reinem `malloc()`
(`kia_protocol_decoder_v5_alloc()`, Zeile: `SubGhzProtocolDecoderKiaV5* instance =
malloc(sizeof(SubGhzProtocolDecoderKiaV5));` — kein `calloc()`, kein expliziter Nullinit),
sodass `yek`/`crc`/`generic.serial`/`btn`/`cnt` nach dem Laden nicht bei `0` stehen bleiben,
sondern beim tatsächlich unveränderten Speicherinhalt der zuletzt dort abgelegten Werte
(in der Praxis: Reste einer vorherigen Decoder-Instanz bzw. bei der ersten Allokation im
Prozess beliebiger `malloc()`-Speichermüll) — exakt dasselbe Muster wie beim bereits
bestätigten `kia_v3_v4`-Fund, hier aber zusätzlich auf `Yek:` und `CRC:` ausgeweitet.

## Nutzen

Ohne den Fix ist „Details ansehen" für jede gespeicherte Kia-V5-Aufnahme bei **Yek, Sn,
Btn, Cnt und CRC** nutzlos und irreführend (zeigt beliebigen Speicherinhalt statt der
echten, beim Empfang ermittelten Werte oder eines erkennbaren Platzhalters wie `0`), obwohl
die Datei selbst intakt und korrekt sendbar bleibt (`generic.data`/`Key:` sind bereits
korrekt geladen). Bestätigt erneut, dass der bereits bei `fiat_v0`/`fiat_v1`/`kia_v2`/
`kia_v3_v4` gefundene Ein-Zeilen-Durchreicher-Deserialize kein Einzelfall ist, sondern ein
wiederkehrendes strukturelles Muster in diesem Protokollsatz ist — hier in der bisher am
stärksten betroffenen Variante (fünf statt drei Felder).

## Betroffene Dateien

- `protocols/kia_v5.c` — `kia_protocol_decoder_v5_deserialize()`

## Port-/Patch-Skizze

```c
SubGhzProtocolStatus
    kia_protocol_decoder_v5_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolDecoderKiaV5* instance = context;
    SubGhzProtocolStatus ret = subghz_block_generic_deserialize_check_count_bit(
        &instance->generic, flipper_format, kia_protocol_v5_const.min_count_bit_for_found);

    if(ret == SubGhzProtocolStatusOk) {
        // generic.data ist jetzt korrekt geladen, aber get_string() liest
        // instance->yek/generic.serial/btn/cnt, die deserialize() bisher nie
        // gesetzt hat -- dieselbe Rueckrechnung wie KiaV5DecoderStepData in
        // kia_protocol_decoder_v5_feed() wiederherstellen.
        instance->yek = 0;
        for(int i = 0; i < 8; i++) {
            uint8_t byte = (instance->generic.data >> (i * 8)) & 0xFF;
            instance->yek |= ((uint64_t)pp_reverse_bits8(byte) << ((7 - i) * 8));
        }
        instance->generic.serial = (uint32_t)((instance->yek >> 32) & 0x0FFFFFFF);
        instance->generic.btn = (uint8_t)((instance->yek >> 60) & 0x0F);
        instance->generic.cnt = mixer_decode((uint32_t)(instance->yek & 0xFFFFFFFF));

        // CRC gehoert zu den 3 zusaetzlichen Frame-Bits jenseits der 64 Datenbits und
        // laesst sich nicht aus generic.data zurueckrechnen -- serialize() schreibt es
        // bereits als eigenes Feld, hier wieder einlesen.
        uint32_t crc_temp = 0;
        if(flipper_format_read_uint32(flipper_format, "CRC", &crc_temp, 1)) {
            instance->crc = (uint8_t)crc_temp;
        }
    }

    return ret;
}
```

`pp_reverse_bits8()` ist in `protocols_common.h` deklariert und in `kia_v5.c` bereits
mittelbar genutzt (die manuelle Bit-für-Bit-Schleife in `kia_protocol_decoder_v5_feed()`
macht exakt dasselbe); hier direkt die vorhandene Hilfsfunktion statt der Inline-Schleife
verwendet, keine zusätzlichen Includes nötig.

## Risiko

Sehr gering. Die Änderung ist rein additiv: `yek`/`serial`/`btn`/`cnt` werden deterministisch
aus dem bereits korrekt geladenen `generic.data` abgeleitet, mit exakt derselben Formel, die
der Live-Decode-Pfad bereits verwendet (inkl. desselben `mixer_decode()`-Aufrufs mit
demselben `get_kia_v5_key()`-Schlüssel); `crc` wird aus dem von `serialize()` bereits
geschriebenen Feld gelesen. Der bisherige Rückgabewert/Kontrollfluss bei Fehlern bleibt
unverändert. Kein Einfluss auf den Sendepfad (Encoder hat einen eigenen, unabhängigen
`kia_protocol_encoder_v5_deserialize()`, betrifft nur `ENABLE_EMULATE_FEATURE`-Builds) und
keiner auf den Live-Empfang (`kia_protocol_decoder_v5_feed()` bleibt unverändert) — betrifft
ausschließlich die Detailanzeige einer geladenen Datei. Für Dateien ohne `"CRC"`-Feld (von
Hand konstruiert/beschädigt) bleibt `instance->crc` auf seinem `malloc()`-Ausgangswert
stehen — bei allen von dieser App selbst gespeicherten Dateien ist das Feld immer vorhanden
(`serialize()` schreibt es unbedingt), also kein Rückschritt gegenüber dem Status quo.

## Testhinweis

Lokal in `protocols/kia_v5.c` `kia_protocol_decoder_v5_deserialize()` durch die Fassung oben
ersetzen, bauen/flashen. Dann: ein eigenes Kia-V5-Signal empfangen, über „Save" sichern, App
verlassen und die Datei erneut über „Saved" öffnen — `Yek:`/`Sn:`/`Btn:`/`Cnt:`/`CRC:` müssen
jetzt exakt die beim Empfang angezeigten Werte zeigen statt willkürlichen Speicherinhalts;
`Key:` war bereits vorher korrekt und muss unverändert bleiben. Zum Erkennen des Effekts
hilft es, vor dem Öffnen der Test-Datei erst mehrere andere Aufnahmen zu laden (füllt den
zuvor freigegebenen Speicher mit erkennbar anderen Werten), um zu bestätigen, dass ALT
tatsächlich fremden Speicherinhalt statt `0` zeigt.

Zusätzlich Host-Repro in
`repro-kia-v5-decoder-deserialize-missing-field-restore/repro.c` (reine
Datentransformation ohne Flipper-Laufzeit/-Dateizugriff: `pp_reverse_bits8()`/
`mixer_decode()`/`build_keystore_from_mfkey()` 1:1 aus `protocols/kia_v5.c` übernommen,
`get_kia_v5_key()` durch einen festen Test-Schlüssel ersetzt (Original liest ihn aus dem
Flipper-Schlüsselspeicher, für den Fund selbst irrelevant); ALT/NEU-Deserialize-Fassungen
gegen einen absichtlich „vergifteten" Vor-Zustand statt gegen `0` getestet, vier Fälle
inkl. Nullen- und Maximalwerte-Randfällen, NEU zusätzlich gegen einen unabhängigen Nachbau
der Live-Decode-Rückrechnung verglichen) unter
`gcc -O2 -Wall -Wextra -fsanitize=address,undefined` gebaut und verifiziert: ALT liefert in
jedem Fall den vergifteten Vor-Zustand unverändert zurück (unabhängig vom geladenen
`generic.data`), NEU liefert in allen vier Fällen einen exakten Round-Trip zum
Live-Decode-Ergebnis desselben Frames.

## Quelle

Eigener Fund beim fortgesetzten vollständigen Quelltextaudit des restlichen
`ProtoPirate`-Codes (`G4MEOVER18/ProtoPirate`, `main`, Commit `e3d9da5`) — `kia_v6.c` und
`kia_v7.c` in derselben Sitzung ebenfalls durchgesehen, dort jeweils kein Fund dieser
Fehlerklasse: `kia_v6`s `deserialize()` liest `Key_2`/`Key_3`/`Key_4`/`Fx` sowie über
`pp_encoder_read_fields()` `Serial:`/`Btn:`/`Cnt:` bereits vollständig aus der Datei zurück
(`serialize()` schreibt alle diese Felder unbedingt); `kia_v7`s `get_string()` ruft vor jeder
Ausgabe `kia_v7_decode_key_decoder()` auf, was Sn/Btn/Cnt/CRC bei jedem Aufruf frisch aus
`generic.data` zurückrechnet — strukturell identisch zum bereits als „gut" bestätigten
Selbstheilungs-Muster in `kia_v7`s eigenem `serialize()` sowie in `mazda_v0.c`.
