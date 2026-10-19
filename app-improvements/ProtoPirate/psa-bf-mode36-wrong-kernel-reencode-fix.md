# ProtoPirate — PSA: manuelle Neu-Codierung verwendet nach BF2-Fund den falschen Bruteforce-Kernel

## Was

Letzten noch offenen Teil des Rückstellpunkts "PSA-Bruteforce-Kernel" bearbeitet: nicht mehr
den *Erfolgspfad* des Bruteforce selbst (das BF2-Counter/CRC-Konflationsproblem ist bereits
als eigener Fund dokumentiert, siehe `psa-bf2-counter-crc-buffer8-conflation-fix.md`), sondern
die Gegenrichtung — was passiert, wenn ein bereits per Bruteforce entschlüsselter Fund später
für die manuelle Codeerzeugung/den erneuten Versand genutzt wird (`protocols/psa.c`,
`subghz_protocol_encoder_psa_deserialize()` → `psa_encoder_build_upload()`).

`psa_brute_force_run()` (`protocols/psa_crypto_bf.c`) hat zwei unabhängige Kernel mit
grundverschiedenen Schlüsselableitungen auf denselben 80 Bit Schlüsselmaterial:

- **BF1** (Suchraum `0x23000000`–`0x23FFFFFF`): Arbeitsschlüssel wird durch TEA-Verschlüsselung
  fester Konstanten (`PSA_CRYPTO_BF1_CONST_U4/U5`) mit dem BF1-Schlüsselplan
  (`psa_crypto_bf1_key_schedule`) abgeleitet; Erfolgsprüfung über eine 8-Bit-Prüfsumme
  (`psa_crypto_tea_crc()`), die allein im untersten Byte von `dec_v1` liegt.
- **BF2** (Suchraum `0xF3000000`–`0xF3FFFFFF`): Arbeitsschlüssel ist einfach
  `psa_crypto_bf2_key_schedule[i] ^ counter` (kein TEA-Zwischenschritt); Erfolgsprüfung über
  eine 16-Bit-CRC16 (`psa_crypto_crc16_bf2()`), die die vollen unteren 16 Bit von `dec_v1`
  belegt.

Beide Erfolgsfälle markieren das Ergebnis aber mit **demselben** `decrypted_type = 0x36`
(`protocols/psa_crypto_bf.c`, `psa_bf_fill_state_from_buffer()`, Aufrufstellen Zeile 96 und
139) — die `.sub`-Datei (Feld `Type`) kann danach nicht mehr unterscheiden, welcher Kernel
den Schlüssel geliefert hat.

Das wäre folgenlos, wenn `Type:0x36` nur zur Anzeige diente. Tatsächlich treibt es aber den
produktiven Zweck der ganzen Funktion an: `subghz_protocol_encoder_psa_deserialize()` liest
`Type` aus der gespeicherten Datei und setzt `instance->mode = (type == 0x36) ? 0x36 : 0x23`
(`protocols/psa.c` Zeile 588) — **nie** etwas anderes für `0x36`. `psa_encoder_build_upload()`
ruft für `mode == 0x36` ausschließlich `psa_build_buffer_mode36()` auf (Zeile 429-436), und
diese Funktion leitet **unbedingt mit dem BF1-Schlüsselplan und der 8-Bit-CRC** einen neuen
Key1/Key2 aus Serial/Counter/Button/CRC ab (Zeile 308-391) — unabhängig davon, ob der
eigentliche, am Gerät gültige Schlüssel per BF1 *oder* BF2 gefunden wurde. Für jeden
BF2-Fund (Type 0x36, aber eigentlich BF2-Herkunft) erzeugt "Senden"/"manuell neu
generieren" damit einen Key1/Key2, den der reale Empfänger nicht akzeptiert — die zentrale
Funktion, für die der Bruteforce-Fund überhaupt gespeichert wird (Zähler hochsetzen und
erneut senden), ist für jeden BF2-Fund von Grund auf defekt.

