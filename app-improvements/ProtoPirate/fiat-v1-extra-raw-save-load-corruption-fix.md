# ProtoPirate — Fiat/Lancia V1 (Magneti Marelli): jede gespeicherte Aufnahme verliert beim Neuladen zwei der 13 Rohdaten-Byte

## Was
Der Fiat/Lancia-Decoder für das Magneti-Marelli-BSI-Protokoll (`protocols/fiat_v1.c`, PCF7946,
Panda/Grande Punto u. a.) hält die vollständigen 104 empfangenen Bit als 13-Byte-Rohpuffer
(`raw_data[13]`) — der beim Live-Empfang bitweise korrekt gefüllt wird und aus dem
`get_string()` seine Anzeige direkt liest. Für das Speichern/Laden einer Aufnahme (Datei)
existiert daneben ein zweiter, redundanter Zwischenspeicher: die 40 Bit ab Byte 8
(`Bytes 8-12: Encrypted payload`) werden zusätzlich bitweise in ein `uint32_t extra_data`
gepackt und nur dieses (plus die Bitanzahl) in die Datei geschrieben:

```c
// subghz_protocol_decoder_fiat_marelli_feed(), pro empfangenem Bit ab Index 64:
instance->extra_data = (instance->extra_data << 1) | new_bit;   // 40 Schiebe-Operationen

// serialize():
flipper_format_write_uint32(flipper_format, "Extra", &instance->extra_data, 1);
uint32_t extra_bits = instance->generic.data_count_bit - 64;    // = 40
flipper_format_write_uint32(flipper_format, "Extra_bits", &extra_bits, 1);

// fiat_marelli_rebuild_raw_data(), beim Laden aufgerufen:
for(uint8_t i = 0; i < extra_bits && i < 32; i++) {             // extra_bits=40, aber Cap bei 32!
    ...
    if(instance->extra_data & (1UL << (extra_bits - 1 - i))) { ... }
}
```

Zwei voneinander unabhängige Fehler treffen hier zusammen:

1. **`extra_data` ist zu schmal für die 40 Bit, die hineingeschoben werden.** `uint32_t` fasst
   nur 32 Bit; nach 40 Schiebe-Operationen sind die zuerst eingeschobenen 8 Bit (= Byte 8, das
   erste Byte der verschlüsselten Nutzlast) am oberen Ende bereits herausgeschoben und für
   immer verloren — und zwar schon beim Empfang, bevor überhaupt gespeichert wird.
2. **Der Wiederherstellungs-Loop beim Laden ist zusätzlich auf 32 Iterationen gedeckelt**
   (`i < extra_bits && i < 32`), obwohl `extra_bits` (40) an dieser Stelle korrekt vorliegt.
   Byte 12 — das letzte der 13 Byte und genau das CRC-Byte — liegt bei Bit-Index 32-39 dieses
   Teilbereichs und wird dadurch nie beschrieben; es bleibt beim durch `memset()` genullten
   Anfangswert `0x00` stehen, unabhängig vom ersten Fehler.

Mit einem Host-Repro verifiziert (Bit-Packing/Wiederherstellung 1:1 aus der Datei
übernommen, 13-Byte-Testframe mit erkennbarem Muster in den Payload-Bytes durch den
kompletten Empfangs→Speichern→Laden-Zyklus geschickt): **Byte 8 und Byte 12 sind nach jedem
Speichern+Neuladen einer Aufnahme `0x00`**, unabhängig vom tatsächlichen Inhalt — Byte 0-7
und 9-11 sind korrekt, weil sie von den beiden Fehlern nicht betroffen sind.

## Nutzen
- Betrifft den Standard-Arbeitsablauf "Signal empfangen → speichern → später erneut öffnen",
  nicht nur einen Randfall — jede gespeicherte Fiat-V1-Aufnahme ist beim Neuladen betroffen.
- Nach dem Neuladen zeigt `get_string()` in der Detailansicht:
  - `Enc:` mit permanent falschem ersten Byte (`00` statt des echten Werts) statt der
    tatsächlichen verschlüsselten Nutzlast,
  - `CRC:` mit permanent falschem gespeicherten Wert (`00`) und, da die CRC über die
    (jetzt ebenfalls falschen) Bytes 0-11 neu berechnet und mit diesem `00` verglichen wird,
    praktisch immer `[FAIL]` — obwohl die ursprünglich empfangene Aufnahme vollständig gültig
    war. Das sieht wie eine beschädigte oder gefälschte Aufnahme aus, ist aber ein reiner
    Lade-Fehler.
  - `Sn:`/`Cnt:`/`Btn:`/`Ep:` bleiben korrekt, da sie ausschließlich aus Byte 2-7 stammen.
