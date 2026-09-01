# ProtoPirate — Timing Tuner findet die VAG-Referenzwerte nie (Namens-Mismatch)

## Was
Der Timing Tuner (`scenes/protopirate_scene_timing_tuner.c`, aktiv bei mir über
`ENABLE_TIMING_TUNER_SCENE`, per Default gesetzt in `defines.h`) vergleicht nach einem
erfolgreichen Live-Match die gemessenen Puls-Mittelwerte gegen eine fest hinterlegte
Referenztabelle, um beim Feintunen der CC1101-Register (short/long/delta) zu helfen:

```c
const char* protocol_name = decoder_base->protocol->name;
ctx->timing_info = protopirate_get_protocol_timing(protocol_name);
```

`protopirate_get_protocol_timing()` (`protocols/protocol_items.c`) sucht `protocol_name`
zuerst per `strcmp`/`strstr` in der `protocol_timings[]`-Tabelle, danach über eine kleine
Alias-Liste (`"Honda V0"→"Kia V0"`, `"Suzuki"→"Kia V0"`, `"V3"/"V4"→"Kia V3/V4"`). Der
Tabelleneintrag für das VAG-Protokoll trägt den Namen `"VW"`:

```c
// VW: Manchester 500/1000µs
{
    .name = "VW",
    .te_short = 500,
    .te_long = 1000,
    .te_delta = 120,
    .min_count_bit = 80,
},
```

Der bei mir tatsächlich registrierte Decoder-Name ist aber `"VAG"`
(`protocols/vag.h:16` `#define VAG_PROTOCOL_NAME "VAG"`, verwendet in `protocols/vag.c:533`
als `.name = VAG_PROTOCOL_NAME` im `SubGhzProtocol`-Struct, das ich im laufenden Betrieb über
`decoder_base->protocol->name` sehe). `"VW"` ist weder als `strcmp`- noch als `strstr`-Treffer
in `"VAG"` enthalten (und umgekehrt), und keine der vier Alias-Einträge deckt diesen Fall ab —
die Suche über alle 24 Tabelleneinträge plus alle vier Aliase liefert für `"VAG"` durchgehend
`NULL`, obwohl die korrekten Referenzwerte (500/1000µs Manchester, 80 Bit, exakt aus dem
VAG-Decoder-Timing selbst) unter dem falschen Namen bereits in der Tabelle stehen.

## Nutzen
- `ctx->timing_info == NULL` schaltet im Timing Tuner den gesamten Referenzvergleich für den
  gematchten Capture ab: kein Soll/Ist-Abgleich (`short_diff`/`long_diff`), keine
  Toleranzbewertung (`short_ok`/`long_ok`), keine der drei Referenzzeilen (Short/Long/Tolerance)
  in der Detailansicht — nur die rohen Sample-Statistiken bleiben sichtbar. Für VAG-Signale ist
  damit genau die Kernfunktion des Tuners (siehe App-Name) unbenutzbar, obwohl die passenden
  Referenzwerte im Code bereits vorhanden sind.
- Betrifft ausschließlich VAG: alle 23 anderen real registrierten Decoder-Namen (Chrysler V0,
  Fiat V0/V1, Ford V0–V3, Honda Static/V1, Kia V0–V3/V4/V5–V7, Land Rover V0, Mazda V0, Porsche
  Touareg, PSA, Scher-Khan, Star Line, Subaru) lösen bereits heute korrekt auf, wie der
  Host-Repro unten für alle Namen einzeln zeigt.
- Reiner Namens-Tippfehler an einer einzigen Stelle — die Referenzdaten selbst sind korrekt und
  bleiben unverändert, ich muss sie nicht neu recherchieren.

## Betroffene Dateien
- `protocols/protocol_items.c` — nur das `.name`-Feld des VAG-Tabelleneintrags in
  `protocol_timings[]`.

Keine Änderung an `protopirate_get_protocol_timing()` selbst, an der Alias-Liste, an
`protocols/vag.c`/`vag.h` oder an einem der anderen 23 Tabelleneinträge nötig.

