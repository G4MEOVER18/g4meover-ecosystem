# ProtoPirate — Timing Tuner hat für Mitsubishi V0 gar keinen Tabelleneintrag

## Was

Der Timing Tuner (`scenes/protopirate_scene_timing_tuner.c`, aktiv über
`ENABLE_TIMING_TUNER_SCENE`) sucht nach jedem Live-Match den Referenzwert für das erkannte
Protokoll ausschließlich über `decoder_base->protocol->name`:

```c
const char* protocol_name = decoder_base->protocol->name;
ctx->timing_info = protopirate_get_protocol_timing(protocol_name);
```

`protopirate_get_protocol_timing()` (`protocols/protocol_items.c`) durchsucht dafür die
Tabelle `protocol_timings[]` (24 Einträge) plus eine kleine vierteilige Alias-Liste
(`"Honda V0"`/`"Suzuki"`→`"Kia V0"`, `"V3"`/`"V4"`→`"Kia V3/V4"`). Für `"Mitsubishi V0"`
(registrierter Decoder-Name, `protocols/mitsubishi_v0.h:15`
`#define MITSUBISHI_PROTOCOL_NAME "Mitsubishi V0"`, verwendet in `protocols/mitsubishi_v0.c:255`
als `.name = MITSUBISHI_PROTOCOL_NAME`) gibt es weder einen Tabelleneintrag noch einen
passenden Alias — anders als beim bereits dokumentierten `"VW"`/`VAG`-Fund
(`timing-tuner-vag-name-mismatch-fix.md`) ist das kein Namens-Tippfehler zu einer vorhandenen
Zeile, sondern schlicht eine fehlende Zeile.

Mitsubishi V0 ist kein Rand- oder Testfall, sondern ein real registrierter, aktiver Decoder:
eingebunden über `protocols/plugins/protopirate_fm_plugin.c` (2FSK/FM-Registry) neben u. a.
`Kia V0/V3-V4/V5-V7`, `Ford V1-V3`, `Honda Static`, `Land Rover V0`, `Mazda V0`, `Scher-Khan`
und `PSA` — **alle** anderen Decoder aus genau dieser FM-Registry haben bereits einen
Tabelleneintrag, nur Mitsubishi V0 fehlt.

Diese Lücke war bereits beim `VW`/`VAG`-Fund als bewusst nicht behobener Punkt vermerkt, dort
aber mit der Begründung zurückgestellt, es lägen keine „verifizierten Referenz-Timings" vor.
Beim erneuten Draufschauen: die Tabelle enthält für jedes der 24 vorhandenen Protokolle exakt
die Werte aus dem `SubGhzBlockConst` des jeweiligen Decoders selbst (z. B. `VW`/`VAG`: 500/
1000/120µs, 80 Bit — identisch zu `vag.c`s eigenem `subghz_protocol_vag_const`; `Kia V0`:
250/500/100µs, 61 Bit — identisch zu `kia_v0.c`). Es handelt sich also nicht um separat
recherchierte "wahre" Referenzwerte, sondern um dieselben Akzeptanzfenster, mit denen der
jeweilige Decoder bereits reale Signale demoduliert — sonst würde der Decoder selbst nicht
funktionieren. Für Mitsubishi V0 stehen diese Werte bereits im Quelltext
(`protocols/mitsubishi_v0.c:9-14`):

```c
static const SubGhzBlockConst subghz_protocol_mitsubishi_const = {
    .te_short = 250,
    .te_long = 500,
    .te_delta = 100,
    .min_count_bit_for_found = 80,
};
```

— dieselbe Kurzimpulslänge wie `Kia V0`/`Kia V3-V7`/`Ford V0`/`Fiat V0`/`Mazda V0`/`Star
Line`/`PSA` (250µs), nur mit eigener Bitanzahl (80 statt z. B. 61 bei Kia V0). Keine neue
Recherche nötig, die bisherige Zurückstellung war zu vorsichtig.

## Nutzen

- Schließt die zuletzt im `VW`/`VAG`-Fix explizit vermerkte offene Lücke
  ("`Mitsubishi V0` … hat überhaupt keinen Eintrag in `protocol_timings[]`").
- Ohne den Fix bleibt der Timing Tuner für jedes Mitsubishi-V0-Signal auf reine
  Sample-Statistik beschränkt (`ctx->timing_info == NULL` unterdrückt Soll/Ist-Abgleich,
  Toleranzbewertung und die drei Referenzzeilen in der Detailansicht) — für genau das
  Protokoll, das als einziges der FM-Registry noch keinen Eintrag hat.
- Reine Tabellenergänzung: keine Änderung an `protopirate_get_protocol_timing()`, der
  Alias-Liste oder einem der 24 vorhandenen Einträge.

## Betroffene Dateien

- `protocols/protocol_items.h` — Include für `mitsubishi_v0.h` ergänzen (für
  `MITSUBISHI_PROTOCOL_NAME`, folgt damit demselben Muster wie `FORD_PROTOCOL_V1_NAME` etc.
  statt eines erneut per Hand abtippbaren String-Literals).
- `protocols/protocol_items.c` — neue Zeile in `protocol_timings[]`.

## Patch

`protocols/protocol_items.h`, nach der bestehenden `"mazda_v0.h"`-Zeile:

```diff
 #include "land_rover_v0.h"
 #include "mazda_v0.h"
+#include "mitsubishi_v0.h"
 #include "porsche_touareg.h"
```

`protocols/protocol_items.c`, neue Zeile direkt nach dem bestehenden `"Mazda V0"`-Eintrag
(passend zur Registrierungsreihenfolge im FM-Plugin: `.../mazda_v0.h` unmittelbar gefolgt von
`.../mitsubishi_v0.h`):

