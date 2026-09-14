# ProtoPirate — PSA-Decoder: unbegrenzter Bit-Zähler in State4 verwirft gültige Signale bei Störkanten

## Was

Rückstellpunkt "restlicher PSA-Bruteforce-Kernel" aus der letzten Sitzung weiterverfolgt —
diesmal nicht `psa_crypto_bf.c`/`psa_bf_core.c` (weiterhin zurückgestellt, reverse-engineerte
Krypto-Konstanten), sondern den bislang komplett ungeprüften eigentlichen Protokoll-Decoder
`protocols/psa.c` (1443 Zeilen). Reine Zustandsautomat-/Zähl-Logik, unabhängig von der
Kryptografie vollständig host-seitig verifizierbar.

`subghz_protocol_decoder_psa_feed()` decodiert ein PSA-128-Bit-Signal über zwei
alternative, sich gegenseitig ausschließende Zustandspfade (`State1→State2` bzw.
`State3→State4`, je nach erkanntem Präambel-Timing — kurze vs. lange Sync-Variante). Beide
Pfade decodieren dieselben 64+16 Bit (Key1/Key2) über dieselbe Manchester-Logik und laufen in
denselben Abschlusstest:

```c
if(instance->decode_count_bit != PSA_KEY2_BITS) {   // PSA_KEY2_BITS = 0x50 = 80
    return;
}
```

In `State2` ist der Zähler-Inkrement dabei bewusst gedeckelt:

```c
// protocols/psa.c, State2, Zeile 1076
if(should_process && instance->decode_count_bit < PSA_KEY2_BITS) {
    bool decoded_bit = false;
    if(manchester_advance(...)) {
        ...
        instance->decode_count_bit++;
        ...
    }
}
```

Zusätzliche, nach dem 80. Datenbit eintreffende Manchester-dekodierbare Flanken (Jitter/
Rauschen zwischen dem letzten Datenbit und der eigentlichen Rahmenende-Lücke — physikalisch
plausibel und genau der Fall, gegen den dieser Deckel schützt) werden dadurch stillschweigend
aufgenommen, ohne den Zähler weiter zu erhöhen: `decode_count_bit` bleibt exakt bei 80 stehen,
passend zum nachfolgenden Gleichheitstest.

`State4` decodiert dieselben 64+16 Bit über eine strukturell identische Manchester-Schleife,
aber **ohne** diesen Deckel:

```c
// protocols/psa.c, State4, Zeile 1184-1226
if(!level) {
    uint8_t manchester_input;
    ...
    if(manchester_advance(
           instance->manchester_state,
           (ManchesterEvent)manchester_input,
           &instance->manchester_state,
           &decoded_bit)) {
        ...
        instance->decode_count_bit++;   // unbedingt, kein `< PSA_KEY2_BITS`-Deckel
        ...
    }
}
```

Der einzige Schutz ist der weit lockerere, ganz oben im `case` stehende Test
`decode_count_bit >= PSA_MAX_BITS` (`PSA_MAX_BITS` = 0x79 = 121) — der erst nach über 40
zusätzlichen Flanken über dem 80-Bit-Ziel greift und dann nur den kompletten Zustand
verwirft, statt (wie in State2) überzählige Flanken einfach zu ignorieren.

Folge: trifft in State4 auch nur **eine einzige** zusätzliche Manchester-dekodierbare Flanke
ein, bevor der Decoder die wahre Rahmenende-Lücke (~500 µs High, `PSA_TE_END_500`) erkennt,
steht `decode_count_bit` bei 81 statt 80. Der anschließende Abschlusstest
(`decode_count_bit != PSA_KEY2_BITS`, Zeile 1235) schlägt dann fehl und die Funktion kehrt
ohne Dekodierung zurück (`return;`) — ein vollständig und korrekt empfangenes 80-Bit-Signal
wird verworfen, obwohl exakt dieselbe Störung im State2-Pfad folgenlos bliebe. State4-Signale
sind dadurch strukturell empfindlicher gegenüber Flanken-Rauschen als State2-Signale desselben
Protokolls, obwohl beide denselben Abschlusstest erwarten.

## Nutzen

Beseitigt eine reine Zähler-Asymmetrie zwischen zwei Decoder-Pfaden desselben Protokolls, die
bei State4-Empfang zu stillen Fehlerkennungen (verworfenes, an sich vollständig korrektes
Signal) führt, ohne jede Fehlermeldung — der Nutzer sieht einfach kein decodiertes Signal und
hat keinen Hinweis, dass eine einzelne Störflanke die Ursache war. Rein defensive
Zähler-Härtung, keine Änderung an der eigentlichen Bit-Interpretation oder Kryptografie.

