# ProtoPirate — Kia V2: geladene Aufnahme zeigt Sn/Btn/Cnt als 0 statt der echten Werte

## Was

`protocols/kia_v2.c` (Kia/Hyundai, 53-Bit-Rolling-Code-Protokoll,
`min_count_bit_for_found = 53`) speichert die Anzeigefelder Seriennummer/Taste/Zähler NICHT
nur im rohen `generic.data`, sondern zusätzlich, aus `generic.data` abgeleitet, in
`instance->generic.serial`/`generic.btn`/`generic.cnt`. Genau diese drei Felder liest
`kia_protocol_decoder_v2_get_string()` (die einzige Anzeigefunktion des Decoders):

```c
furi_string_cat_printf(
    output,
    "%s %dbit\r\n"
    "Key:%013llX\r\n"
    "Sn:%08lX Btn:%X\r\n"
    "Cnt:%03lX CRC:%X - %s\r\n",
    instance->generic.protocol_name,
    instance->generic.data_count_bit,
    instance->generic.data,
    instance->generic.serial,   // Sn
    instance->generic.btn,      // Btn
    instance->generic.cnt,      // Cnt
    crc,
    crc_valid ? "OK" : "BAD");
```

Bei einem Live-Empfang werden `serial`/`btn`/`cnt` korrekt in
`kia_protocol_decoder_v2_feed()` gesetzt, sobald 53 Bit vollständig eingesammelt sind:

```c
instance->generic.serial = (uint32_t)((instance->generic.data >> 20) & 0xFFFFFFFF);
instance->generic.btn = (uint8_t)((instance->generic.data >> 16) & 0x0F);

uint16_t raw_count = (uint16_t)((instance->generic.data >> 4) & 0xFFF);
instance->generic.cnt = ((raw_count >> 4) | (raw_count << 8)) & 0xFFF;
```

Beim **Laden einer zuvor gespeicherten Aufnahme** ruft der Decoder aber nur

```c
SubGhzProtocolStatus
    kia_protocol_decoder_v2_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolDecoderKiaV2* instance = context;
    return subghz_block_generic_deserialize_check_count_bit(
        &instance->generic, flipper_format, kia_protocol_v2_const.min_count_bit_for_found);
}
```

auf — ein reiner Ein-Zeilen-Durchreicher zur Stock-Firmware-Hilfsfunktion aus
`<lib/subghz/blocks/generic.h>` (derselbe bereits bei `fiat_v0`/`fiat_v1` bestätigte
Vertrag: sie füllt ausschließlich `"Bit"` → `generic.data_count_bit` und `"Key"` →
`generic.data`, prüft die Bitanzahl, und fasst `generic.serial`/`btn`/`cnt` nicht an —
dieselbe Hilfsfunktion wird unverändert in rund 20 weiteren Decodern in diesem Repo
verwendet, u. a. den bereits bestätigten `fiat_v0`/`fiat_v1`-Fällen). `instance` selbst
kommt frisch aus `malloc()` (`kia_protocol_decoder_v2_alloc()`, kein `calloc()` und kein
expliziter Nullinit von `generic`), `serial`/`btn`/`cnt` bleiben also nach dem Laden auf
ihrem Ausgangswert stehen (praktisch `0`, da der Speicher der App bereits an anderer
Stelle typischerweise genullt ankommt, aber nicht garantiert — siehe Risiko).

Ergebnis: Jede über "Saved" → Datei öffnen (oder jeden anderen Pfad, der eine gespeicherte
`.sub`-Datei via `deserialize()`+`get_string()` anzeigt) angezeigte Kia-V2-Aufnahme zeigt
**immer** `Sn:00000000 Btn:0 Cnt:000` — unabhängig vom tatsächlichen Inhalt der Datei.
`generic.data`/`generic.data_count_bit` selbst sind dabei bereits korrekt geladen (das
`"Key"`-Hexfeld wird von der Stock-Hilfsfunktion richtig gelesen, `Key:` und `CRC:` in
`get_string()` sind beide direkt aus `generic.data` berechnet und daher unbetroffen), nur
die zur Anzeige genutzten abgeleiteten Kopien fehlen. Senden (Emulate) ist davon NICHT
betroffen: der Encoder hat einen eigenen, unabhängigen
`kia_protocol_encoder_v2_deserialize()`-Pfad, der `serial`/`btn`/`cnt` bereits selbst aus
den serialisierten `FF_SERIAL`/`FF_BTN`/`FF_CNT`-Feldern liest und `generic.data` daraus
neu zusammensetzt (Zeilen 201–222 in der Originaldatei) — nur der reine Anzeigepfad über
den *Decoder* ist kaputt.

## Nutzen

Ohne den Fix ist "Details ansehen" für jede gespeicherte Kia-V2-Aufnahme bei Seriennummer,
Taste und Rolling-Zähler nutzlos (zeigt ausnahmslos Nullen), obwohl die Datei selbst intakt
ist und korrekt sendbar bleibt — ein rein kosmetischer, aber irreführender Anzeigefehler,
der beim Sichten/Vergleichen mehrerer gespeicherter Aufnahmen (z. B. um zwei Tastendrücke
desselben Senders am `Cnt`-Rolling-Zähler zu unterscheiden, oder um zu prüfen, ob zwei
Dateien vom selben Sender stammen) den eigentlichen Zweck der Detailansicht komplett
unterläuft. Dieselbe Fehlerklasse wie die bereits bestätigten `fiat_v0`/`fiat_v1`-Funde
bestätigt damit, dass es sich nicht um einen Einzelfall handelt, sondern um ein
wiederkehrendes Muster im Ein-Zeilen-Durchreicher-Deserialize dieses Protokollsatzes.