```diff
     // Mazda V0: 250/500us
     {
         .name = "Mazda V0",
         .te_short = 250,
         .te_long = 500,
         .te_delta = 100,
         .min_count_bit = 64,
     },
+    // Mitsubishi V0: PWM 250/500us
+    {
+        .name = MITSUBISHI_PROTOCOL_NAME,
+        .te_short = 250,
+        .te_long = 500,
+        .te_delta = 100,
+        .min_count_bit = 80,
+    },
     // Land Rover V0: Differential PWM 250/500us + sync 750us
```

Werte 1:1 aus `subghz_protocol_mitsubishi_const` (`protocols/mitsubishi_v0.c`) übernommen;
`.name` als Konstante statt Literal, aus demselben Grund wie beim `VAG`-Fix (kann nicht mehr
per Hand vom tatsächlich registrierten Namen abweichen).

## Risiko

- Sehr niedrig: rein additiv, ein neuer `#include` plus eine neue, schreibgeschützte
  Tabellenzeile zur Kompilierzeit; keine der 24 vorhandenen Zeilen, keine Struktur/Signatur
  und keine Suchlogik wird verändert.
- Kein Einfluss auf Empfang/Dekodierung/Encoding von Mitsubishi V0 selbst
  (`protocols/mitsubishi_v0.c` bleibt unverändert) — betrifft ausschließlich die optionale
  Timing-Tuner-Hilfsansicht.
- Werte sind keine neue, unabhängig zu verifizierende Behauptung, sondern eine reine Kopie des
  bereits im Live-Empfangspfad genutzten `SubGhzBlockConst` desselben Decoders — dieselbe
  Herleitung wie bei allen 24 bestehenden Tabelleneinträgen.
- Kein zusätzliches Sicherheits-/Missbrauchsrisiko.

## Testhinweis (ohne Hardware)

Reine String-Vergleichslogik, host-nachvollziehbar ohne Flipper-/`SubGhz`-Abhängigkeiten.
Reproduktionsprogramm in diesem Ordner
(`repro-mitsubishi-v0-timing-tuner-missing-entry/repro.c`) bildet
`protopirate_get_protocol_timing()` (Tabellensuche + Alias-Fallback) 1:1 nach, einmal mit der
ALT-Tabelle (24 Einträge, wie aktuell im Quelltext) und einmal mit der NEU-Tabelle (25
Einträge, Mitsubishi-V0-Zeile ergänzt), und prüft alle 25 real registrierten Decoder-Namen aus
beiden Registries (AM-Haupt-Registry + FM-Plugin) einzeln gegen beide Tabellen:

```
gcc -O2 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro
```

Ergebnis: ALT liefert für `"Mitsubishi V0"` `NOT FOUND` (ebenso weiterhin `"VAG"` — separater,
hier bewusst unverändert mitgeführter Fund), alle 23 übrigen Namen lösen bereits unter ALT
korrekt auf. NEU liefert für `"Mitsubishi V0"` die korrekten Referenzwerte (short=250, long=500,
delta=100, bits=80), lässt `"VAG"` unverändert `NOT FOUND` (nicht Teil dieses Fixes) und lässt
alle 23 übrigen Namen unverändert (Regressionsvergleich Zeile für Zeile identisch zwischen ALT
und NEU, im Repro explizit geprüft). Unter ASan/UBSan ohne Befund.

Funktionaler Test danach lokal auf dem Gerät: im Timing Tuner ein Mitsubishi-V0-Signal
empfangen/matchen lassen — vor dem Fix bleibt die Detailansicht auf reine Sample-Statistik ohne
Referenzzeilen/Toleranzbewertung beschränkt, danach erscheinen Short/Long/Tolerance-Referenz
und die OK/Nicht-OK-Bewertung wie bei jedem anderen unterstützten Protokoll.

## Quelle

`G4MEOVER18/ProtoPirate` (main, aktueller Klon dieser Sitzung): `protocols/protocol_items.h`/
`.c` (`protocol_timings[]`, `protopirate_get_protocol_timing`, fehlender Include),
`protocols/mitsubishi_v0.h`/`.c` (`MITSUBISHI_PROTOCOL_NAME`, `subghz_protocol_mitsubishi_const`,
`.name = MITSUBISHI_PROTOCOL_NAME`), `protocols/plugins/protopirate_fm_plugin.c`
(Registrierung von `mitsubishi_v0_protocol` in der FM-Registry),
`scenes/protopirate_scene_timing_tuner.c` (Aufrufstelle, registry-unabhängig über
`decoder_base->protocol->name`). Aufgegriffen aus dem im `VW`/`VAG`-Fix selbst vermerkten
offenen Punkt; dabei zusätzlich `protocols/ford_v0.c`, `mitsubishi_v0.c` (Encoder/Decoder-
Deserialize, insb. die `pp_encoder_read_fields()`-Aufrufstellen für Serial/Btn/Cnt) sowie
`protocols/aut64.c`, `keys.c` und `protocols_common.c` aus dem weiterhin offenen Rückstellpunkt
"restlicher ProtoPirate-Code" auf das bereits bestätigte Bug-Muster (`deserialize()` vergisst
von `get_string()` genutzte Felder) hin geprüft: kein neuer Fund dieser Art, da `ford_v0.c` und
`mitsubishi_v0.c` die Serial/Btn/Cnt-Felder über `pp_encoder_read_fields()` aus stets
unbedingt geschriebenen Serialize-Feldern zurücklesen (strukturell identisch zum bereits als
„gut" bestätigten `kia_v6`-Muster), `aut64.c` eine in sich geschlossene, bereits defensiv
validierte Kryptoprimitive ohne Deserialize-Bezug ist und `keys.c`/`protocols_common.c`
reine, unauffällige Hilfsfunktionen sind.
