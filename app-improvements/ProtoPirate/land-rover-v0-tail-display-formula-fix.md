# ProtoPirate — Land Rover V0: "Tail:"-Anzeige spiegelt nicht den tatsächlich geladenen Wert

## Was

`protocols/land_rover_v0.c` (Land Rover, 81-Bit-Rolling-Code-Protokoll mit 16-Bit-Tail-Feld
plus einem abschließenden "Extra"-Terminierungsbit) zeigt in
`subghz_protocol_decoder_land_rover_v0_get_string()` den Tail-Wert nicht direkt an, sondern
über eine eigenartige Rekonstruktion:

```c
furi_string_cat_printf(
    output,
    "%s %dbit\r\n"
    "Key:%016llX\r\n"
    "Sn:%06lX  Btn:%02X - %s\r\n"
    "BtnSig:%06lX\r\n"
    "Cnt:%05lX  Chk:%02X [%s]  Tail:%05lX [%s]\r\n",
    ...
    (unsigned long)(((instance->tail >> 15) & 1U) ? 0x1FFFFUL : 0x0FFFFUL),
    instance->tail_ok ? "OK" : "BAD");
```

Statt `instance->tail` (bzw. den tatsächlich empfangenen/geladenen 16 Tail-Bits plus dem
Extra-Bit) auszugeben, wird nur Bit 15 von `instance->tail` betrachtet und daraus einer von
genau zwei fest kodierten 17-Bit-Werten gewählt (`0x1FFFF` oder `0x0FFFF`). Das ist keine
willkürliche Zahl: `land_rover_v0_calculate_tail()` liefert im Live-Decode-Pfad ausschließlich
`0xFFFF` oder `0x7FFF` als Tail, und der volle Tail-plus-Extra-Bit-Bereich eines gültigen
Frames ist `(tail << 1) | 1` (das Extra-Bit ist bei einem gültigen Frame immer 1, siehe
`land_rover_v0_validate_frame()`: `tail_ok = (tail == expected_tail) && extra_bit`) — für
`tail=0xFFFF` ergibt das `0x1FFFF`, für `tail=0x7FFF` ergibt das `0xFFFF` (`%05lX`-Anzeige:
`0x0FFFF`). Die aktuelle Zeile trifft also für die beiden einzigen Werte, die
`instance->tail` nach einem **erfolgreichen Live-Empfang** je annehmen kann, zufällig den
richtigen 17-Bit-Wert — aber über den Umweg eines aus dem MSB geratenen Ergebnisses statt
`instance->tail` und `instance->extra_bit` tatsächlich zu verwenden.

Der Unterschied wird beim **Laden einer gespeicherten Datei** sichtbar. In
`subghz_protocol_decoder_land_rover_v0_deserialize()` werden `instance->tail` und
`instance->extra_bit` direkt und bedingungslos aus den optionalen Datei-Feldern `"Tail"`/
`"ExtraBit"` übernommen (mit Fallback auf die berechneten Werte, falls die Felder fehlen):

```c
if(flipper_format_read_uint32(flipper_format, LAND_ROVER_V0_FF_TAIL, &temp, 1)) {
    instance->tail = temp & 0xFFFFU;
} else {
    ...
}
...
if(flipper_format_read_uint32(flipper_format, LAND_ROVER_V0_FF_EXTRA_BIT, &temp, 1)) {
    instance->extra_bit = (temp & 1U) != 0;
} else {
    instance->extra_bit = true;
}

land_rover_v0_validate_frame(
    instance->key, instance->tail, instance->extra_bit,
    &instance->check_ok, &instance->tail_ok);
```

`land_rover_v0_validate_frame()`s Rückgabewert wird hier **nicht** geprüft (nur die
Out-Parameter `check_ok`/`tail_ok` werden übernommen) — anders als im Live-Empfangspfad
(`land_rover_v0_finish_frame()`), der bei ungültigem Frame früh mit `return false` abbricht,
*bevor* `instance->tail` überhaupt gesetzt wird. Beim Laden bleibt `instance->tail` also auch
dann auf dem Datei-Wert stehen, wenn dieser nicht mit einem der beiden kanonischen
`0x7FFF`/`0xFFFF`-Werte übereinstimmt oder `extra_bit` `false` ist (`tail_ok` zeigt das
korrekt als `[BAD]` an) — genau die Fälle, in denen die Anzeige interessant wäre, weil sie
von der Norm abweichen. Betroffen sind zum einen Dateien mit bewusst oder versehentlich
abweichendem `"Tail"`/`"ExtraBit"`-Feld (manuell bearbeitete/erstellte Testdateien — im
eigenen autorisierten Pentest-/CTF-Rahmen ein regulärer Anwendungsfall, um z. B. die
Tail-Validierung selbst zu prüfen — oder ein beschädigtes/fremdes File), zum anderen jeder
zukünftige Code-Pfad, der eine Instanz mit einem nicht-kanonischen Tail-Zustand befüllt. Die
aktuelle Formel ignoriert `instance->extra_bit` komplett und rundet jeden `instance->tail`-Wert
auf einen von nur zwei Ausgaben — die angezeigte Hex-Zahl neben `Tail:` entspricht dann nicht
dem tatsächlich geladenen Zustand, obwohl `[OK]`/`[BAD]` daneben bereits korrekt ist.

## Nutzen