## Patch
```c
// VAG: Manchester 500/1000µs
{
    .name = VAG_PROTOCOL_NAME,
    .te_short = 500,
    .te_long = 1000,
    .te_delta = 120,
    .min_count_bit = 80,
},
```

Einzige Änderung: `.name = "VW"` → `.name = VAG_PROTOCOL_NAME` (Konstante aus `vag.h`, über
`protocol_items.h` bereits eingebunden, damit der Tabelleneintrag nicht erneut per Hand vom
tatsächlich registrierten Namen abweichen kann) plus Kommentar-Korrektur. Reihenfolge/Position
in der Tabelle unverändert.

## Testhinweis (ohne Hardware)
Reine String-Vergleichslogik, host-nachvollziehbar ohne Flipper-/`SubGhz`-Abhängigkeiten.
Reproduktionsprogramm in diesem Ordner
(`repro-timing-tuner-vag-name-mismatch/repro.c`) bildet `protopirate_get_protocol_timing()`
(Tabellensuche + Alias-Fallback) 1:1 nach, einmal mit dem ALT-Tabelleneintrag (`"VW"`) und
einmal mit dem NEU-Eintrag (`"VAG"`), und prüft alle 24 real registrierten Decoder-Namen
einzeln gegen beide Tabellen:

```
gcc -O2 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro
```

Ergebnis: ALT liefert für `"VAG"` `NOT FOUND`, alle 23 anderen Namen lösen bereits unter ALT
korrekt auf. NEU liefert für `"VAG"` die korrekten Referenzwerte
(short=500, long=1000, delta=120, bits=80) und lässt alle 23 anderen Namen unverändert
(Regressionsvergleich Zeile für Zeile identisch zwischen ALT und NEU).

Funktionaler Test danach lokal auf dem Gerät: im Timing Tuner ein VAG-Signal empfangen/matchen
lassen — vor dem Fix bleibt die Detailansicht auf reine Sample-Statistik ohne
Referenzzeilen/Toleranzbewertung beschränkt (Log zeigt `No timing info found for protocol:
VAG`), danach erscheinen Short/Long/Tolerance-Referenz und die OK/Nicht-OK-Bewertung wie bei
jedem anderen unterstützten Protokoll.

## Risiko
- Sehr niedrig: reine Namensänderung eines einzelnen `const char*`-Literals in einer
  schreibgeschützten, zur Kompilierzeit initialisierten Tabelle — keine Struktur-, Reihenfolge-
  oder Signaturänderung.
- Kein Einfluss auf Empfang/Dekodierung/Encoding des VAG-Protokolls selbst (`protocols/vag.c`
  bleibt unverändert) — betrifft ausschließlich die optionale Timing-Tuner-Hilfsansicht.
- Kein zusätzliches Sicherheits-/Missbrauchsrisiko.

## Offen (bewusst nicht Teil dieses Fixes)
- `"Mitsubishi V0"` (registrierter Decoder-Name, `protocols/mitsubishi_v0.h`) hat überhaupt
  keinen Eintrag in `protocol_timings[]` — anders als der VAG-Fall ist das keine Namens-
  Divergenz zu einem vorhandenen (aber falsch benannten) Eintrag, sondern eine schlicht fehlende
  Tabellenzeile; für eine künftige Sitzung vorgemerkt, sobald ich verifizierte Referenz-Timings
  für Mitsubishi V0 habe.

## Quelle
- `G4MEOVER18/ProtoPirate` (main, `e3d9da5`): `protocols/protocol_items.c`
  (`protocol_timings[]`, `protopirate_get_protocol_timing`), `protocols/vag.h`/`vag.c`
  (`VAG_PROTOCOL_NAME`, `.name = VAG_PROTOCOL_NAME`), `scenes/protopirate_scene_timing_tuner.c`
  (`timing_tuner_rx_callback`, Aufrufstelle und Auswirkung von `ctx->timing_info == NULL`),
  `defines.h` (`ENABLE_TIMING_TUNER_SCENE` per Default aktiv).