## Betroffene Dateien

- `protocols/psa.c` — `subghz_protocol_decoder_psa_feed()`, `case PSADecoderState4`,
  Manchester-Bit-Dekodierzweig (`if(!level) { ... }`). Einziger Fix-Ort.

## Patch

```diff
         if(!level) {
             uint8_t manchester_input;
             bool decoded_bit = false;
 
             if(duration < PSA_TE_SHORT_125) {
                 tolerance = PSA_TE_SHORT_125 - duration;
                 if(tolerance > PSA_TOLERANCE_49) {
                     return;
                 }
                 manchester_input = ((level ^ 1) & 0x7f) << 1;
             } else {
                 tolerance = duration - PSA_TE_SHORT_125;
                 if(tolerance < PSA_TOLERANCE_50) {
                     manchester_input = ((level ^ 1) & 0x7f) << 1;
                 } else if(duration >= PSA_TE_LONG_250 && duration < PSA_TE_LONG_300) {
                     if(level == 0) {
                         manchester_input = 6;
                     } else {
                         manchester_input = 4;
                     }
                 } else {
                     return;
                 }
             }
 
-            if(manchester_advance(
+            if(instance->decode_count_bit < PSA_KEY2_BITS &&
+               manchester_advance(
                    instance->manchester_state,
                    (ManchesterEvent)manchester_input,
                    &instance->manchester_state,
                    &decoded_bit)) {
                 uint32_t carry = (instance->decode_data_low >> 31) & 1;
                 instance->decode_data_low = (instance->decode_data_low << 1) |
                                             (decoded_bit ? 1 : 0);
                 instance->decode_data_high = (instance->decode_data_high << 1) | carry;
                 instance->decode_count_bit++;
 
                 if(instance->decode_count_bit == PSA_KEY1_BITS) {
                     instance->key1_low = instance->decode_data_low;
                     instance->key1_high = instance->decode_data_high;
                     instance->decode_data_low = 0;
                     instance->decode_data_high = 0;
                 }
             }
         } else if(level) {
```

Ein-Zeilen-Fix, mirror-genau zu State2s bereits vorhandenem Deckel: sobald 80 Bit erreicht
sind, werden weitere Flanken zwar weiterhin an `manchester_advance()` bzw. gar nicht mehr
übergeben (Kurzschluss-Auswertung verhindert den Aufruf sobald der Deckel greift) und der
Zähler bleibt stehen, statt weiterzuzählen. Wichtig: der Manchester-*Zustand* selbst
(`instance->manchester_state`) wird dadurch ab Bit 80 nicht mehr fortgeschrieben — exakt wie
in State2, wo derselbe Kurzschluss über `should_process && ... < PSA_KEY2_BITS` bereits genauso
wirkt.

## Risiko

**Sehr gering.** Reiner Zähler-Deckel, identisch zum bereits produktiv laufenden State2-Muster
in derselben Datei. Keine Änderung an der Bit-Interpretation, den Zeitfenstern oder der
Manchester-Dekodierlogik selbst — betroffen ist ausschließlich, ob Flanken *nach* dem 80.
Datenbit noch gezählt werden. Ein echt zu kurzes/unvollständiges Signal (< 80 Bit) bleibt vom
Fix unberührt und wird weiterhin korrekt über denselben `!=`-Test verworfen (siehe
Host-Repro-Regressionsfälle unten).

## Testhinweis

Rein logisch verifizierbar, keine Hardware nötig (siehe Host-Repro unten). Wer zusätzlich am
Gerät prüfen will: ein PSA-Signal, das über den State3→State4-Pfad einläuft (langes
Präambel-Timing, siehe `PSA_TE_LONG_250`/`PSA_PATTERN_THRESHOLD_2`-Schwelle beim
State3→State4-Übergang), wiederholt aufnehmen — vor dem Fix schlägt die Dekodierung bei jedem
Versuch fehl, bei dem eine einzelne zusätzliche Flanke zwischen letztem Datenbit und
Rahmenende-Lücke ankommt (abhängig von HF-Rauschen/Empfangsqualität, nicht bei jedem Versuch
reproduzierbar); nach dem Fix wird ein ansonsten vollständiges 80-Bit-Signal zuverlässig
decodiert. Da die auslösende Störflanke vom Empfangsrauschen abhängt, ist der Host-Repro (der
die Störflanke deterministisch erzwingt) der zuverlässigere Nachweis.

