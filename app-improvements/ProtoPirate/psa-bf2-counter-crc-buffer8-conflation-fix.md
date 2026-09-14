# ProtoPirate — PSA-Bruteforce: BF2-Fund vermischt CRC16-Hochbyte mit Counter-Daten

## Was

Nächsten Schritt im Rückstellpunkt "PSA-Bruteforce-Kernel" gemacht: `protocols/psa_crypto.c`s
reine Puffer-Auf-/Abbau-Helfer (`psa_crypto_setup_byte_buffer()`, `psa_crypto_prepare_tea_data()`,
`psa_crypto_tea_crc()`, `psa_crypto_crc16_bf2()`, `psa_crypto_unpack_tea_result_to_buffer()`) —
bislang als letzter offener Teil des Kernels vermerkt — jetzt vollständig gelesen. Bewusst
weiterhin nicht an den reverse-engineerten TEA-Krypto-Konstanten selbst gearbeitet (bleibt aus
denselben Gründen wie bisher zurückgestellt), sondern an der Stelle, an der ein Bruteforce-Fund
in die für Anzeige/Speicherung genutzten Felder zerlegt wird — das ist unabhängig von der
Kryptokorrektheit vollständig host-seitig verifizierbar: es geht nur darum, *welche Bytes eines
bereits als richtig akzeptierten 80-Bit-Ergebnisses* welchem Feld zugeordnet werden, nicht darum,
ob das Ergebnis selbst korrekt entschlüsselt wurde.

`psa_brute_force_run()` (`protocols/psa_crypto_bf.c`) hat zwei unabhängige Bruteforce-Stufen mit
grundverschiedenen Erfolgsprüfungen auf denselben 80 Bit (`dec_v0`/`dec_v1`):

```c
// BF1 (Zeile 92-94): 8-Bit-CRC, liegt allein im untersten Byte von dec_v1
if((counter & 0xFFFFFF) == (dec_v0 >> 8)) {
    uint8_t crc = psa_crypto_tea_crc(dec_v0, dec_v1);
    if(crc == (dec_v1 & 0xFF)) { ... }
}
```

```c
// BF2 (Zeile 126-138): 16-Bit-CRC, belegt die UNTERSTEN 16 BIT von dec_v1 vollständig
if((counter & 0xFFFFFF) == (dec_v0 >> 8)) {
    psa_crypto_unpack_tea_result_to_buffer(buffer, dec_v0, dec_v1);
    uint8_t crc_buffer[6] = { ... };
    uint16_t crc16 = psa_crypto_crc16_bf2(crc_buffer, 6);
    uint16_t expected_crc = (uint16_t)(dec_v1 & 0xFFFF);   // <-- Byte 8 UND Byte 9
    if(crc16 == expected_crc) { ... }
}
```

Nach `psa_crypto_unpack_tea_result_to_buffer(buffer, dec_v0, dec_v1)` gilt (`protocols/psa_crypto.c`
Zeile 85-94): `buffer[8]` = Byte 1 von `dec_v1` (Bits 15-8), `buffer[9]` = Byte 0 von `dec_v1`
(Bits 7-0). Für BF2 sind das laut `expected_crc` **beide** Bytes der 16-Bit-Prüfsumme — für BF1
ist `buffer[8]` dagegen echte Nutzlast, denn dort prüft nur `dec_v1 & 0xFF` (also nur `buffer[9]`).

Beide Erfolgsfälle rufen aber dieselbe, für BF1 geschriebene Zerlegungsfunktion auf
(`protocols/psa_crypto_bf.c` Zeile 46-55, Aufrufstellen Zeile 96 und 139):

```c
static void psa_bf_fill_state_from_buffer(PsaBfState* state, uint8_t* buffer) {
    state->decrypted_button = (buffer[5] >> 4) & 0xF;
    state->decrypted_serial = ((uint32_t)buffer[3] << 8) | ((uint32_t)buffer[2] << 16) |
                              (uint32_t)buffer[4];
    state->decrypted_counter = ((uint32_t)buffer[7] << 8) | ((uint32_t)buffer[6] << 16) |
                               (uint32_t)buffer[8] | (((uint32_t)buffer[5] & 0xF) << 24);
    state->decrypted_crc = (uint16_t)buffer[9];
    state->decrypted_seed = state->decrypted_serial;
    state->decrypted_type = 0x36;
}
```

Für einen BF2-Fund ist das doppelt falsch:

- `state->decrypted_crc` bekommt nur `buffer[9]` — das obere Byte der tatsächlich geprüften
  16-Bit-Prüfsumme (`buffer[8]`) geht verloren, angezeigt/gespeichert wird nur der untere Rand
  der echten CRC.
- `state->decrypted_counter` übernimmt `buffer[8]` unbedingt als Nutzlast-Byte (Bits 7-0) — für
  BF2 ist das aber kein Nutzlast-Byte, sondern das obere CRC-Byte selbst. Der Counter wird also
  mit einem Prüfsummen-Byte verunreinigt, das mit den eigentlichen Zähler-Daten nichts zu tun
  hat.

Für BF1 (Aufruf Zeile 96) ist die Funktion weiterhin korrekt: dort ist `buffer[8]` tatsächlich
Zähler-Nutzlast und `buffer[9]` allein die vollständige (8-Bit-)Prüfsumme — dieser Pfad/diese
Funktion bleibt vom Fund unberührt.

`decrypted_counter`/`decrypted_crc` sind keine internen Zwischenwerte, sondern werden nach einem
Fund direkt weiterverwendet: in die Verlaufsanzeige geschrieben
(`scenes/plugins/protopirate_psa_bf_plugin.c`, `apply_success_to_history()`, Format-String
`"Cnt:%lX"`) und in die `.sub`-Flipper-Format-Datei persistiert
(`helpers/protopirate_psa_bf_host.c`, `host_patch_flipper_format_on_success()`,
`FF_CNT`/`"CRC"`-Felder) — ein BF2-Fund zeigt und speichert damit nach dem Status quo einen
falschen Counter- und CRC-Wert.

## Nutzen

Schließt den letzten offenen Teil des PSA-Bruteforce-Rückstellpunkts (die reinen
Puffer-Auf-/Abbau-Helfer in `psa_crypto.c` und ihre Verwendung im BF2-Erfolgspfad), ohne die als
riskant eingestufte TEA-Kryptografie selbst anzufassen. Ein erfolgreicher BF2-Fund liefert nach
dem Fix denselben Counter- und CRC-Wert, der auch tatsächlich gegen die 16-Bit-Prüfsumme
verifiziert wurde, statt eines mit dem CRC-Hochbyte verunreinigten Counters und eines auf 8 Bit
verstümmelten CRC-Anzeigewerts.

## Betroffene Dateien

- `protocols/psa_crypto_bf.c` — neue Funktion `psa_bf_fill_state_from_buffer_bf2()`, ein
  geänderter Aufruf (BF2-Erfolgszweig, Zeile 139). `psa_bf_fill_state_from_buffer()` selbst und
  ihr BF1-Aufruf (Zeile 96) bleiben unverändert.

## Patch

```diff
 static void psa_bf_fill_state_from_buffer(PsaBfState* state, uint8_t* buffer) {
     state->decrypted_button = (buffer[5] >> 4) & 0xF;
     state->decrypted_serial = ((uint32_t)buffer[3] << 8) | ((uint32_t)buffer[2] << 16) |
                               (uint32_t)buffer[4];
     state->decrypted_counter = ((uint32_t)buffer[7] << 8) | ((uint32_t)buffer[6] << 16) |
                                (uint32_t)buffer[8] | (((uint32_t)buffer[5] & 0xF) << 24);
     state->decrypted_crc = (uint16_t)buffer[9];
     state->decrypted_seed = state->decrypted_serial;
     state->decrypted_type = 0x36;
 }
+
+// BF2-Erfolgsfall: buffer[8] ist hier das obere Byte der tatsaechlich geprueften 16-Bit-CRC
+// (siehe expected_crc = dec_v1 & 0xFFFF weiter unten), keine Zaehler-Nutzlast wie im
+// BF1-Erfolgsfall -- eigene Zerlegung statt Wiederverwendung der BF1-Funktion.
+static void psa_bf_fill_state_from_buffer_bf2(PsaBfState* state, uint8_t* buffer) {
+    state->decrypted_button = (buffer[5] >> 4) & 0xF;
+    state->decrypted_serial = ((uint32_t)buffer[3] << 8) | ((uint32_t)buffer[2] << 16) |
+                              (uint32_t)buffer[4];
+    state->decrypted_counter = ((uint32_t)buffer[7] << 8) | ((uint32_t)buffer[6] << 16) |
+                               (((uint32_t)buffer[5] & 0xF) << 24);
+    state->decrypted_crc = ((uint16_t)buffer[8] << 8) | (uint16_t)buffer[9];
+    state->decrypted_seed = state->decrypted_serial;
+    state->decrypted_type = 0x36;
+}
```

