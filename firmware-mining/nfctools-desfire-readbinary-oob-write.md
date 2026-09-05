# NFC Tools — Fortsetzung des NFC-Werkzeug-Clusters: DESFire-NDEF-Pfad (T4T READ BINARY)

## Was
Ich habe den in `firmware-mining/nfctools-app-port.md` als "nur begonnen" zurückgestellten
NFC-Tools-Cluster (`applications/external/nfctools/`, Autor `wakdev`, `fap_version` 1.3, Quelle
`github.com/wakdev/nfctools-fz`) fortgesetzt und diesmal `core/nfc_tools_mfc.c` (687 Zeilen),
`core/nfc_tools_ntag.c` (110 Zeilen), `core/nfc_tools_app.c` (120 Zeilen), `core/nfc_tools_desfire.c`
(1060 Zeilen) sowie zur Kontextprüfung `scenes/read/nfc_tools_scene_read_tag.c` (432 Zeilen) und
`scenes/other/nfc_tools_scene_sector_analysis.c` (398 Zeilen) vollständig gelesen — zusammen rund
2800 weitere Zeilen des Clusters. Damit sind Mifare-Classic-Lese-/Schreib-/Format-/Dump-Pfad und
NTAG-Format-/Dump-Pfad jetzt mitgeprüft; offen bleiben `core/nfc_tools_icode.c` (635 Zeilen), die
~35 Schreib-Szenen sowie die Passwort-Entfernung.

## Fund: `t4t_read_binary()` schreibt eine kartenkontrollierte Antwortlänge ungeprüft in feste Puffer (Stack- und Heap-Overflow)
`t4t_read_binary()` (`core/nfc_tools_desfire.c:108`) kapselt jede ISO7816-4 READ-BINARY-APDU im
DESFire-NDEF-Pfad (T4T CC-Datei, NDEF-Länge, NDEF-Nutzdaten):

```c
static bool t4t_read_binary(
    Iso14443_4aPoller* poller, uint16_t offset, uint8_t length, uint8_t* out, size_t* out_len) {
    const uint8_t cmd[] = {0x00, 0xB0, (uint8_t)(offset >> 8), (uint8_t)(offset & 0xFF), length};
    ApduResp r = apdu_exchange(poller, cmd, sizeof(cmd));
    if(!sw_ok(&r) || r.len < 2) return false;
    size_t dlen = r.len - 2;
    if(out_len) *out_len = dlen;
    if(out && dlen) memcpy(out, r.data, dlen);   // <-- dlen nie gegen `length` begrenzt
    return true;
}
```

`length` ist das vom Aufrufer gewählte `Le`-Feld der C-APDU **und** zugleich implizit die Größe des
mitgegebenen `out`-Puffers — bei jeder Aufrufstelle stimmen beide exakt überein (siehe unten). Die
Funktion prüft aber nirgends, ob `dlen` (aus der tatsächlichen R-APDU-Länge berechnet, die der
`apdu_exchange()`-Zwischenspeicher `ApduResp.data[258]` intern bereits auf 258 Bytes begrenzt, aber
sonst 1:1 von der Karte übernimmt) diese Größe überhaupt einhält. `Le` ist im ISO7816-4-Sinn nur ein
*Wunsch* des Lesers — nichts auf der RF-/Poller-Seite erzwingt, dass eine kontaktlose Karte sich
daran hält. Eine bösartige oder fehlerhaft implementierte ISO14443-4A-Karte (Proxmark3/ChameleonMini/
Chameleon Ultra/zweiter Flipper im Emulationsmodus mit eigener, für einen autorisierten Funktionstest
präparierter T4T-Firmware) kann auf jede READ-BINARY-Anfrage mit bis zu 256 Nutzdatenbytes antworten,
unabhängig vom angeforderten `length`.

Betroffene Aufrufstellen, alle mit Puffer, dessen Größe exakt dem übergebenen `length` entspricht:

- `core/nfc_tools_desfire.c:720` (Schreibpfad, `nfc_tools_desfire_ndef_write_cb`):
  `uint8_t cc[CC_SIZE];` (**15 Bytes, Stack**) mit `t4t_read_binary(poller, 0, (uint8_t)CC_SIZE, cc, &cc_read)`.
- `core/nfc_tools_desfire.c:739` (derselbe Callback, Reparatur-Retry-Zweig nach fehlgeschlagener
  CC-Validierung): derselbe `cc`-Stack-Puffer, derselbe Aufruf.
- `core/nfc_tools_desfire.c:791`: `uint8_t old_len_raw[2];` (**2 Bytes, Stack**) mit
  `t4t_read_binary(poller, 0, 2, old_len_raw, &old_len_got)`.