## Betroffene Dateien

- `protocols/kia_v2.c` — `kia_protocol_decoder_v2_deserialize()`

## Port-/Patch-Skizze

```c
SubGhzProtocolStatus
    kia_protocol_decoder_v2_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolDecoderKiaV2* instance = context;

    SubGhzProtocolStatus ret = subghz_block_generic_deserialize_check_count_bit(
        &instance->generic, flipper_format, kia_protocol_v2_const.min_count_bit_for_found);
    if(ret != SubGhzProtocolStatusOk) return ret;

    // generic.data/generic.data_count_bit sind jetzt korrekt geladen, aber
    // get_string() liest ausschliesslich serial/btn/cnt -- dieselbe
    // Rueckrechnung wie in kia_protocol_decoder_v2_feed() beim Live-Empfang
    // (decode_count_bit == 53 Zweig) wiederherstellen.
    instance->generic.serial = (uint32_t)((instance->generic.data >> 20) & 0xFFFFFFFFULL);
    instance->generic.btn = (uint8_t)((instance->generic.data >> 16) & 0x0F);

    uint16_t raw_count = (uint16_t)((instance->generic.data >> 4) & 0xFFF);
    instance->generic.cnt = ((raw_count >> 4) | (raw_count << 8)) & 0xFFF;

    return SubGhzProtocolStatusOk;
}
```

Keine zusätzlichen Includes nötig — verwendet ausschließlich bereits in der Datei
vorhandene Felder und Operationen (dieselben Ausdrücke wie in
`kia_protocol_decoder_v2_feed()`, Zeilen 348–352 der Originaldatei, dorthin 1:1
übernommen).

## Risiko

Sehr gering. Die Änderung ist rein additiv (leitet drei Werte deterministisch aus dem
bereits korrekt geladenen `generic.data` ab, mit exakt derselben Formel, die der
Live-Decode-Pfad bereits verwendet); der bisherige Rückgabewert/Kontrollfluss bei Fehlern
(`ret != SubGhzProtocolStatusOk`) bleibt unverändert. Kein Einfluss auf den Sendepfad
(Encoder bleibt unverändert) und keinen auf den Live-Empfang (Decoder-`feed()` bleibt
unverändert) — betrifft ausschließlich die Detailanzeige einer geladenen Datei. Als
Nebeneffekt behebt der Fix zusätzlich, dass `serial`/`btn`/`cnt` nach dem Laden nicht mehr
von unbestimmtem `malloc()`-Speicherinhalt abhängen (der Instanz-Speicher wird in
`kia_protocol_decoder_v2_alloc()` nicht explizit genullt) — in der Praxis meist ohnehin 0,
aber ohne Garantie.

## Testhinweis

Lokal in `protocols/kia_v2.c` `kia_protocol_decoder_v2_deserialize()` durch die Fassung
oben ersetzen, bauen/flashen. Dann: ein eigenes Kia/Hyundai-V2-Signal empfangen, über
"Save" sichern, App verlassen und die Datei erneut über "Saved" öffnen — `Sn:`/`Btn:`/
`Cnt:` müssen jetzt exakt die beim Empfang angezeigten Werte zeigen statt durchgehend
`Sn:00000000 Btn:0 Cnt:000`; `Key:`/`CRC:` waren bereits vorher korrekt und müssen
unverändert bleiben. Zusätzlich Host-Repro in
`repro-kia-v2-decoder-deserialize-missing-field-restore/repro.c` (Modell der
Stock-Hilfsfunktion und beider Deserialize-Fassungen, drei Testfälle inkl. Randfällen
`serial=btn=cnt=0`/Maximalwerte, `serial`/`btn`/`cnt` unabhängig aus Ziel-Rohwerten
rückwärts in einen `generic.data`-Wert kodiert statt zirkulär aus der zu prüfenden
Funktion selbst) unter `gcc -O2 -Wall -Wextra -fsanitize=address,undefined` gebaut und
verifiziert: ALT liefert für jede nicht-Null-Aufnahme durchgehend `Sn:00000000 Btn:0
Cnt:000`, NEU liefert in allen Testfällen einen exakten Round-Trip von `serial`/`btn`/
`cnt`.

## Quelle

Eigener Fund beim vollständigen Quelltextaudit von `protocols/kia_v2.c`
(`G4MEOVER18/ProtoPirate`, `main`, Commit `e3d9da5`), vorgemerkt in der vorigen Sitzung
beim Durchsehen der übrigen `ProtoPirate`-Decoder als Verdachtsfall aufgrund des
strukturell identischen Ein-Zeilen-Durchreicher-Deserialize wie bei den bereits
bestätigten `fiat_v0`/`fiat_v1`-Funden — hier durch vollständiges Lesen der Datei und
Abgleich von `get_string()` gegen `feed()`/`deserialize()` einzeln bestätigt statt aus der
Ähnlichkeit pauschal angenommen.
