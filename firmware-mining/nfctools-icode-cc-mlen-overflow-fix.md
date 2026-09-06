# NFC Tools — Fortsetzung des NFC-Werkzeug-Clusters: ICODE/ISO15693-NDEF-Schreibpfad (CC.MLEN-Überlauf)

## Was
Ich habe den NFC-Tools-Cluster (`applications/external/nfctools/`, Autor `wakdev`, `fap_version` 1.3,
Quelle `github.com/wakdev/nfctools-fz`) fortgesetzt und diesmal `core/nfc_tools_icode.c` (635 Zeilen,
letzter noch offener Einzelkandidat aus dem Cluster neben den ~35 Schreib-Szenen und der
Passwort-Entfernung) vollständig gelesen — ICODE/ISO15693-Format-, Dump- und NDEF-Schreibpfad, jeweils
mit eigenständigem Zweig für generische ISO15693-3-Tags und für NXP-SLIX/SLIX2-Tags (Passwortschutz).

## Fund: `iso15693_write_ndef_blocks()` schreibt bei großen Tags einen falschen, unbemerkt umlaufenden CC.MLEN-Wert
`iso15693_write_ndef_blocks()` (`core/nfc_tools_icode.c:397`) erzeugt vor dem eigentlichen NDEF-Schreiben
die NFC-Forum-Type-5-Tag-Capability-Container (CC), die jedem konformen Lesegerät mitteilt, wie viele
Bytes am Tag als NDEF-Speicherbereich nutzbar sind:

```c
size_t user_capacity = (size_t)(block_count - 1) * block_size;
if(ndef_size > user_capacity) {
    // "Tag too small!" -- korrekt behandelt
    return false;
}

uint8_t mlen = (uint8_t)(((uint32_t)(block_count - 1) * block_size) >> 3);
const uint8_t cc_expected[4] = {0xE1, 0x40, mlen, 0x00};
```

Das vierte CC-Byte (`MLEN`, "Memory Length") ist im **kurzen** NFC-Forum-Type-5-Tag-CC-Format
(`E1 40 MLEN 00`, das diese Funktion ausschließlich implementiert) ein einzelnes Byte: `MLEN * 8` Bytes
nutzbarer NDEF-Speicher, maximal `255 * 8 = 2040` Bytes darstellbar. Für die volle
Tag-Speicherkapazität (`(block_count - 1) * block_size`, unabhängig von der tatsächlich geschriebenen
`ndef_size`) wird `mlen` aber als 32-Bit-Wert berechnet und danach **ungeprüft** auf `uint8_t`
gecastet — überschreitet die wahre Kapazität in Achtel-Bytes die 255, läuft der Wert einfach um
(`true_mlen % 256`), statt entweder auf 0xFF geklammert oder als Fehler behandelt zu werden. Der
bereits vorhandene `user_capacity`-Vergleich schützt hiervor nicht, da er die tatsächliche
Nachrichtengröße (`ndef_size`) gegen die volle Kapazität prüft, nicht die CC-Kodierbarkeit der vollen
Kapazität selbst — der Bug tritt also unabhängig davon auf, wie klein die geschriebene NDEF-Nachricht
ist, sobald der Tag selbst groß genug ist.

ISO15693-3 erlaubt pro Spec sowohl `block_count` (1-Byte-Feld, bis 256 Blöcke bei mir/nfctools über die
nicht-erweiterte `READ_BLOCK`/`WRITE_BLOCK`-Adressierung, siehe unten) als auch `block_size` (bis zu 32
Byte) unabhängig voneinander — ein Tag mit z. B. 256 Blöcken zu je 9 oder mehr Bytes überschreitet die
2040-Byte-Grenze bereits deutlich, ohne die Spezifikation zu verletzen. Ein konformes Lesegerät (jedes
Gerät außer diesem selbst geschriebenen Tag zurücklesende App) vertraut dem CC-MLEN-Feld, um das Ende
des NDEF-TLV-Bereichs zu bestimmen — ein falsch umgelaufener, zu kleiner MLEN-Wert lässt einen
korrekt geschriebenen, aber am Tag größer als erwartet vorhandenen NDEF-Bereich für jeden anderen Leser
vorzeitig/falsch begrenzt erscheinen (Interoperabilitätsdefekt), ohne dass die schreibende App selbst
einen Fehler meldet — der Rückgabewert der Funktion ist in diesem Fall weiterhin `true`
("bytes written").