- Verhindert außerdem jede künftige Weiterverwendung der geladenen Aufnahme, die den vollen
  13-Byte-Rohframe braucht (z. B. eine spätere Sendefunktion — aktuell ist der Encoder für
  dieses Protokoll noch `NULL`/nicht implementiert, der Fehler liegt also aktuell "nur" auf dem
  Anzeigepfad, würde aber jede künftige Replay-Implementierung von Anfang an mit kaputten
  Frames füttern).

## Betroffene Dateien
- `protocols/fiat_v1.c` — `fiat_marelli_rebuild_raw_data()`,
  `subghz_protocol_decoder_fiat_marelli_serialize()`,
  `subghz_protocol_decoder_fiat_marelli_deserialize()`.

Keine Änderung an der Empfangs-Zustandsmaschine (`subghz_protocol_decoder_fiat_marelli_feed()`),
an der CRC-Prüfung oder an `fiat_marelli_try_recover_tail_bits()` nötig — `raw_data[]` ist
während des Live-Empfangs bereits durchgehend korrekt befüllt (jedes Bit wird direkt und
unabhängig von `extra_data` hineingeschrieben); der Fehler entsteht ausschließlich auf dem
Speichern/Laden-Pfad.

## Patch
Der Fix ersetzt das verlustbehaftete Bit-Packing durch das direkte Schreiben/Lesen der 5
Rohbyte (`raw_data[8..12]`) als Hex-Feld — dasselbe Muster, das ich im eng verwandten
`ford_v2.c` (`"TailRaw"`) bereits für einen vergleichbaren Rohbyte-Rest verwende, statt der
hier verwendeten, zu schmalen `uint32_t`-Zwischendarstellung:

```c
static void fiat_marelli_rebuild_raw_data(
    SubGhzProtocolDecoderFiatMarelli* instance,
    const uint8_t* extra_raw,
    uint8_t extra_raw_len) {
    memset(instance->raw_data, 0, sizeof(instance->raw_data));

    uint64_t key = instance->generic.data;
    for(uint8_t i = 0; i < 8; i++) {
        instance->raw_data[i] = (uint8_t)(key >> (56 - i * 8));
    }

    for(uint8_t i = 0; i < extra_raw_len && (8U + i) < sizeof(instance->raw_data); i++) {
        instance->raw_data[8 + i] = extra_raw[i];
    }

    // Konsistent mit dem, was der 32-Bit-Akkumulator einer Live-Aufnahme enthält
    // (die letzten 32 der 40 Restbit, also raw_data[9..12]), damit get_hash_data()
    // eine neu geladene Aufnahme genauso dedupliziert wie eine frisch empfangene.
    instance->extra_data = ((uint32_t)instance->raw_data[9] << 24) |
                           ((uint32_t)instance->raw_data[10] << 16) |
                           ((uint32_t)instance->raw_data[11] << 8) |
                           (uint32_t)instance->raw_data[12];

    instance->bit_count = instance->generic.data_count_bit;

    if(instance->bit_count >= 56) {
        instance->generic.serial =
            ((uint32_t)instance->raw_data[2] << 24) | ((uint32_t)instance->raw_data[3] << 16) |
            ((uint32_t)instance->raw_data[4] << 8) | ((uint32_t)instance->raw_data[5]);
        instance->generic.btn = (instance->raw_data[6] >> 4) & 0x0F;
        instance->generic.cnt = (instance->raw_data[7] >> 3) & 0x1F;
    }
}
```

`serialize()` — `"Extra"`/`"Extra_bits"` durch ein einziges 5-Byte-Hex-Feld ersetzt:

```c
    if(ret == SubGhzProtocolStatusOk) {
        flipper_format_write_hex(flipper_format, "ExtraRaw", &instance->raw_data[8], 5);

        uint32_t te = instance->te_detected;
        flipper_format_write_uint32(flipper_format, "TE", &te, 1);
        ...
```

`deserialize()` — liest die 5 Rohbyte direkt statt sie aus `extra_data` zu rekonstruieren:

