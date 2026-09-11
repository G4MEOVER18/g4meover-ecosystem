# ProtoPirate — toter Zweig im Ford-V3-Decoder (Data-Step, invalider Puls)

## Was

Aufgegriffener Rückstellpunkt aus der vorherigen Sitzung: `ford_v1.c`/`ford_v2.c`/`ford_v3.c`
waren bislang nur auf das eine, bereits mehrfach bestätigte Deserialize-Bug-Muster geprüft
(fehlende Feldrücknahme), nicht auf sonstige Korrektheit. Diese Sitzung deckt genau das nach:
alle drei Dateien vollständig gelesen.

`ford_v1.c` und `ford_v2.c` (Zähler-/Paritäts-/CRC-/Bit-Verwürflungslogik, jeweils über 800 bzw.
1200 Zeilen) ergeben keinen Fund — die Feldberechnungen sind durchgehend rücktransformierbar
(Encoder-Gegenrechnung liefert exakt den Decoder-Eingabewert zurück, siehe unten unter
"Zusätzlich geprüft, kein Fund").

`ford_v3.c` (`subghz_protocol_decoder_ford_v3_feed()`, `FordV3DecoderStepData`-Zweig) enthält
dagegen einen toten Codezweig:

```c
case FordV3DecoderStepData: {
    if(!pp_is_short(duration, &subghz_protocol_ford_v3_const) &&
       !pp_is_long(duration, &subghz_protocol_ford_v3_const)) {
        ford_v3_emit_if_ready(instance);
        instance->decoder.parser_step = FordV3DecoderStepReset;

        if(pp_is_short(duration, &subghz_protocol_ford_v3_const)) {   // <-- nie wahr
            ford_v3_reset_data(instance);
            instance->preamble_count = 1U;
            instance->decoder.parser_step = FordV3DecoderStepPreamble;
        }
        break;
    }
    ...
```