```diff
             uint16_t crc16 = psa_crypto_crc16_bf2(crc_buffer, 6);
             uint16_t expected_crc = (uint16_t)(dec_v1 & 0xFFFF);
             if(crc16 == expected_crc) {
-                psa_bf_fill_state_from_buffer(state, buffer);
+                psa_bf_fill_state_from_buffer_bf2(state, buffer);
                 state->progress_current = (PSA_CRYPTO_BF1_END - PSA_CRYPTO_BF1_START) +
                                           (counter - PSA_CRYPTO_BF2_START);
                 state->status = PSA_BF_STATUS_FOUND;
                 return;
             }
```

Neue, eigenständige Zerlegungsfunktion statt Umbau der bestehenden — spiegelt exakt die bereits
im selben Quelltext vorhandene BF2-Prüfsemantik (`expected_crc = dec_v1 & 0xFFFF`, beide Bytes).
Einziger Unterschied zur bestehenden Funktion: `decrypted_counter` lässt den Term `| (uint32_t)
buffer[8]` weg (dieses Byte ist hier CRC, keine Nutzlast), `decrypted_crc` liest beide CRC-Bytes
statt nur `buffer[9]`. `decrypted_button`/`decrypted_serial`/`decrypted_seed`/`decrypted_type`
bleiben unverändert, da sie ausschließlich aus `buffer[2]`-`buffer[5]` (v0-Anteil) gebildet
werden, den BF1 und BF2 identisch behandeln. Keine neue API, keine geänderte Struktur, der
BF1-Aufrufort und die BF1-Funktion selbst bleiben unangetastet.

## Risiko

**Sehr gering.** Reine Datenzerlegung nach einem bereits als kryptografisch korrekt akzeptierten
Fund — keine Änderung an der TEA-Kryptografie, der Brute-Force-Suche selbst oder der
Erfolgsprüfung. Die neue Funktion wird ausschließlich am bestehenden BF2-Aufrufort eingesetzt;
der BF1-Pfad (Funktion und Aufrufstelle) ist textuell identisch zum Status quo. Einzige
beobachtbare Verhaltensänderung: nach einem BF2-Fund zeigen/speichern Verlaufseintrag und
`.sub`-Datei einen anderen (den tatsächlich korrekten) Counter- und CRC-Wert als zuvor —
das ist die beabsichtigte Korrektur, keine Nebenwirkung.

## Testhinweis

Rein logisch verifizierbar, keine Hardware nötig (siehe Host-Repro unten, deckt exakt die
Byte-Zerlegung ab, unabhängig davon, ob die TEA-Entschlüsselung selbst korrekt ist). Wer
zusätzlich am Gerät prüfen will: ein PSA-128-Bit-Signal laden, dessen Schlüssel2-Hochbyte den
direkten XOR-Pfad ausschließt (`psa_direct_xor_allowed_by_key2()` liefert `false`, siehe
`protocols/psa.c`) und damit auf `Type:0x36`/Bruteforce zurückfällt, Bruteforce auslösen und
abwarten, bis die **zweite** Stufe (Schlüsselraum `0xF3000000`-`0xF3FFFFFF`, `PSA_CRYPTO_BF2_*`)
einen Treffer liefert (BF1 deckt den Bereich `0x23000000`-`0x23FFFFFF` ab und muss dafür zuvor
erfolglos durchlaufen sein) — vor dem Fix zeigt der Verlaufseintrag einen mit dem CRC-Hochbyte
verunreinigten Counter sowie ein auf 8 Bit verstümmeltes CRC; nach dem Fix stimmen beide Werte
mit der tatsächlich geprüften 16-Bit-Prüfsumme überein. Da BF2-Treffer von den Schlüsseldaten des
konkreten Signals abhängen (nicht bei jedem beliebigen Testsignal reproduzierbar, und beide
Bruteforce-Stufen zusammen können mehrere Minuten laufen), ist der Host-Repro der
zuverlässigere und deutlich schnellere Nachweis.

Host-Repro: `repro-psa-bf2-counter-crc-conflation/repro.c`, baut
`psa_crypto_unpack_tea_result_to_buffer()`, `psa_crypto_crc16_bf2()`, `psa_crypto_tea_crc()`
sowie die aktuelle (`ALT`) und die neue BF2-Funktion (`NEU`) 1:1 nach, unter
`gcc -O1 -Wall -Wextra -fsanitize=address,undefined` verifiziert:

