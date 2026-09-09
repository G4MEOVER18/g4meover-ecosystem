# ProtoPirate — Kia V3/V4: geladene Aufnahme zeigt Sn/Btn/Cnt als Speichermüll statt der echten Werte

## Was

`protocols/kia_v3_v4.c` (Kia/Hyundai KeeLoq-basiertes 64-Bit-Protokoll, zwei Varianten
V3/„is_v3_sync" und V4, `min_count_bit_for_found = 64`) berechnet Seriennummer/Taste/Zähler
bei einem Live-Empfang in `kia_v3_v4_process_buffer()` direkt aus den rohen Frame-Bytes bzw.
dem KeeLoq-entschlüsselten Zähler:

```c
instance->encrypted = encrypted;
instance->decrypted = decrypted;
instance->crc = crc;
instance->generic.serial = serial;      // aus b[4..7] extrahiert (bit-reversed)
instance->generic.btn = btn;            // aus b[7] extrahiert (bit-reversed)
instance->generic.cnt = decrypted & 0xFFFF;
instance->version = instance->is_v3_sync ? 1 : 0;
```

Genau diese drei Felder liest `kia_protocol_decoder_v3_v4_get_string()` (die einzige
Anzeigefunktion) direkt und unverändert:

```c
void kia_protocol_decoder_v3_v4_get_string(void* context, FuriString* output) {
    ...
    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:%08lX%08lX\r\n"
        "Yek:%08lX%08lX\r\n"
        "Serial:%07lX Btn:%01X\r\n"
        "Cnt:%04lX CRC:%01X\r\n",
        ...
        instance->generic.serial,   // Serial
        instance->generic.btn,      // Btn
        instance->generic.cnt,      // Cnt
        instance->crc);
}
```

(`Key:`/`Yek:` sind rein aus `generic.data` berechnet, davon unbetroffen.)

Beim **Laden einer zuvor gespeicherten Aufnahme** liest `kia_protocol_decoder_v3_v4_deserialize()`
aber nur die protokollspezifischen Zusatzfelder — `generic.serial`/`btn`/`cnt` werden an
keiner Stelle gesetzt:

```c
SubGhzProtocolStatus
    kia_protocol_decoder_v3_v4_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolDecoderKiaV3V4* instance = context;

    SubGhzProtocolStatus ret =
        subghz_block_generic_deserialize_check_count_bit(&instance->generic, flipper_format, 64);

    if(ret == SubGhzProtocolStatusOk) {
        uint32_t temp = 0;

        if(flipper_format_read_uint32(flipper_format, "Encrypted", &temp, 1)) {
            instance->encrypted = temp;
        }
        if(flipper_format_read_uint32(flipper_format, "Decrypted", &temp, 1)) {
            instance->decrypted = temp;
        }
        if(flipper_format_read_uint32(flipper_format, "KIAVersion", &temp, 1)) {
            instance->version = (uint8_t)temp;
        }
        if(flipper_format_read_uint32(flipper_format, "CRC", &temp, 1)) {
            instance->crc = (uint8_t)temp;
        }
    }

    return ret;
}
```

Dieselbe Stock-Firmware-Hilfsfunktion `subghz_block_generic_deserialize_check_count_bit`
wie bei den bereits bestätigten `fiat_v0`/`fiat_v1`/`kia_v2`-Funden — sie füllt
ausschließlich `"Bit"` → `generic.data_count_bit` und `"Key"` → `generic.data`, prüft die
Bitanzahl, und fasst `generic.serial`/`btn`/`cnt` nicht an. `pp_encoder_read_fields()`
(die im Repo übliche Funktion zum Nachladen von `Serial:`/`Btn:`/`Cnt:` aus der Datei) wird
in diesem Decoder-`deserialize()` an keiner Stelle aufgerufen, obwohl der zugehörige
`kia_protocol_decoder_v3_v4_serialize()` diese drei Felder beim Speichern durchaus über
`pp_serialize_fields(..., PP_FIELD_SERIAL | PP_FIELD_BTN | PP_FIELD_CNT, ...)` in die Datei
schreibt — sie werden beim Laden schlicht nie wieder eingelesen.

`instance` selbst kommt zudem aus reinem `malloc()` (`kia_protocol_decoder_v3_v4_alloc()`,
kein `calloc()`, kein expliziter Nullinit von `generic`) — anders als beim `kia_v2`-Fund
bleiben `serial`/`btn`/`cnt` nach dem Laden also nicht bei `0` stehen, sondern beim
tatsächlich unveränderten Speicherinhalt der zuletzt dort abgelegten Werte (in der Praxis:
Reste einer vorherigen `SubGhzProtocolDecoderKiaV3V4`-Instanz, bei der ersten Allokation im
Prozess beliebiger `malloc()`-Speichermüll).

Ergebnis: Jede gespeicherte Kia-V3/V4-Aufnahme zeigt nach dem Öffnen über „Saved" bei
Seriennummer, Taste und Zähler **nicht die echten, tatsächlich gespeicherten Werte**,
sondern Speicherinhalt, der nichts mit der Datei zu tun hat — schlimmer als der bereits
behobene `kia_v2`-Fund (dort zumindest verlässlich `0`, hier unvorhersehbar). `generic.data`
selbst ist bereits korrekt geladen (`Key:`/`Yek:` sind davon unbetroffen); nur die drei zur
Anzeige genutzten abgeleiteten Kopien fehlen.

## Nutzen

Ohne den Fix ist „Details ansehen" für jede gespeicherte Kia-V3/V4-Aufnahme bei
Seriennummer, Taste und Rolling-Zähler nutzlos und sogar irreführend (zeigt beliebigen
Speicherinhalt statt der echten Werte oder eines erkennbaren Platzhalters wie `0`), obwohl
die Datei selbst intakt und korrekt sendbar bleibt. Bestätigt zusätzlich, dass der bereits
bei `fiat_v0`/`fiat_v1`/`kia_v2` gefundene Ein-Zeilen-Durchreicher-Deserialize kein
Einzelfall ist, sondern ein wiederkehrendes strukturelles Muster in diesem Protokollsatz —
hier in einer Variante, die zusätzliche protokollspezifische Felder liest, aber
ausgerechnet die drei von `get_string()` gebrauchten vergisst.

## Betroffene Dateien

- `protocols/kia_v3_v4.c` — `kia_protocol_decoder_v3_v4_deserialize()`

## Port-/Patch-Skizze

```c
SubGhzProtocolStatus
    kia_protocol_decoder_v3_v4_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolDecoderKiaV3V4* instance = context;

    SubGhzProtocolStatus ret =
        subghz_block_generic_deserialize_check_count_bit(&instance->generic, flipper_format, 64);

    if(ret == SubGhzProtocolStatusOk) {
        uint32_t temp = 0;

        if(flipper_format_read_uint32(flipper_format, "Encrypted", &temp, 1)) {
            instance->encrypted = temp;
        }
        if(flipper_format_read_uint32(flipper_format, "Decrypted", &temp, 1)) {
            instance->decrypted = temp;
        }
        if(flipper_format_read_uint32(flipper_format, "KIAVersion", &temp, 1)) {
            instance->version = (uint8_t)temp;
        }
        if(flipper_format_read_uint32(flipper_format, "CRC", &temp, 1)) {
            instance->crc = (uint8_t)temp;
        }

        // generic.data/generic.data_count_bit sind jetzt korrekt geladen, aber
        // get_string() liest ausschliesslich serial/btn/cnt -- dieselbe
        // Rueckrechnung wie in kia_v3_v4_process_buffer() beim Live-Empfang
        // wiederherstellen (serial/btn direkt aus den Frame-Bytes, cnt aus
        // dem bereits oben restaurierten Decrypted-Feld).
        uint8_t b[8];
        pp_u64_to_bytes_be(instance->generic.data, b);
        instance->generic.serial = ((uint32_t)pp_reverse_bits8(b[7] & 0xF0) << 24) |
                                   ((uint32_t)pp_reverse_bits8(b[6]) << 16) |
                                   ((uint32_t)pp_reverse_bits8(b[5]) << 8) |
                                   (uint32_t)pp_reverse_bits8(b[4]);
        instance->generic.btn = (uint8_t)((pp_reverse_bits8(b[7]) & 0xF0) >> 4);
        instance->generic.cnt = instance->decrypted & 0xFFFF;
    }

    return ret;
}
```

Keine zusätzlichen Includes nötig — `pp_u64_to_bytes_be()`/`pp_reverse_bits8()` werden in
derselben Datei bereits verwendet (`kia_v3_v4_process_buffer()`), die Rückrechnungsformel
ist 1:1 aus dort übernommen.

## Risiko

Sehr gering. Die Änderung ist rein additiv (leitet zwei der drei Werte deterministisch aus
dem bereits korrekt geladenen `generic.data` ab, mit exakt derselben Formel, die der
Live-Decode-Pfad bereits verwendet; `cnt` aus dem im selben Funktionsaufruf unmittelbar
zuvor restaurierten `Decrypted`-Feld); der bisherige Rückgabewert/Kontrollfluss bei Fehlern
bleibt unverändert. Kein Einfluss auf den Sendepfad (Encoder hat einen eigenen,
unabhängigen `kia_protocol_encoder_v3_v4_deserialize()`) und keiner auf den Live-Empfang
(`kia_v3_v4_process_buffer()`/`feed()` bleiben unverändert) — betrifft ausschließlich die
Detailanzeige einer geladenen Datei. Für Dateien, die (aus welchem Grund auch immer) kein
`"Decrypted"`-Feld enthalten, bleibt `instance->decrypted` unverändert auf seinem
`malloc()`-Ausgangswert stehen und `Cnt:` entsprechend falsch — bei allen bisher von dieser
App selbst gespeicherten Dateien ist das Feld aber immer vorhanden (`serialize()` schreibt
es unbedingt), betrifft also nur von Hand konstruierte/beschädigte Dateien und ist kein
Rückschritt gegenüber dem Status quo.

## Testhinweis

Lokal in `protocols/kia_v3_v4.c` `kia_protocol_decoder_v3_v4_deserialize()` durch die
Fassung oben ersetzen, bauen/flashen. Dann: ein eigenes Kia/Hyundai-V3- oder -V4-Signal
empfangen, über „Save" sichern, App verlassen und die Datei erneut über „Saved" öffnen —
`Serial:`/`Btn:`/`Cnt:` müssen jetzt exakt die beim Empfang angezeigten Werte zeigen statt
willkürlichen Speicherinhalts; `Key:`/`Yek:`/`CRC:` waren bereits vorher korrekt und müssen
unverändert bleiben. Zum Erkennen des Effekts hilft es, vor dem Öffnen der Test-Datei erst
mehrere andere Aufnahmen zu laden (füllt den zuvor freigegebenen Speicher mit erkennbar
anderen Werten), um zu bestätigen, dass ALT tatsächlich fremden Speicherinhalt statt `0`
zeigt.

Zusätzlich Host-Repro in
`repro-kia-v3-v4-decoder-deserialize-missing-field-restore/repro.c` (Modell der reinen
Datentransformation ohne Flipper-Laufzeit/-Dateizugriff: `pp_reverse_bits8()`/
`pp_u64_to_bytes_be()` 1:1 übernommen, ein synthetischer Frame wird aus Ziel-`serial`/`btn`/
`encrypted` rückwärts aufgebaut, ALT/NEU-Deserialize-Fassungen gegen einen absichtlich
„vergifteten" Vor-Zustand (`0xDEADBEEF`/`0xAA`/`0xBEEF` statt `0`, um zu beweisen, dass ALT
wirklich unverändert lässt statt zufällig zu nullen) getestet, vier Fälle inkl. Nullen- und
Maximalwerte-Randfällen) unter `gcc -O2 -Wall -Wextra -fsanitize=address,undefined` gebaut
und verifiziert: ALT liefert in jedem Fall den vergifteten Vor-Zustand unverändert zurück,
NEU liefert in allen Fällen einen exakten Round-Trip von `serial`/`btn`/`cnt`.