## Host-Repro
Eigenständiges Host-Repro (kein Flipper-/NFC-Poller-Bezug), reduziert auf die reine MLEN-Arithmetik aus
`iso15693_write_ndef_blocks()` (ALT = 1:1 wie im Original, NEU = zusätzliche Bereichsprüfung vor dem
Cast):

```
gcc -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
./repro
```

Tatsächliche Ausgabe (lokal ausgeführt):
```
case: ICODE SLIX2-like tag (80 blocks x 4 bytes, 320 bytes total)
  block_count=80 block_size=4 true_mlen=39 (0x27)
  OLD: writes CC.MLEN=0x27 -- correct
  NEW: writes CC.MLEN=0x27

case: exact short-CC boundary (256 blocks x 8 bytes, 2040 bytes user capacity)
  block_count=256 block_size=8 true_mlen=255 (0xFF)
  OLD: writes CC.MLEN=0xFF -- correct
  NEW: writes CC.MLEN=0xFF

case: just over the boundary (256 blocks x 9 bytes, 2295 bytes user capacity)
  block_count=256 block_size=9 true_mlen=286 (0x11E)
  OLD: writes CC.MLEN=0x1E -- WRONG (wrapped, true value does not fit in one byte)
  NEW: refuses NDEF write ("Tag too large for NDEF") instead of a wrong CC

case: large-capacity ISO15693 tag (256 blocks x 32 bytes, 8160 bytes user capacity)
  block_count=256 block_size=32 true_mlen=1020 (0x3FC)
  OLD: writes CC.MLEN=0xFC -- WRONG (wrapped, true value does not fit in one byte)
  NEW: refuses NDEF write ("Tag too large for NDEF") instead of a wrong CC

OK: OLD silently wraps CC.MLEN for over-capacity tags, NEW detects and refuses.
```

Fall 1 (kleiner ICODE-SLIX2-artiger Tag, 80×4 Byte) und Fall 2 (Tag exakt an der 2040-Byte-Grenze,
256×8 Byte) sind Regressionsfälle: ALT und NEU liefern denselben, korrekten CC.MLEN-Wert — der Fix
ändert am normalen, ganz überwiegenden Anwendungsfall (kleine ISO15693/ICODE-Tags) nichts. Fall 3 und 4
(Tags mit größerer Blockgröße bzw. voller 256-Block-Kapazität) zeigen den Umlauf: ALT schreibt einen
kleineren, falschen MLEN-Wert (`0x1E` statt `0x11E`, `0xFC` statt `0x3FC`) unbemerkt in die CC, NEU
erkennt die Überschreitung und bricht das Schreiben mit einer Fehlermeldung ab, statt eine defekte CC
zu hinterlassen.

## Betroffene Dateien
- `core/nfc_tools_icode.c` — `iso15693_write_ndef_blocks()` (Zeile ~397–421), einziger Fix-Ort;
  betrifft beide Aufrufstellen (generischer ISO15693-3-Zweig und SLIX/SLIX2-Zweig, die dieselbe
  gemeinsame Funktion nutzen).

## Risiko
Gering. Eine zusätzliche Bereichsprüfung direkt nach der bestehenden `mlen`-Berechnung, vor deren
erstmaliger Verwendung — keine neue Abhängigkeit, keine geänderte Signatur. Verhalten bei den weit
überwiegenden real vorkommenden ICODE/SLIX-Tags (Speicherkapazität deutlich unter 2040 Byte, siehe
Regressionsfälle im Repro) bleibt unverändert.