```
20 Faelle, ALT falsches CRC in 19/20, ALT verunreinigter Counter in 19/20, NEU fehlerhaft in 0/20

Alle Kernaussagen bestaetigt (exit 0).
```

20 synthetische BF2-Funde (5 `dec_v0`-Werte × 4 `dec_v1`-Datenanteile, `dec_v1`s untere 16 Bit
jeweils exakt auf das dazugehörige echte CRC16 gesetzt, sodass die reale BF2-Erfolgsprüfung in
jedem Fall wie bei einem echten Treffer erfüllt ist — vorab per Assertion gegengeprüft): `ALT`
liefert in 19 von 20 Fällen ein falsches CRC und einen mit dem CRC-Byte verunreinigten Counter
(die eine Ausnahme ist der entartete Alles-Null-Fall, bei dem das CRC16-Hochbyte zufällig 0 ist
und der Fehler dadurch unsichtbar bleibt); `NEU` liefert in allen 20 Fällen exakt die tatsächlich
geprüfte 16-Bit-Prüfsumme und einen unverunreinigten Counter. Zusätzlich ein
BF1-Regressionscheck über 16 synthetische BF1-Funde (eigene, aus `psa_crypto_tea_crc()`
abgeleitete Erfolgsprüfung): `psa_bf_fill_state_from_buffer()` (unverändert, weiterhin für BF1
im Einsatz) liefert in allen 16 Fällen weiterhin `buffer[8]` korrekt als Counter-Nutzlast und
`buffer[9]` allein als vollständiges CRC — keine Regression durch die neue, nur für BF2
eingesetzte Funktion.

## Zusätzlich geprüft, kein Fund

- `psa_crypto_setup_byte_buffer()`: Byte-Reihenfolge einzeln nachgerechnet (`key1_high`
  Big-Endian in `buffer[0..3]`, `key1_low` Big-Endian in `buffer[4..7]`, `key2_low` Big-Endian
  in `buffer[8..9]`) — konsistent mit der einzigen Gegenstelle, die dieselbe Kodierung wieder
  einliest (`psa_crypto_prepare_tea_data()`), an beiden Aufrufstellen (`psa_crypto_bf.c` Zeile
  59, `psa.c` Zeile 732/770) identisch verwendet.
- `psa_crypto_prepare_tea_data()`: liest `buffer[2..5]` nach `w0`, `buffer[6..9]` nach `w1` —
  exakt spiegelbildlich zu `psa_crypto_unpack_tea_result_to_buffer()`s Schreibreihenfolge
  (`buffer[2..5]` aus `v0`, `buffer[6..9]` aus `v1`), Rundlauf über alle vier Bytepaare per
  Hand nachgerechnet, kein Bruch.
- `psa_crypto_tea_crc()`: reine Byte-Summe über `v0` (4 Bytes) und die oberen 3 Bytes von `v1`
  — bewusst ohne das unterste `v1`-Byte, das ja selbst das Ergebnis der Prüfsumme ist; als reine
  Prüfsummenfunktion ohne kryptografischen Anspruch nachvollzogen, kein Bruch mit der einzigen
  Aufrufstelle (BF1-Erfolgsprüfung).
- `psa_crypto_crc16_bf2()`: Standard-CRC16/Polynom `0x8005`, MSB-first, Init `0`, Byte für Byte
  gegen eine unabhängige Referenzimplementierung im Host-Repro nachgerechnet (siehe
  `real_bf2_crc16()`), keine Abweichung.
- `ENABLE_EMULATE_FEATURE`-Zweig (`psa_crypto_tea_encrypt()`): in der eigenen Basis nicht aktiv
  (`defines.h` setzt das Makro nicht), rein informativ gelesen, kein Fund im aktiven Codepfad.

## Quelle

Eigene Quelltextprüfung von `G4MEOVER18/ProtoPirate` (eigener Fork/Stand, `e3d9da5`, unverändert
seit den letzten PSA-Sitzungen): `protocols/psa_crypto.c`, `protocols/psa_crypto.h`,
`protocols/psa_crypto_bf.c`, `helpers/protopirate_psa_bf_host.c` (Verwendungsstellen von
`decrypted_counter`/`decrypted_crc`), `scenes/plugins/protopirate_psa_bf_plugin.c`
(Verlaufsanzeige-Format) — reiner Logikfund im eigenen Code, kein externer Quellenvergleich
nötig.