- `core/nfc_tools_desfire.c:947` (Lesepfad, `nfc_tools_desfire_ndef_read_cb`): derselbe
  `cc[CC_SIZE]`-Musteraufruf, eigener Stack-Frame.
- `core/nfc_tools_desfire.c:964`: `uint8_t len_bytes[2];` (**2 Bytes, Stack**), analog zu `old_len_raw`.
- `core/nfc_tools_desfire.c:994` (NDEF-Nutzdaten-Chunked-Read): `t4t_read_binary(poller, offset, chunk, ndef_buf + read_total, &got)`
  — `ndef_buf` ist ein **Heap**-Puffer (`malloc(ndef_len)`), `chunk` ist auf den tatsächlich noch
  freien Rest (`ndef_len - read_total`) begrenzt; eine zu lange Antwort schreibt hier über das Ende
  der `malloc`-Allokation hinaus.

Jede dieser sechs Aufrufstellen wird bereits beim bloßen Antippen einer präparierten Karte erreicht:
Der Schreibpfad liest die CC-Datei, sobald "NFC Tools" → NDEF schreiben auf eine DESFire-Karte trifft
(Zeile 720/739 vor jeder eigentlichen Schreiboperation); der Lesepfad liest CC + Längenfeld + Nutzdaten
automatisch bei jedem erfolgreichen DESFire-Scan über `nfc_tools_desfire_read_ndef()`
(aufgerufen aus `scenes/read/nfc_tools_scene_read_tag.c:266`, Teil des normalen "Read"-Ablaufs, kein
Extra-Klick nötig). Ein Angreifer muss also nur eine passend präparierte Karte in Lesereichweite eines
Flippers mit NFC Tools bringen — klassisches "Tag-Drop"-Szenario, hier gegen das Lesegerät statt gegen
den späteren Kartenleser gerichtet.

Auf dem eingebetteten Ziel (STM32WB55, keine MMU/Stack-Canaries in dieser Form) bedeutet ein
Stack-Overflow von bis zu ~241 Bytes (256 − 15) direkten Rücksprungadress-/Stack-Frame-Verderb im
Poller-Callback-Kontext — mit hoher Wahrscheinlichkeit ein sofortiger Absturz (Hard Fault), im
schlechtesten Fall kontrollierter Kontrollflussverderb, da der überschreibende Inhalt vollständig von
der angreifenden Karte stammt.

## Host-Repro
Eigenständiges Host-Repro (kein Flipper/keine NFC-/Poller-Abhängigkeit), das `t4t_read_binary()` auf
ihre Kopierlogik reduziert (ALT = 1:1 wie im Original, NEU = `dlen` zusätzlich auf `length` begrenzt):

```
gcc -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
./repro
```

Tatsächliche Ausgabe (lokal ausgeführt):
```
case 1 (well-behaved, 15/15 bytes) -- ALT: ok=1 len=15 | NEU: ok=1 len=15 | match=1
case 2 (malicious tag, 200 bytes into a 15-byte cc[CC_SIZE]) -- NEU: ok=1 len=15 (expected: truncated to 15, no OOB write)
case 2 -- ALT: ==================...
==ERROR: AddressSanitizer: stack-buffer-overflow ... WRITE of size 200
    ... in t4t_read_binary_old
    ... ==> Memory access at offset 911 overflows this variable 'cc_old'
==ABORTING
```

Fall 1 (Regressionscheck, wohlverhaltene Karte antwortet exakt mit den angeforderten 15 Bytes): ALT
und NEU liefern identisches Ergebnis — der Fix ändert am normalen Anwendungsfall nichts. Fall 2
(bösartige Karte antwortet mit 200 Nutzdatenbytes auf eine Anfrage mit `length=15` in einen
`cc[CC_SIZE]`-Stack-Puffer gleicher Größe wie im Original): NEU begrenzt korrekt auf 15 Bytes ohne
Speicherzugriff außerhalb des Puffers; ALT schreibt deterministisch über das Pufferende hinaus — ASan
meldet das exakt als `stack-buffer-overflow` auf der betroffenen Stack-Variable. Auf echter Hardware
ohne ASan wäre das je nach Stack-Layout ein Absturz oder unkontrollierter Kontrollflussverderb statt
eines kontrollierten Abbruchs.

## Betroffene Dateien
- `core/nfc_tools_desfire.c` — `t4t_read_binary()` (Zeile ~108–121), einziger Fix-Ort; betrifft
  alle sechs oben aufgeführten Aufrufstellen in derselben Datei.