Algebraisch nachvollzogen, warum ein Fix ohne Risiko an der (als riskant eingestuften)
TEA-Kryptografie selbst möglich ist: beide Bruteforce-Schleifen prüfen
`(counter & 0xFFFFFF) == (dec_v0 >> 8)`, und `dec_v0 >> 8` ist exakt das gespeicherte
`decrypted_serial`-Feld. `psa_build_buffer_mode36()` nutzt das bereits aus, um seinen
eigenen Such-Counter aus dem Serial zurückzugewinnen
(`bf_counter = PSA_CRYPTO_BF1_START | (serial & 0xFFFFFF)`), statt ihn separat zu
speichern. Derselbe Trick funktioniert identisch für BF2
(`PSA_CRYPTO_BF2_START | (serial & 0xFFFFFF)`) — der fehlende Teil ist nicht die
Rekonstruierbarkeit, sondern allein das Fehlen eines Markers, *welcher* der beiden Kernel
überhaupt gemeint ist, und einer zweiten Encoder-Funktion, die den BF2-Schlüsselplan
tatsächlich verwendet.

## Nutzen

Schließt den Rückstellpunkt "PSA-Bruteforce-Kernel" vollständig ab. Nach dem Fix erzeugt
die manuelle Codeerzeugung/der erneute Versand eines BF2-Fundes denselben Key1/Key2, den
auch der reale Sender erzeugt hätte — vor dem Fix ein für jeden BF2-Fund unbemerkt falsches,
am Zielgerät wirkungsloses Signal.

## Betroffene Dateien

- `protocols/psa_crypto_bf.c` — neue Funktion `psa_bf_fill_state_from_buffer_bf2()` (ersetzt/
  erweitert den bereits in `psa-bf2-counter-crc-buffer8-conflation-fix.md` vorgeschlagenen
  gleichnamigen Fix um die neue Typkennzeichnung `0x37`; wer jenen Patch bereits lokal
  eingespielt hat, ersetzt dort nur die letzte Zeile `state->decrypted_type = 0x36;` durch
  `0x37`, sonst diesen Patch komplett anwenden — beide sind nicht kumulativ gemeint), ein
  geänderter Aufruf im BF2-Erfolgszweig. `psa_bf_fill_state_from_buffer()` selbst und ihr
  BF1-Aufruf bleiben unverändert.
- `protocols/psa.c` — neue Funktion `psa_build_buffer_mode37()` (Spiegelbild zu
  `psa_build_buffer_mode36()`, aber mit BF2-Schlüsselableitung und 16-Bit-CRC), ein neuer
  Dispatch-Zweig in `psa_encoder_build_upload()`, eine erweiterte Typ-zu-Modus-Abbildung in
  `subghz_protocol_encoder_psa_deserialize()`.

## Patch

`psa-bf-mode36-wrong-kernel-reencode-fix/psa_crypto_bf_c.patch`:

```diff
@@ -54,6 +54,23 @@ static void psa_bf_fill_state_from_buffer(PsaBfState* state, uint8_t* buffer) {
     state->decrypted_type = 0x36;
 }
 
+// BF2 success case: buffer[8] is here the upper byte of the actually-checked 16-bit CRC
+// (see expected_crc = dec_v1 & 0xFFFF below), not counter payload like in the BF1 case --
+// own field split instead of reusing the BF1 function. Also marks the result with the new
+// Type 0x37 (instead of 0x36) so the encoder can later tell which bruteforce kernel (and
+// therefore which key schedule) actually produced this result -- see
+// psa-bf-mode36-wrong-kernel-reencode-fix.md for why that distinction matters.
+static void psa_bf_fill_state_from_buffer_bf2(PsaBfState* state, uint8_t* buffer) {
+    state->decrypted_button = (buffer[5] >> 4) & 0xF;
+    state->decrypted_serial = ((uint32_t)buffer[3] << 8) | ((uint32_t)buffer[2] << 16) |
+                              (uint32_t)buffer[4];
+    state->decrypted_counter = ((uint32_t)buffer[7] << 8) | ((uint32_t)buffer[6] << 16) |
+                               (((uint32_t)buffer[5] & 0xF) << 24);
+    state->decrypted_crc = ((uint16_t)buffer[8] << 8) | (uint16_t)buffer[9];
+    state->decrypted_seed = state->decrypted_serial;
+    state->decrypted_type = 0x37;
+}
+
 void psa_brute_force_run(PsaBfState* state) {
     uint8_t buffer[48] = {0};
     psa_crypto_setup_byte_buffer(buffer, state->key1_low, state->key1_high, state->key2_low);
@@ -136,7 +153,7 @@ void psa_brute_force_run(PsaBfState* state) {
             uint16_t crc16 = psa_crypto_crc16_bf2(crc_buffer, 6);
             uint16_t expected_crc = (uint16_t)(dec_v1 & 0xFFFF);
             if(crc16 == expected_crc) {
-                psa_bf_fill_state_from_buffer(state, buffer);
+                psa_bf_fill_state_from_buffer_bf2(state, buffer);
                 state->progress_current = (PSA_CRYPTO_BF1_END - PSA_CRYPTO_BF1_START) +
                                           (counter - PSA_CRYPTO_BF2_START);
                 state->status = PSA_BF_STATUS_FOUND;
```