## Port-Skizze / Fix
In `iso15693_write_ndef_blocks()`, direkt nach der Berechnung von `mlen`, die volle 32-Bit-Zwischengröße
prüfen, bevor sie auf `uint8_t` reduziert wird:
```c
uint32_t mlen_full = ((uint32_t)(block_count - 1) * block_size) >> 3;
if(mlen_full > 0xFF) {
    furi_string_printf(
        app->info_str,
        "Tag too large!\nNDEF needs ext. CC\n(short CC max %u B)",
        (unsigned)(0xFF * 8));
    return false;
}
uint8_t mlen = (uint8_t)mlen_full;
const uint8_t cc_expected[4] = {0xE1, 0x40, mlen, 0x00};
```
Bewusst als sauberer Fehlerpfad statt als Klammerung auf `0xFF` umgesetzt: eine geklammerte CC würde dem
Lesegerät weiterhin eine falsche (wenn auch nicht umgelaufene) Kapazität vorspiegeln; das eigentliche
Problem — die kurze CC kann Tags über 2040 Byte grundsätzlich nicht korrekt beschreiben — bliebe
bestehen. Eine vollständige Unterstützung der erweiterten (8-Byte-)CC für solche Tags wäre ein separates,
größeres Feature und ist hier bewusst nicht mitportiert.

## Testhinweis (lokal nachvollzogen)
`firmware-mining/repro-nfctools-icode-cc-mlen-overflow/repro.c`, host-kompilierbar, keine
Flipper-/NFC-Abhängigkeiten (Befehl und tatsächliche Ausgabe siehe Host-Repro oben).

Für den lokalen Funktionstest an echter Hardware: einen eigenen ISO15693-3-Tag mit einer über die
`GET_SYSTEM_INFO`-Antwort meldbaren Kombination aus `block_count`/`block_size`, deren Produkt (minus
einem Block für die CC) über 2040 Byte liegt (z. B. über einen zweiten Flipper oder einen anderen
ISO15693-Emulator mit entsprechend konfigurierten Systeminformationen emuliert, da mir kein passendes
reales Tag mit so großer Kapazität vorliegt), mit "NFC Tools" → ICODE → Write NDEF eine kurze
Testnachricht schreiben. Vor dem Fix: Schreibvorgang meldet Erfolg, die geschriebene CC enthält jedoch
einen zu kleinen/falschen MLEN-Wert (mit einem unabhängigen NFC-Forum-Type-5-fähigen Lesegerät
gegenprüfbar: gemeldete NDEF-Größe weicht von der tatsächlich geschriebenen Nachricht ab). Nach dem
Fix: Schreibvorgang bricht mit "Tag too large!" ab, statt eine defekte CC zu hinterlassen.

## Zusätzlich geprüft, kein Fund
- `nfc_tools_icode_format()`/`nfc_tools_iso15693_format_cb()`/`nfc_tools_slix_format_cb()`: iterieren
  blockweise über `b <= 0xFF` (`uint16_t`, kein Umlauf möglich, da die Obergrenze fest bei 255 liegt);
  brechen korrekt bei `ISO15693_3_RESP_ERROR_BLOCK_UNAVAILABLE` bzw. Timeout ab und prüfen
  `NFC_TOOLS_WORKER_FLAG_STOP` vor jedem Block — kein Fund.
- `iso15693_build_write_frame_fmt()`/`iso15693_build_read_frame_fmt()` (nur von den beiden
  Format-Callbacks genutzt) verwenden fest 4 Datenbytes je Block. Das deckt sich mit der bei mir
  (`mntm-012`) üblichen, weit überwiegenden 4-Byte-Blockgröße handelsüblicher ICODE-/SLIX-Tags; ein
  echter Bug wäre das nur bei einem Tag mit abweichender `block_size`, was die Format-Funktion nirgends
  prüft (im Unterschied zu `iso15693_write_ndef_blocks()`, das `block_size` korrekt parametrisiert) —
  als eigenständiger, aber deutlich spekulativerer Kandidat (kein mir bekanntes reales Tag mit
  abweichender Blockgröße in diesem Marktsegment) bewusst nicht als eigener Fund gewertet, für eine
  künftige Sitzung vorgemerkt, falls ein passendes Tag auftaucht.