## Risiko
Gering. Eine zusätzliche Vergleichs-/Zuweisungszeile (`if(dlen > length) dlen = length;`) direkt vor
der bestehenden `*out_len`-Zuweisung — keine neue Abhängigkeit, keine geänderte Signatur, kein Eingriff
in `apdu_exchange()` oder die Aufrufstellen selbst. Verhalten bei einer wohlverhaltenen Karte
(Regressionsfall im Repro) bleibt unverändert, da `length` an jeder bestehenden Aufrufstelle bereits
exakt der Größe des mitgegebenen Puffers entspricht bzw. (beim Chunked-Read) den noch freien Rest nie
überschreitet.

## Port-Skizze / Fix
In `t4t_read_binary()`, direkt nach der Berechnung von `dlen`, eine Begrenzung auf `length` einfügen:
```c
size_t dlen = r.len - 2;
if(dlen > length) dlen = length;   // NEU: nie mehr als vom Aufrufer angefordert/vorgehalten
if(out_len) *out_len = dlen;
if(out && dlen) memcpy(out, r.data, dlen);
```
Kein Fix an `apdu_exchange()` nötig (deren eigener 258-Byte-Puffer ist bereits korrekt begrenzt) und
keiner an den sechs Aufrufstellen — die zentrale Begrenzung in der einen gemeinsam genutzten Funktion
schützt alle gleichzeitig, exakt nach demselben Ein-Stellen-Fix-Muster wie beim bereits gelieferten
`nfc_tools_ndef.c`-Fund derselben App.

## Testhinweis (lokal nachvollzogen)
`firmware-mining/repro-nfctools-desfire-readbinary-oob-write/repro.c`, host-kompilierbar, keine
Flipper-/NFC-Abhängigkeiten (Befehl und tatsächliche Ausgabe siehe Host-Repro oben).

Für den lokalen Funktionstest an echter Hardware: einen zweiten, für Tests kontrollierten
ISO14443-4A-Emulator (Proxmark3/ChameleonMini/Chameleon Ultra, oder ein zweiter Flipper mit eigener
Testfirmware) so konfigurieren, dass er auf jede `00 B0 xx xx Le`-APDU unabhängig von `Le` mit einer
festen, längeren Nutzlast (z. B. 200 Bytes Füllwert + `90 00`) antwortet, sich dabei aber weiterhin
als DESFire/T4T meldet (ATS/SAK entsprechend), und die Karte anschließend mit "NFC Tools" → Read
scannen. Vor dem Fix: Absturz/undefiniertes Verhalten beim automatischen NDEF-Lesepfad (bereits beim
CC-Read, vor jeder Nutzerinteraktion). Nach dem Fix: NFC Tools bricht die NDEF-Erkennung sauber ab
(ungültige/verkürzte CC bzw. Längenfeld) oder zeigt ggf. verstümmelte, aber pufferbegrenzte Daten an
— kein Absturz.

## Zusätzlich geprüft, kein Fund
- `apdu_exchange()`: `tx`/`rx`-`BitBuffer` werden exakt auf `cmd_len` bzw. 258 Bytes alloziert;
  `resp.len` wird vor dem `memcpy` in den festen `ApduResp.data[258]`-Puffer bereits korrekt auf
  `sizeof(resp.data)` begrenzt — dieser eine `memcpy` ist sicher, der Fund liegt ausschließlich in der
  fehlenden zweiten Begrenzung in `t4t_read_binary()`.
- `t4t_update_binary()`: `length` ist an jeder Aufrufstelle bereits vor dem Aufruf auf `APDU_MAX_LC`
  (54) begrenzt (`if(chunk > APDU_MAX_LC) chunk = APDU_MAX_LC;` bzw. feste Konstanten ≤ 54), der
  Ziel-Puffer `cmd[5 + APDU_MAX_LC]` ist entsprechend dimensioniert — kein Fund.
- `desfire_create_ndef_app()`/`desfire_ensure_ndef_files()`: alle APDU-Antworten werden ausschließlich
  über SW1/SW2 (letzte zwei Bytes) ausgewertet, nie über Nutzdaten-Kopien mit variabler, kartenkontrol-
  lierter Länge — kein Fund.
- `nfc_tools_desfire_write_ndef()`s TLV-Stripping (Zeile 852ff.): die 1-/3-Byte-Längenfelder werden
  stets korrekt gegen `ndef_size` geklammert (`if(rlen > ndef_size - N) rlen = ndef_size - N;`),
  `ndef_size` ist außerdem nie kleiner als für die jeweilige Prüfung nötig (durch die vorausgehenden
  `>= 2`/`>= 5`-Bedingungen abgesichert) — kein Underflow, kein Fund; die Eingabe stammt hier ohnehin
  aus dem eigenen `nfc_tools_ndef_build()`, nicht von der Karte.