Host-Repro: `repro-psa-state4-key2-bit-count-uncapped/repro.c`, baut die Zähler-Logik beider
Zustände (`State2` unverändert als Referenz, `State4` als ALT/aktueller Code und als
NEU/Fix) 1:1 nach, unter `gcc -O1 -Wall -Wextra -fsanitize=address,undefined` gegen 0-5
zusätzliche Störflanken nach einem vollständigen 80-Bit-Signal sowie 5 echte
Kurz-Signal-Regressionsfälle (75-79 Bit, keine Störflanke) geprüft:

```
extra_bits=0  State4-ALT accepted=1  State4-FIXED accepted=1  State2(existing) accepted=1
extra_bits=1  State4-ALT accepted=0  State4-FIXED accepted=1  State2(existing) accepted=1
extra_bits=2  State4-ALT accepted=0  State4-FIXED accepted=1  State2(existing) accepted=1
extra_bits=3  State4-ALT accepted=0  State4-FIXED accepted=1  State2(existing) accepted=1
extra_bits=4  State4-ALT accepted=0  State4-FIXED accepted=1  State2(existing) accepted=1
extra_bits=5  State4-ALT accepted=0  State4-FIXED accepted=1  State2(existing) accepted=1

--- summary over 5 trailing-noise cases (extra_bits=1..5) ---
State4-ALT   false negatives (valid capture dropped): 5 / 5
State4-FIXED false negatives (valid capture dropped): 0 / 5
State2 (existing, unchanged) false negatives:         0 / 5

OK: State4-ALT drops every trailing-noise capture that State2 and State4-FIXED both still accept correctly; truncated captures remain correctly rejected by all three.
```

ALT (aktueller Code) verliert bei jeder der fünf getesteten Störflanken-Mengen (1-5
zusätzliche Flanken) das vollständige Signal; NEU (Fix) und der unveränderte State2-Pfad
akzeptieren dasselbe Signal in allen Fällen korrekt. Alle fünf Kurz-Signal-Regressionsfälle
(75-79 Bit, keine Störflanke) werden von ALT, NEU und State2 gleichermaßen korrekt verworfen —
der Fix führt zu keinem falschen Akzeptieren unvollständiger Signale.

## Zusätzlich geprüft, kein Fund

- `psa_second_stage_xor_encrypt()` (Encoder, Mode 0x23) gegen `psa_second_stage_xor_decrypt()`
  (Decoder) algebraisch durchgerechnet: beide sind exakte Inversen zueinander über alle sechs
  Feldgleichungen (`E0`-`E5`), kein Fund.
- `psa_calculate_checksum()`: wird in Encoder (`psa_build_buffer_mode23`, nach der
  XOR-Verschlüsselung) und Decoder (`psa_direct_xor_decrypt`, vor der Entschlüsselung)
  konsistent auf denselben Geheimtext-Bytes (`buffer[2..7]`) berechnet — keine Diskrepanz
  zwischen Sende- und Empfangspfad.
- `psa_extract_fields_mode23()` gegen `psa_build_buffer_mode23()` feldweise abgeglichen
  (Serial-/Counter-/CRC-/Button-Bytelayout): Encoder-Packung und Decoder-Entpackung sind
  bytegenau symmetrisch, kein Fund.
- `psa_encoder_build_upload()`: Pulszahl-Rechnung für den worst case (80 Header-Pulse + 3
  Übergangspulse + 128 Key1-Bit-Pulse + 32 Key2-Bit-Pulse + 2 Endpulse = 325) exakt
  nachgerechnet — passt bitgenau zur allozierten `PSA_UPLOAD_CAPACITY` (325), kein
  Pufferüberlauf und kein vorzeitig abgeschnittenes Signal in keinem Pfad.
- `subghz_protocol_encoder_psa_deserialize()`/`subghz_protocol_decoder_psa_deserialize()`:
  jeder Flipper-Format-Lesezugriff (`pp_flipper_read_hex_u64()`, `pp_encoder_read_fields()`,
  `flipper_format_read_uint32()` für CRC/Seed) ruft vorher selbst `flipper_format_rewind()`
  auf (in `protocols_common.c` bzw. direkt inline) — Feldreihenfolge in der gespeicherten
  Datei spielt keine Rolle, kein Fund (anders als beim bereits dokumentierten
  `Key_2`-Hex/2-Verlust-Fund, der eine andere Ursache hatte).

Restlicher PSA-Bruteforce-Kernel (`protocols/psa_crypto.c`s reine Puffer-Auf-/Abbau-Helfer
sowie die reverse-engineerten Krypto-Konstanten selbst) weiterhin offen.

## Quelle

Eigene Quelltextprüfung von `G4MEOVER18/ProtoPirate` (eigener Fork/Stand, `e3d9da5`):
`protocols/psa.c` — reiner Logikfund im eigenen Code, kein externer Quellenvergleich nötig.