`psa-bf-mode36-wrong-kernel-reencode-fix/psa_c.patch` (gekürzt, vollständig in der Datei):
fügt `psa_build_buffer_mode37()` nach `psa_build_buffer_mode36()` ein (identischer Aufbau,
BF2-Schlüsselplan `psa_crypto_bf2_key_schedule[i] ^ bf_counter` statt TEA-Zwischenschritt,
`v1 = ((counter >> 8) & 0xFFFF) << 16 | crc16` statt `(counter & 0xFFFFFF) << 8 | crc8`),
erweitert den Dispatch in `psa_encoder_build_upload()` um `else if(instance->mode == 0x37)`
und die Typ-Abbildung in `subghz_protocol_encoder_psa_deserialize()` um den Fall `0x37`.

Beide vollständigen Patches liegen in diesem Verzeichnis
(`psa-bf-mode36-wrong-kernel-reencode-fix/psa_crypto_bf_c.patch`,
`.../psa_c.patch`) und sind gegen den geprüften Stand `e3d9da5` erstellt.

## Risiko

**Gering bis mäßig.** Reine Ergänzung: `psa_build_buffer_mode23()`, `psa_build_buffer_mode36()`
und ihre beiden bestehenden Aufrufstellen bleiben textuell unverändert; Typ 0x36 verhält sich
exakt wie zuvor. Die einzige Verhaltensänderung für bestehende Dateien: ein *historischer*
BF2-Fund, der vor diesem Fix bereits mit `Type:0x36` gespeichert wurde, nutzt nach dem Fix
weiterhin (fälschlich) `psa_build_buffer_mode36()`, da sein gespeicherter Typ nie auf `0x37`
aktualisiert wird — das betrifft nur bereits vorhandene Dateien aus der Zeit vor dem Fix,
nicht neue Bruteforce-Läufe. Etwas Sorgfalt braucht die BF2-Schlüsselableitung selbst
(reverse-engineerte Kryptografie, laut WORKFLOW als eigener Risikobereich geführt) — deshalb
der host-verifizierte Rückgriff auf die bereits vorhandene, unveränderte BF2-Erfolgsprüfung
aus `psa_brute_force_run()` als Gegenprobe (siehe Testhinweis), statt die Kernlogik neu zu
erfinden.

## Testhinweis

Rein logisch verifizierbar, keine Hardware nötig. Host-Repro:
`repro-psa-bf-mode36-wrong-kernel-reencode/repro.c`, baut die reinen Pufferhelfer aus
`psa_crypto.c`, die Schlüsselplan-/TEA-Hilfsfunktionen aus `psa_crypto_bf.c` sowie
(reduziert auf die reine Pufferlogik, ohne SubGhz-Upload-Framing) die bestehende
`psa_build_buffer_mode36()` (ALT) und die neue `psa_build_buffer_mode37()` (NEU) 1:1 nach.
Unter `gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined
-fno-sanitize-recover=undefined` bei `-O0`/`-O1`, identisches Ergebnis:

```
BF1 cases: 120, BF2 cases: 120, total checks: 1320, failed: 0
All expected outcomes confirmed: BF1 round-trips today; BF2 round-trips ONLY with the NEU
fix; ALT (status quo) mismatches on every BF2 case (exit 0).
```