```c
        uint8_t extra_raw[5] = {0};
        flipper_format_read_hex(flipper_format, "ExtraRaw", extra_raw, sizeof(extra_raw));

        uint32_t te = 0;
        if(flipper_format_read_uint32(flipper_format, "TE", &te, 1)) {
            instance->te_detected = te;
        }

        fiat_marelli_rebuild_raw_data(instance, extra_raw, sizeof(extra_raw));
```

`deserialize()` prüft weiterhin vorab `data_count_bit == FIAT_MARELLI_MAX_DATA_BITS` (104) und
bricht sonst mit `SubGhzProtocolStatusErrorValueBitCount` ab — ein unvollständiger Rest-Fall
(weniger als 5 volle Byte) kommt an dieser Stelle also nie vor, ein fester 5-Byte-Puffer statt
einer Bitanzahl-Verfolgung ist hier bereits ausreichend und einfacher.

## Testhinweis (ohne Hardware)
Reine Bit-/Byte-Logik ohne Flipper-/`SubGhz`-Laufzeitabhängigkeiten, vollständig host-
nachvollziehbar. Zwei Reproduktionsprogramme in diesem Ordner:

```
gcc -O2 -Wall -Wextra -fsanitize=address,undefined -o repro_before repro-fiat-v1-extra-raw-fix/repro_before.c && ./repro_before
gcc -O2 -Wall -Wextra -fsanitize=address,undefined -o repro_after  repro-fiat-v1-extra-raw-fix/repro_after.c  && ./repro_after
```

`repro_before.c` übernimmt das ungeänderte Bit-Packing/die ungeänderte Wiederherstellung
1:1 aus `fiat_v1.c`, schickt einen 13-Byte-Testframe mit erkennbarem Muster in den
Payload-Bytes (`0x11 0x22 0x33 0x44 0x55` für `raw_data[8..12]`) durch den vollen
Empfangen→Speichern→Laden-Zyklus und vergleicht Byte für Byte gegen das Original — Ergebnis:
Byte 8 und Byte 12 sind nach dem Zyklus `0x00` statt `0x11`/`0x55` (2 von 13 Byte falsch).
`repro_after.c` führt denselben Zyklus mit der gepatchten Logik aus — Ergebnis: alle 13 Byte
stimmen exakt überein, `extra_data` (für `get_hash_data()`) ergibt weiterhin denselben Wert
wie eine Live-Aufnahme.

Funktionaler Test danach lokal auf dem Gerät: ein eigenes Fiat/Lancia-Signal (Panda oder
Grande Punto, je nach TE-Variante) empfangen, speichern, App neu starten (oder das Signal aus
der Historie/Datei erneut öffnen) und die Detailansicht vergleichen — vor dem Fix zeigt
`Enc:` ein führendes `00`-Byte und `CRC:` `[FAIL]`, nach dem Fix stimmen `Enc:`/`CRC:` mit der
ursprünglichen Live-Ansicht überein (`[OK]`).

## Risiko
- Ändert das Dateiformat für dieses Protokoll: `"Extra"`/`"Extra_bits"` werden nicht mehr
  geschrieben, `"ExtraRaw"` (5-Byte-Hex) kommt neu hinzu. Bereits **vor** diesem Fix
  gespeicherte Fiat-V1-Aufnahmen enthalten in ihrem `"Extra"`-Feld ohnehin schon die durch den
  ersten Fehler unwiderruflich verstümmelten Daten (Byte 8 fehlt bereits beim Schreiben) — beim
  Laden einer alten Datei mit dem gepatchten Code fehlt `"ExtraRaw"` schlicht, `extra_raw[]`
  bleibt bei der Null-Initialisierung, Byte 8-12 werden `0x00` (identisch zum bisherigen
  Fehlerbild dieser alten Datei, keine Verschlechterung). Für alle **ab jetzt neu**
  gespeicherten Aufnahmen ist der Rundlauf verlustfrei.
- Kein Sicherheits-/Missbrauchsrisiko: reine Lese-/Schreib-Byte-Logik, keine neue
  Speicherzugriffs- oder Bounds-Problematik (`extra_raw_len` wird zusätzlich gegen
  `sizeof(instance->raw_data)` abgesichert).

## Quelle
- `G4MEOVER18/ProtoPirate` (main, `e3d9da5`): `protocols/fiat_v1.c` (Fiat/Lancia V1,
  Magneti-Marelli-BSI-Protokoll); Vorbild für das gewählte Speicherformat:
  `protocols/ford_v2.c` (`"TailRaw"`-Hex-Feld für einen strukturell gleichartigen
  Rohbyte-Rest).