`pp_is_short()`/`pp_is_long()` (`protocols/protocols_common.h:92-98`) sind reine, seiteneffektfreie
Funktionen von `(duration, Protokoll-Konstante)`. Die äußere Bedingung hat bereits bewiesen, dass
`pp_is_short(duration, …)` **falsch** ist (linke Seite des `&&`) — dieselbe Auswertung mit
denselben Argumenten kann im inneren `if` folglich nie wahr sein. Der Block ist wortwörtlich der
Drei-Zeiler aus `FordV3DecoderStepReset` (Zeile 133-138 im Original: "bei kurzem Puls Präambel neu
starten") — offenkundig ein Copy-Paste mit der Absicht, einen ungültigen Puls im selben `feed()`-
Aufruf sofort als ersten Puls einer neuen Präambel weiterzuverwenden, dabei aber versehentlich
dieselbe (bereits als ungültig erkannte) `duration` erneut geprüft statt auf den nächsten Puls zu
warten. Funktional entsteht dadurch **keine** Lücke: der äußere Zweig setzt `parser_step` bereits
auf `FordV3DecoderStepReset`, und der `FordV3DecoderStepReset`-Fall im selben `switch()` fängt den
nächsten (dann tatsächlich kurzen) Puls beim nächsten `feed()`-Aufruf korrekt ab — der tote Zweig
ist also reine Redundanz ohne Verhaltensunterschied, kein Empfangsfehler.

## Nutzen

Entfernt einen beweisbar unerreichbaren Zweig aus dem einzigen Hot-Path-Decoder-Callback von
Ford V3 (`feed()`, wird pro empfangenem Funkpuls aufgerufen). Kein Verhaltensgewinn beim Empfang
selbst (siehe Beweis oben und Host-Repro unten), aber weniger Verwirrungspotential für künftige
Änderungen an dieser Funktion — ein Leser könnte sonst annehmen, der Zweig werde tatsächlich
gebraucht, und beim nächsten Umbau versehentlich echte Logik daran anhängen.

## Betroffene Dateien

- `protocols/ford_v3.c` — `subghz_protocol_decoder_ford_v3_feed()`, `FordV3DecoderStepData`-Fall.

## Patch

```diff
     case FordV3DecoderStepData: {
         if(!pp_is_short(duration, &subghz_protocol_ford_v3_const) &&
            !pp_is_long(duration, &subghz_protocol_ford_v3_const)) {
             ford_v3_emit_if_ready(instance);
             instance->decoder.parser_step = FordV3DecoderStepReset;
-
-            if(pp_is_short(duration, &subghz_protocol_ford_v3_const)) {
-                ford_v3_reset_data(instance);
-                instance->preamble_count = 1U;
-                instance->decoder.parser_step = FordV3DecoderStepPreamble;
-            }
             break;
         }
```

Keine weiteren Änderungen nötig — `FordV3DecoderStepReset` deckt den entfernten Fall bereits ab.

## Risiko

Sehr gering. Der entfernte Zweig war beweisbar unerreichbar (siehe "Was" und Host-Repro); seine
Entfernung ändert das beobachtbare Verhalten des Decoders nicht. Einziges denkbares Risiko wäre
ein Tippfehler beim Patchen selbst — der Diff ist eine reine Löschung ohne Zusatzlogik.

## Testhinweis

Rein logisch verifizierbar, keine Hardware nötig (siehe Host-Repro unten). Wer zusätzlich am
Gerät prüfen will: einen realen Ford-V3-Sender (oder eine zuvor gespeicherte `.sub`-Datei mit
Ford-V3-Signal) vor und nach dem Patch empfangen/wiedergeben — Anzeige (Sn/Btn/Cnt/Hop) muss
identisch bleiben, da der Patch reinen toten Code entfernt.

Host-Repro: `repro-ford-v3-unreachable-preamble-restart-check/repro.c` — zwei unabhängige
Prüfungen unter `gcc -O2 -Wall -Wextra -fsanitize=address,undefined`:
1. Erschöpfende Suche über `duration` von 0 bis 20000: die innere `pp_is_short()`-Prüfung feuert
   in keinem einzigen Fall (0/20001).
2. Randomisierte Äquivalenzsimulation (2000 Pulsströme × 100 Pulse, ALT- vs. NEU-Version des
   Data-Step-Zweigs nebeneinander mit denselben Zufallseingaben gefüttert): `parser_step`,
   `preamble_count` und Bitfortschritt stimmen nach jedem Puls exakt überein, der tote Zweig wird
   in keinem der 200 000 gefütterten Pulse betreten.

Tatsächliche Ausgabe:
```
[1/2] exhaustive duration sweep (0..20000): inner pp_is_short() fired 0 / 20001 times
[2/2] randomized OLD-vs-NEW equivalence: 200000 pulses fed, 0 mismatches, dead branch entered 0 times
OK: dead branch is unreachable and its removal is behavior-preserving.
```

## Zusätzlich geprüft, kein Fund

- **`ford_v0.c`/`ford_v1.c`/`ford_v2.c`** (Deserialize-Feld-Rücknahme): bereits in der
  vorherigen Sitzung bestätigt, alle drei lesen Serial/Btn/Cnt strukturell wie `kia_v6`
  (unbedingt geschriebene Felder), kein erneuter Fund.
- **`ford_v1.c`** vollständig gelesen (CRC16-Prüfung inkl. Ganzblock-Invertierungs-Fallback,
  Byte-6/7-Interleave-Descramble mit Encode-Rundtrip-Validierung, Zähler-/Button-/Seriennummer-
  Rückrechnung aus dem entschlüsselten Klartext, `Key_3`/CRC-Legacy-Format-Kompatibilität):
  jede geprüfte Feld-Transformation ist über die zugehörige Gegenrichtung exakt umkehrbar
  (`ford_v1_fields_from_plain()` ↔ `ford_v1_plain_apply_fields()` liefert für zufällige
  Serial/Btn/Cnt-Werte bitgenau denselben Wert zurück), kein Speicherzugriffsfehler. Zwei
  Stellen mit unerreichbarem/inaktivem Code notiert, keine Funktionsauswirkung:
  `ford_v1_encoder_apply_counter_cap()`s `ee == 0x80000001u`-Zweig, da
  `ford_v1_encoder_preset_hop_read_stub()` fest `0` liefert (erkennbar als Platzhalter für ein
  noch nicht angebundenes "Preset-Hop"-Feature); `subghz_block_generic_global_counter_override_get()`
  ist ein `weak`-Symbol mit Standardrückgabe `false`, hat also ebenfalls keinen Effekt, solange
  keine andere Übersetzungseinheit es überschreibt.
- **`ford_v2.c`** vollständig gelesen (Sync-Wort-Erkennung, Manchester-Zustandsautomat inkl.
  Präambel→Sync→Data-Übergängen, Paritätsbit-Berechnung für Button, Struktur-Validierung
  `structure_ok`): Encoder-Gegenrechnung (`ford_v2_encoder_rebuild_raw_from_payload()`) liefert
  für zufällige Serial/Btn/Cnt/Tail31-Werte exakt dieselben rohen Bytes zurück, die der Decoder
  aus einem entsprechenden Signal extrahieren würde, kein Fund.
- **`ford_v3.c`** darüber hinaus (Preamble-Erkennung, Manchester-Bitextraktion, CRC-freie
  Struktur, Encoder/Decoder-Rundtrip von Serial/Btn/Cnt): kein weiterer Fund über den oben
  dokumentierten toten Zweig hinaus.

Damit ist der Rückstellpunkt "ford_v1/v2/v3 auf sonstige Korrektheit prüfen" aus der
vorherigen Sitzung abgeschlossen.

## Quelle

Eigene Quelltextprüfung von `G4MEOVER18/ProtoPirate` (eigener Fork/Stand,
`protocols/ford_v1.c`, `protocols/ford_v2.c`, `protocols/ford_v3.c`,
`protocols/protocols_common.h`), kein externer Quellenvergleich nötig (reiner Logikfund im
eigenen Code, keine Quellenübernahme).