Methodik: für 120 synthetische `(serial, counter, button)`-Tupel je Kernel wird ein
"echter Sender" simuliert, indem mit dem jeweils echten Schlüsselplan vorwärts
TEA-verschlüsselt wird. Das Ergebnis wird danach durch die **unveränderte, wörtlich
übernommene** BF1/BF2-Erfolgsprüfung aus `psa_brute_force_run()` geschickt (bestätigt, dass
das hier verwendete BF2-Format von der echten, unveränderten Such-/Prüflogik als gültiger
Fund akzeptiert wird — kein reiner Zirkelschluss der eigenen Annahmen), die gefundenen
Felder werden zurück durch den Encoder-Pfad geschickt, und das neu erzeugte Schlüsselmaterial
wird byteweise mit dem Original verglichen. BF1 rundet in allen 120 Fällen korrekt (Status
quo, Regressionsnachweis); BF2 rundet mit `ALT` (heutiger, einziger Encoder-Pfad) in
**keinem** der 120 Fälle korrekt, mit `NEU` in allen 120. Eine Mutationsprobe (BF2-XOR in
`^` → `&` geändert) lässt gezielt die 4 BF2/NEU-spezifischen Prüfungen je Fall (480 von 1320)
scheitern, keine BF1-Prüfung — das Testgeschirr ist also nicht pauschal-, sondern
kernel-spezifisch empfindlich.

Wer zusätzlich am Gerät prüfen will: ein per BF2 bereits entschlüsseltes Signal (siehe
Testhinweis in `psa-bf2-counter-crc-buffer8-conflation-fix.md`, wie man gezielt einen
BF2-Treffer statt eines BF1-Treffers erzwingt) erneut senden/manuell mit geändertem Zähler
neu erzeugen und gegen eine Gegenstelle prüfen, die den echten Schlüssel kennt — vor dem Fix
wird das Signal nicht akzeptiert, nach dem Fix schon. Dieser Gerätetest ist deutlich
aufwendiger als der Host-Repro (setzt eine echte PSA-Gegenstelle mit bekanntem BF2-Schlüssel
voraus) und daher hier nicht der primäre Nachweis.

## Zusätzlich geprüft, kein Fund / kein Eingriff nötig

- `subghz_protocol_decoder_psa_get_string()`s `decrypted_type == 0x36`-Zweig
  (`protocols/psa.c`, Decoder-Seite): dieser Zweig ist auf dem Live-Decoder-Pfad bereits vor
  diesem Fund unerreichbar — `subghz_protocol_decoder_psa_deserialize()` liest `Type` aus
  der Datei gar nicht ein, und `psa_decrypt_router()` setzt `decrypted_type` für Modus 0x36
  nirgends (bleibt bei der Initialisierung auf 0). Die tatsächliche Anzeige nach einem
  Bruteforce-Fund kommt ausschließlich aus dem eigenen Format-String des Plugins
  (`apply_success_to_history()` in `scenes/plugins/protopirate_psa_bf_plugin.c`), nicht aus
  dieser Funktion. Bewusst unverändert gelassen (Encoder- und Decoder-Instanz führen hier
  ohnehin getrennten Zustand) statt toten Code um einen `0x37`-Zweig zu erweitern.
- `defines.h`: `ENABLE_EMULATE_FEATURE` ist entgegen einer Angabe in der vorherigen Sitzung
  (`psa-bf2-counter-crc-buffer8-conflation-fix.md`, Abschnitt "Zusätzlich geprüft") in der
  eigenen Basis tatsächlich **aktiv** (`#define ENABLE_EMULATE_FEATURE` in `defines.h` Zeile
  7) — der gesamte Encoder-Pfad (`subghz_protocol_psa_encoder`, `psa_build_buffer_mode36()`
  und jetzt `psa_build_buffer_mode37()`) ist damit live kompilierter, nicht toter Code. Reine
  Protokollkorrektur zur vorherigen Sitzung, kein eigener Fund in dieser Sitzung.
- `psa_build_buffer_mode23()` (direkter XOR-Modus): nutzt weder Bruteforce-Kernel noch
  Typkennzeichnung aus diesem Fund, unverändert und nicht betroffen.

## Quelle

Eigene Quelltextprüfung von `G4MEOVER18/ProtoPirate` (eigener Fork/Stand, `e3d9da5`,
unverändert seit den letzten PSA-Sitzungen): `protocols/psa.c`
(`psa_build_buffer_mode36()`, `psa_encoder_build_upload()`,
`subghz_protocol_encoder_psa_deserialize()`, `subghz_protocol_decoder_psa_get_string()`),
`protocols/psa_crypto_bf.c` (`psa_brute_force_run()`,
`psa_bf_fill_state_from_buffer()`), `protocols/psa_crypto.h` (Kernel-Konstanten,
Schlüsselpläne), `defines.h` — reiner Logikfund im eigenen Code, kein externer
Quellenvergleich nötig.