Die `Tail:`-Zeile in der Detailansicht soll den tatsächlich empfangenen/geladenen
Tail-Bereich zeigen (so wie `Key:` unverändert `instance->key` zeigt) — aktuell zeigt sie
stattdessen einen von zwei geratenen Werten, unabhängig vom echten Inhalt. Für die beiden
einzigen Zustände, die ein per Funk erfolgreich empfangenes Signal je erreicht, ist das
zufällig deckungsgleich mit dem korrekten Wert, macht das Feld dort also nicht falsch, aber
auch redundant zur bereits vorhandenen `[OK]`/`[BAD]`-Kennzeichnung. Für jede geladene Datei
mit abweichendem Tail-Zustand ist die Zahl daneben irreführend: sie suggeriert einen der zwei
"bekannten" Werte, obwohl der tatsächliche Inhalt ein anderer ist — bei der Detailprüfung
einer verdächtigen/bearbeiteten `.sub`-Datei (Ziel dieser App: Analyse, Vergleich, gezieltes
Testen von Protokoll-Edgecases) verschleiert das gerade den Fall, den man sehen möchte.

## Betroffene Dateien

- `protocols/land_rover_v0.c` — `subghz_protocol_decoder_land_rover_v0_get_string()`

## Port-/Patch-Skizze

```c
void subghz_protocol_decoder_land_rover_v0_get_string(void* context, FuriString* output) {
    furi_check(context);
    SubGhzProtocolDecoderLandRoverV0* instance = context;

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:%016llX\r\n"
        "Sn:%06lX  Btn:%02X - %s\r\n"
        "BtnSig:%06lX\r\n"
        "Cnt:%05lX  Chk:%02X [%s]  Tail:%05lX [%s]\r\n",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        (unsigned long long)instance->key,
        (unsigned long)instance->serial,
        instance->button,
        land_rover_v0_button_name(instance->button),
        (unsigned long)instance->command_signature,
        (unsigned long)instance->count,
        instance->check,
        instance->check_ok ? "OK" : "BAD",
        // vorher: ((instance->tail >> 15) & 1U) ? 0x1FFFFUL : 0x0FFFFUL
        // -- geraten aus Bit 15 von tail, ignoriert extra_bit komplett.
        (((unsigned long)instance->tail) << 1) | (instance->extra_bit ? 1UL : 0UL),
        instance->tail_ok ? "OK" : "BAD");
}
```

Für die beiden kanonischen Live-Empfangs-Zustände (`tail=0xFFFF`/`extra_bit=1` und
`tail=0x7FFF`/`extra_bit=1`) liefert die neue Zeile exakt dieselbe Ausgabe wie zuvor
(`0x1FFFF` bzw. `0x0FFFF`) — reine Verhaltensänderung nur für Zustände, die vorher falsch
dargestellt wurden.

## Risiko

Sehr gering. Rein kosmetische Änderung an einer einzelnen `printf`-Formatargument-Berechnung
in der Detailanzeige; kein Einfluss auf Empfang, Validierung (`check_ok`/`tail_ok` bleiben
unverändert korrekt berechnet), Speichern oder Senden. Für die beiden bisher einzig über
einen normalen Live-Empfang erreichbaren Zustände ist die Ausgabe byte-identisch zum
bisherigen Verhalten (siehe Host-Repro).

## Testhinweis

Lokal in `protocols/land_rover_v0.c` die `Tail:`-Formatzeile in
`subghz_protocol_decoder_land_rover_v0_get_string()` durch die Fassung oben ersetzen,
bauen/flashen. Regressionstest: ein eigenes Land-Rover-V0-Signal empfangen und die
Detailansicht prüfen — `Tail:`-Wert und `[OK]`/`[BAD]`-Kennzeichnung müssen exakt wie vorher
`0x1FFFF [OK]` bzw. `0x0FFFF [OK]` zeigen (je nach Taste). Für den eigentlichen Fixnachweis
eine gespeicherte `.sub`-Datei dieses Protokolls mit einem Texteditor öffnen und das
`ExtraBit`-Feld probeweise auf `0` setzen (oder das `Tail`-Feld auf einen anderen Wert als
`FFFF`/`7FFF`) — anschließend über "Saved" laden: `Tail:` muss jetzt den tatsächlich im File
stehenden Wert widerspiegeln statt weiterhin `0x1FFFF`/`0x0FFFF [OK]`-artig auszusehen, während
`Chk:`/`Tail:`-`[BAD]`-Kennzeichnung bereits vorher korrekt "BAD" anzeigte. Zusätzlich
Host-Repro in `repro-land-rover-v0-tail-display-formula-fix/repro.c` (beide Formeln 1:1
isoliert nachgebaut, sieben Fälle inkl. der zwei kanonischen Live-Zustände als
Regressionsanker sowie BAD-extra_bit- und off-kanonischen-Tail-Fällen) unter
`gcc -O2 -Wall -Wextra -fsanitize=address,undefined` gebaut und verifiziert: ALT und NEU
stimmen ausschließlich in den zwei kanonischen `extra_bit=1`-Fällen überein, in allen anderen
fünf Fällen weicht ALT vom tatsächlich geladenen Wert ab.

## Quelle

Eigener Fund beim vollständigen Quelltextaudit von `protocols/land_rover_v0.c`
(`G4MEOVER18/ProtoPirate`, `main`, Commit `e3d9da5`) im Rahmen der laufenden
Datei-für-Datei-Prüfung des restlichen `ProtoPirate`-Codes (nach `chrysler_v0.c`, das anders
als `fiat_v0`/`fiat_v1`/`kia_v2` bereits eine korrekte Feld-Rückrechnung im
`deserialize()`-Pfad hat und ohne Befund blieb).