## Quelle

Eigener Fund beim fortgesetzten vollständigen Quelltextaudit des restlichen
`ProtoPirate`-Codes (`G4MEOVER18/ProtoPirate`, `main`, Commit `e3d9da5`) — `honda_static.c`,
`honda_v1.c`, `kia_v0.c`, `kia_v1.c` und `subaru.c` in derselben Sitzung ebenfalls
vollständig gegengeprüft (`deserialize()` vs. `get_string()`/Live-Decode-Pfad), dort jeweils
kein Fund dieser Fehlerklasse: alle fünf berechnen die angezeigten Sn/Btn/Cnt-Werte in
`get_string()` bei jedem Aufruf frisch aus `generic.data` (bzw. rufen dieselbe
Rückrechnungsfunktion wie der Live-Decode-Pfad auf), sodass ein unvollständiges
`deserialize()` dort folgenlos bleibt — strukturell identisch zum bereits als „gut"
bestätigten Muster in `kia_v0.c`/`kia_v1.c`. `kia_v3_v4.c` weicht davon ab: `get_string()`
liest `generic.serial`/`btn`/`cnt` direkt und ungeprüft, ohne Rückrechnung — genau das
Muster, das bei `fiat_v0`/`fiat_v1`/`kia_v2` bereits als Fehlerquelle bestätigt wurde.