- `core/nfc_tools_mfc.c` (687 Zeilen, vollständig gelesen: NDEF-Lese-/Schreibpfad inkl. MAD1/MAD2-
  CRC-8, Format, Memory-Dump): alle Sektor-/Block-Iterationen sind gegen `mf_classic_get_total_sectors_num()`/
  `_get_blocks_num_in_sector()` (feste, nicht kartenabhängige Lookup-Tabellen) begrenzt, `crc_input[31]`
  wird exakt mit 15+16=31 Bytes gefüllt — kein Fund. Einzige Auffälligkeit: `app->mfc_type` wird nur
  beim initialen Scan aus dem SAK abgeleitet (`scenes/read/nfc_tools_scene_read_tag.c:307-314`) und
  bleibt für die separate "Sector Analysis"-Szene bestehen; wird zwischen erstem Scan und Sector
  Analysis eine andere MFC-Karte vorgelegt, arbeitet `nfc_tools_mfc_dump()` mit der falschen
  Sektoranzahl — führt aber nachweislich nur zu einem unvollständigen/verlängerten Dump (feste
  Lookup-Funktionen, keine kartenabhängige Puffergröße), kein Speicherzugriffsfehler; nicht als
  eigener Fund gewertet.
- `core/nfc_tools_ntag.c` (110 Zeilen, vollständig gelesen: Format-Bereichsberechnung, Memory-Dump):
  `mful->page[]` ist firmwareseitig immer auf die maximale Seitenzahl aller bekannten Typen alloziert;
  der Dump-Zugriff ist zusätzlich durch `if(p < pages_read)` vor jedem `mful->page[p].data`-Zugriff
  abgesichert — dieselbe `app->mful_type`-Staleness-Möglichkeit wie bei `mfc_type` oben, mit
  identischem Ergebnis (unvollständige Anzeige, kein OOB) — kein eigener Fund.
- `core/nfc_tools_app.c` (120 Zeilen, App-Lifecycle: alloc/free aller Views): symmetrische
  alloc/free-Paare für jede View, keine doppelte Freigabe, keine fehlende Freigabe gefunden.
- `scenes/read/nfc_tools_scene_read_tag.c`: alle vier Protokollzweige (ISO 15693, DESFire, FeliCa,
  Mifare-Classic/-Ultralight-Standardzweig) prüfen `NFC_TOOLS_WORKER_FLAG_STOP` konsistent vor jedem
  blockierenden Schritt; `uid_len`/`app->uid`-Kopien sind an jeder Stelle mit
  `len < sizeof(app->uid) ? len : sizeof(app->uid)` gegen den festen `uid`-Puffer geklammert — kein
  Fund außerhalb des oben beschriebenen DESFire-Funds.
- `scenes/other/nfc_tools_scene_sector_analysis.c`: FeliCa- und DESFire-Dump-Zweige nutzen
  ausschließlich strukturierte SDK-Zugriffsfunktionen (`simple_array_cget`, feste `spad[14]`-Iteration)
  ohne eigene kartenkontrollierte Kopierschritte — kein Fund.

## Quelle
- `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`, Sparse-Checkout, Stand dieser Sitzung,
  identischer Commit-Stand wie die vorherige NFC-Tools-Sitzung): `applications/external/nfctools/`,
  `fap_author` "wakdev", Original-Quelle `github.com/wakdev/nfctools-fz`, `fap_version` 1.3.
- API-87-Einordnung: der Fund betrifft ausschließlich appeigene Puffer-/Kopierlogik
  (`memcpy`/Größenvergleich) ohne Firmware-API-Bezug — kein firmwareversionsabhängiges
  Kompatibilitätsrisiko; `t4t_select_file`/`apdu_exchange`/`iso14443_4a_poller_send_block` selbst
  bereits in der Vorsitzung gegen `mntm-012` als unauffällig eingestuft (Standardmuster wie bei den
  übrigen NFC-Fremd-Apps).
- Eigener Fund: `core/nfc_tools_desfire.c`, `t4t_read_binary()`, mit Host-Repro bestätigt (siehe
  `firmware-mining/repro-nfctools-desfire-readbinary-oob-write/repro.c`).
- Restlicher Umfang von `nfctools` (`core/nfc_tools_icode.c`, ~35 Schreib-Szenen, Passwort-Entfernung)
  für eine künftige Sitzung offen; der Rückstellpunkt "restlicher NFC-Werkzeug-Cluster: nfctools" ist
  damit weiter fortgeschritten, aber noch nicht vollständig abgeschlossen.