- `nfc_tools_icode_dump()` (`nfc_tools_iso15693_sector_cb()`): iteriert `b < bc` (`uint16_t`) und castet
  den Index für `iso15693_3_get_block_data(iso, (uint8_t)b)` auf `uint8_t` — zunächst als möglicher
  Index-Umlauf bei `bc > 256` vermutet, dann verworfen: die hier wie bei mir (`mntm-012`) verwendete
  nicht-erweiterte ISO15693-3-Blockadressierung (`READ_BLOCK`/`WRITE_BLOCK` mit einem einzelnen
  Adressbyte, siehe bereits dokumentiertes `ISO15693_3_LISTENER_BUFFER_SIZE`-Limit in
  `firmware-mining/nfc-iso15693-read-multi-blocks-and-parser-overrun-fix.md`) begrenzt `block_count`
  bereits selbst auf maximal 256 (Indizes 0–255, vollständig in `uint8_t` darstellbar) — kein
  erreichbarer Umlauf, kein Fund.
- `nfc_tools_icode_write_ndef()`/die beiden Poller-Callbacks: `bit_buffer_alloc(block_size + 3)` bzw.
  `+4` sind korrekt an die tatsächliche, gelesene `block_size` gekoppelt (im Unterschied zu den
  Format-Callbacks); `block_count < 2 || block_size < 4`-Prüfung vor jedem Zugriff vorhanden — kein
  Fund außerhalb des oben beschriebenen CC.MLEN-Funds.
- SLIX-Passwort-Handling (`slix_poller_get_random_number()`/`slix_poller_set_password()` in Format und
  NDEF-Write): identisches Muster in beiden Funktionen, Passwort wird nur bei tatsächlich
  schreibgeschützten Blöcken angefordert — kein Fund.
- `malloc(num_blocks * block_size)` in `iso15693_write_ndef_blocks()`: `num_blocks` ist über die bereits
  geprüfte `ndef_size <= user_capacity`-Schranke indirekt begrenzt (`block_count`/`block_size` sind
  `uint16_t`/`uint8_t`, das Produkt passt ohne Überlauf in `size_t` auf der Zielplattform) — kein Fund.

## Quelle
- `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`, Sparse-Checkout, Stand dieser Sitzung,
  identischer Commit-Stand wie die vorherigen NFC-Tools-Sitzungen): `applications/external/nfctools/`,
  `fap_author` "wakdev", Original-Quelle `github.com/wakdev/nfctools-fz`, `fap_version` 1.3.
- API-87-Einordnung: der Fund betrifft ausschließlich appeigene Arithmetik/Kodierlogik (Berechnung
  eines CC-Feldwerts) ohne Firmware-API-Bezug — kein firmwareversionsabhängiges
  Kompatibilitätsrisiko; die zugrundeliegenden `iso15693_3_get_block_count()`/`_get_block_size()`/
  `iso15693_3_poller_send_frame()`- bzw. `slix_poller_*`-Aufrufe selbst bereits in vorherigen
  Sitzungen gegen `mntm-012` als unauffällig eingestuft (gleiches Muster wie beim bereits gelieferten
  DESFire-Fund derselben App).
- Eigener Fund: `core/nfc_tools_icode.c`, `iso15693_write_ndef_blocks()`, mit Host-Repro bestätigt
  (siehe `firmware-mining/repro-nfctools-icode-cc-mlen-overflow/repro.c`).
- Damit ist `core/nfc_tools_icode.c` vollständig geprüft. Restlicher Umfang des Clusters (~35
  Schreib-Szenen für Kontakt/Bitcoin/WLAN/Social/etc., Passwort-Entfernung) für künftige Sitzungen
  offen.
