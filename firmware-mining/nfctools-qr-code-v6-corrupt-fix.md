# NFC Tools — Fortsetzung des NFC-Werkzeug-Clusters: QR-Code-Anzeige erzeugt für lange NDEF-Werte einen unlesbaren Code

## Was
Ich habe den seit mehreren Sitzungen offenen Rückstellpunkt "Tag-Info-/QR-Code-/NDEF-Detail-
Lesepfade" aus dem NFC-Tools-Cluster (`applications/external/nfctools/`, Autor `wakdev`,
`fap_version` 1.3, Quelle `github.com/wakdev/nfctools-fz`) aufgegriffen und
`scenes/read/nfc_tools_scene_tag_info.c` (444 Zeilen, kein Fund — Chip-/ISO-/Speicher-Erkennung
und Submenü-Aufbau sauber implementiert) sowie `scenes/read/nfc_tools_scene_ndef_qr_code.c`
(119 Zeilen, Scene/View-Wrapper, kein Fund) vollständig gelesen. Der eigentliche Fund liegt im
dahinterliegenden Encoder `helpers/qrcode/nfc_tools_qrcodegen.c` (471 Zeilen, laut Kopfkommentar
ein eigenständiger, minimaler QR-Code-Generator für Byte-Modus/ECC-L/Version 1-7, den die
QR-Code-Scene für jeden angezeigten NDEF-Wert aufruft) — vollständig gelesen und mit einem echten
QR-Decoder (nicht nur gegen sich selbst) gegengeprüft.

## Fund: Versionen 6 (und 7) sind bei ECC-L in Wirklichkeit zweiblöckig — der Encoder behandelt sie fälschlich als einen einzigen Reed-Solomon-Block
Der Dateikopf behauptet: *"Only single-block versions (1-7 ECC-L) are supported."* Das stimmt für
Version 1-5, ist für Version 6 und 7 aber falsch: Die QR-Spezifikation (ISO/IEC 18004) teilt bei
ECC-Level L ab Version 6 die Daten in **zwei gleich große Reed-Solomon-Blöcke** auf (Version 6:
2×68 Datencodewörter, Version 7: 2×78), deren Codewörter anschließend spaltenweise interleaved
werden — nicht in einen einzigen Block mit `total_codewords − ecc_per_block` Datenbytes.

`qr_data_cw[8] = {0, 19, 34, 55, 80, 108, 154, 176}` (Zeile 20) behandelt Version 6/7 aber genau
so: `154 = 172(total) − 18(ecc)` bzw. `176 = 196 − 20`, als gäbe es nur einen Block. Die
tatsächliche Datenkapazität (Summe über beide Blöcke) ist laut Spezifikation **136** (Version 6)
bzw. **156** (Version 7) — die Tabelle beansprucht damit 18 bzw. 20 Byte mehr Nutzdaten, als in der
Matrix tatsächlich für Daten reserviert sind (die "fehlenden" Bytes werden in Wirklichkeit als
zweiter ECC-Block benötigt). `nfc_tools_qr_encode()` (Zeile 360) berechnet daraufhin **eine**
Reed-Solomon-ECC-Folge (`rs_remainder(data, nData, gen, nEcc, ecc)`, Zeile 410) über den kompletten,
zu langen `nData`-Block und hängt sie unverändert an — statt die Daten in zwei 68-/78-Byte-Hälften
zu teilen, für jede Hälfte einzeln 18/20 ECC-Bytes zu berechnen und Daten- sowie ECC-Bytes
blockweise zu interleaven. Jede echte QR-Version-6- oder -7-Antwort ist dadurch strukturell
falsch aufgebaut, unabhängig vom tatsächlichen Inhalt.

Betroffen ist jeder NDEF-Wert mit 107 bis 127 Zeichen (`NfcToolsNdefRecord.value[128]`, laut
eigenem Kommentar "truncated to fit" — der praktisch einzige erreichbare Aufrufer dieses
Encoders, siehe `nfc_tools_scene_ndef_qr_code.c:93`): Werte in diesem Bereich wählen laut
Kapazitätstabelle Version 6 (`qr_data_cw[6] >= textLen+2`), und jeder so erzeugte Code ist ab
Werk unlesbar — z. B. jede längere URL, WLAN-Zugangsdaten- oder vCard-NDEF-Nachricht in diesem
Längenbereich. Version 7 ist über diesen Aufrufer wegen der festen 128-Byte-Werte-Obergrenze nie
erreichbar; Versionen 1-5 (bis 106 Zeichen) sind bereits heute korrekt, da ECC-L dort tatsächlich
einblockig ist.

## Host-Repro
Der komplette, unveränderte Encoder (`nfc_tools_qrcodegen.c`, byteidentisch zum Firmware-Baum,
keine Flipper-/NFC-Abhängigkeiten) lässt sich host-seitig bauen. Ein kleiner Harness rendert das
erzeugte Modul-Bitmap als PGM-Bild; ein Standard-QR-Decoder (`pyzbar`/`zbar`) prüft, ob der Code
tatsächlich – wie ein echter Handy-Scanner – zurückgelesen werden kann, statt den Puffer nur
gegen sich selbst zu vergleichen:

```
gcc -O1 -Wall -Wextra -fsanitize=address,undefined nfc_tools_qrcodegen_old.c qr_repro_harness.c -o gen_ppm_old
pip install pyzbar pillow   # + libzbar0 auf dem System
python3 decode_sweep.py ./gen_ppm_old
```

Tatsächliche Ausgabe (lokal ausgeführt, ALT = unveränderter Encoder, 137 zufällige Testfälle über
Textlängen 1-140):
```
n=107: DECODE MISMATCH info='version=6 size=41' got=[]
n=108: DECODE MISMATCH info='version=6 size=41' got=[]
...
n=134: DECODE MISMATCH info='version=6 size=41' got=[]
n=135: expected encode failure, got: version=6 size=41
n=136: expected encode failure, got: version=6 size=41
n=140: expected encode failure, got: version=6 size=41
tested=137 fail=31
```
**Alle 28 getesteten Längen zwischen 107 und 134 Zeichen (jede davon wählt Version 6) liefern
einen von `pyzbar` nicht lesbaren Code — 100 % Fehlschlagsrate für den gesamten betroffenen
Längenbereich**, dazu drei Fälle über 134 Zeichen, die ALT fälschlich noch als Version 6 kodiert
(dazu unten mehr), statt korrekt "QR unavailable" zu melden. Als Gegenprobe: ein mit der
etablierten Python-Referenzbibliothek `qrcode` erzeugter, echter Version-6-Code derselben Länge
wird von genau demselben `pyzbar`-Aufruf anstandslos gelesen — das Encoder-Ergebnis selbst ist
also der Fehler, nicht die Prüfmethode.

Mit dem gefixten Encoder (`nfc_tools_qrcodegen_new.c`, `python3 decode_sweep.py ./gen_ppm_new`):
```
tested=137 fail=0
```
Alle 135 zufälligen Textlängen 1-134 (Versionen 1-6) decodieren wieder exakt auf den
Original-Text; die drei Längen über 134 werden korrekt mit "encode failed" abgelehnt (führt in
der App zum bereits vorhandenen "QR unavailable"-Anzeigezweig statt zu einem stillen Fehlcode).

## Betroffene Dateien
- `helpers/qrcode/nfc_tools_qrcodegen.c` — `qr_data_cw[]` (Zeile 20) und `nfc_tools_qr_encode()`
  (Zeile 360-para., insbesondere die ECC-Berechnung/Zusammensetzung Zeile 406-415), einziger
  Fix-Ort. Keine andere Datei des Clusters ruft diesen Encoder auf.

## Risiko
Kein Speicherzugriffsfehler (mit ASan/UBSan über alle 137 Testfälle bestätigt, ALT wie NEU) —
reiner Korrektheitsbug: Der erzeugte Code ist strukturell falsch und wird von jedem
spezifikationskonformen Reader zuverlässig zurückgewiesen (die Fehlerkorrektur eines einzelnen
falsch dimensionierten Blocks reicht nicht annähernd aus, um den fehlenden zweiten Block zu
kompensieren). Für die Nutzung bedeutet das: kein Absturz, aber ein für den Anwender nicht
nachvollziehbarer, stiller Fehlschlag ("Der QR-Code lässt sich einfach nicht scannen") ausgerechnet
bei den längeren, praktisch relevantesten NDEF-Inhalten (lange URLs, WLAN-Zugangsdaten, vCards).
Der Fix ändert nur die Byteaufteilung/ECC-Berechnung innerhalb dieser einen Funktion, keine
Firmware-API-Fläche, kein Flipper-/NFC-HAL-Bezug — die gesamte Logik ist reines, hostseitig
testbares C ohne Hardwareabhängigkeit.

## Port-Skizze / Fix
Zwei Änderungen an `helpers/qrcode/nfc_tools_qrcodegen.c`:

**1. Korrigierte Datenkapazitätstabelle** (Gesamtdaten über alle Blöcke, nicht `total − ecc` für
einen einzelnen, in Wirklichkeit nicht existierenden Block):
```c
static const uint8_t qr_data_cw[8] = {0, 19, 34, 55, 80, 108, 136, 156};
```

**2. Blockaufteilung + Interleaving in `nfc_tools_qr_encode()`**, ersetzt die bisherige
Einzelblock-ECC-Berechnung:
```c
// ECC-L Versionen 6-7 teilen sich in zwei gleich große RS-Blöcke (68+68 bzw.
// 78+78 Datencodewörter) auf; Version 1-5 bleiben einblockig. Beide Fälle
// gehen bei ECC-L glatt auf, kein ungleicher Rest-/Kurzblock zu behandeln.
int nBlocks = (version >= 6) ? 2 : 1;
int dataPerBlock = nData / nBlocks;
...
static uint8_t gen[30];
rs_divisor(nEccPerBlock, gen);

static uint8_t block_ecc[2][30];
for(int b = 0; b < nBlocks; b++)
    rs_remainder(data + b * dataPerBlock, dataPerBlock, gen, nEccPerBlock, block_ecc[b]);

int pos = 0;
for(int i = 0; i < dataPerBlock; i++)
    for(int b = 0; b < nBlocks; b++)
        tempBuffer[pos++] = data[b * dataPerBlock + i];
for(int i = 0; i < nEccPerBlock; i++)
    for(int b = 0; b < nBlocks; b++)
        tempBuffer[pos++] = block_ecc[b][i];
```
Für `nBlocks == 1` (Version 1-5) ist das Ergebnis identisch zum bisherigen Verhalten (einfache
Konkatenation von Daten- und ECC-Bytes) — reine Erweiterung, keine Verhaltensänderung im bereits
korrekten Bereich, wie der Repro-Sweep für die Längen 1-106 bestätigt.

**Bewusst nicht mitgefixt: Version 7 bleibt deaktiviert.** Beim Nachprüfen von Version 7 zeigte
sich ein zweiter, unabhängiger Fehler: Der `v7`-Zweig der Alignment-Pattern-Platzierung
(`if(version >= 2) { ... } else { draw_align(...6,22); draw_align(...22,6);
draw_align(...22,22); }`) zeichnet nur 3 der laut Spezifikation für Version 7 nötigen 6
Alignment-Patterns (die 3×3-Kombinationen von `{6,22,38}` abzüglich der 3 mit den Finder-Patterns
überlappenden Positionen `(6,6)`/`(6,38)`/`(38,6)` — es fehlen `(22,38)`, `(38,22)`, `(38,38)`).
Das Nachrüsten der fehlenden drei `draw_align()`-Aufrufe allein behebt Version 7 im eigenen
Repro-Sweep **nicht** (weiterhin harter Decode-Fehlschlag bei jeder getesteten Länge ≥ 135
Zeichen) — es steckt dort mindestens ein weiterer, noch nicht identifizierter Fehler. Da
Version 7 über den einzigen tatsächlichen Aufrufer (`NfcToolsNdefRecord.value[128]`, maximal 127
Nutzzeichen) ohnehin nie erreicht wird und Version 6 allein bereits bis 134 Zeichen Kapazität
bietet (mehr als die 127 überhaupt möglichen), begrenzt der Fix die Versionsauswahl-Schleife
bewusst auf `v <= 6` statt einen nur teilweise verifizierten Version-7-Pfad auszuliefern:
```c
for(int v = 1; v <= 6; v++) {
    if((size_t)qr_data_cw[v] >= textLen + 2) { version = v; break; }
}
```
Damit bleibt der ausgelieferte Fix vollständig verifiziert (137/137 Testfälle bestehen); Version 7
bleibt ein offener, aber praktisch folgenloser Rückstellpunkt für eine künftige Sitzung, falls der
Encoder je mit einer größeren Werte-Obergrenze wiederverwendet werden sollte.

## Testhinweis (lokal nachvollzogen)
`firmware-mining/repro-nfctools-qr-v6-corrupt/` — `nfc_tools_qrcodegen_old.c` (unverändert,
byteidentisch zum Firmware-Baum), `nfc_tools_qrcodegen_new.c` (mit obigem Fix),
`nfc_tools_qrcodegen.h`, `qr_repro_harness.c` (PGM-Renderer), `decode_sweep.py`
(Rundlauf-Prüfung über 137 Testfälle via `pyzbar`). Exakte Befehle und tatsächliche Ausgabe siehe
Host-Repro oben.

Für den lokalen Funktionstest an echter Hardware: Fix einbauen (`ufbt launch_app` gegen
`nfctools`), einen eigenen NTAG21x-Tag mit einer NDEF-URL zwischen 107 und 127 Zeichen
beschreiben (z. B. über die App selbst oder ein bereits vorhandenes `.nfc`-File mit passendem
NDEF-Inhalt), den Tag einlesen, "Tag Info" → den betreffenden Record → QR-Code öffnen und mit
einem echten Smartphone scannen. Vor dem Fix lässt sich der angezeigte Code nicht scannen
(Version 6, sichtbar an der Codegröße); nach dem Fix liefert derselbe Scan-Versuch den
Original-Text. Als Regressionstest einen kürzeren NDEF-Wert (< 107 Zeichen, Version ≤ 5) mit
demselben Ablauf gegenprüfen — muss unverändert scannbar bleiben.

## Zusätzlich geprüft, kein Fund
- `nfc_tools_scene_tag_info.c`: Chip-/ISO-/NFC-Typ-Erkennung, Speicherzeilen-Berechnung,
  Submenü-Aufbau (`s_chip_header[48]`/`s_record_labels[8][32]`, beide durchgängig über
  `strlcpy`/`snprintf` mit expliziter Zielgröße befüllt) und Event-Routing
  (`event.event <= app->ndef_record_count`, konsistent mit der in `nfc_tools_ndef.c:346`
  bereits auf `NFC_TOOLS_MAX_NDEF_RECORDS` begrenzten Record-Zählung) — kein Fund.
- `nfc_tools_scene_ndef_qr_code.c`: `rec->value` (128-Byte-Feld, laut eigenem Kommentar
  "truncated to fit") wird unverändert an `nfc_tools_qr_encode()` weitergereicht, das Modell
  (`NfcToolsQrModel`) sowie Zeichenlogik (`nfc_tools_qr_draw_cb()`, Skalierung/Quiet-Zone-Berechnung
  für 128×64) gegen `qr_size()`/`qr_module()` gegengeprüft — kein Fund.
- `nfc_tools_qrcodegen.c`: BCH-Formatinformationsberechnung (`fmt_word()`) gegen die acht
  offiziellen 15-Bit-Referenzwerte für ECC-Level L (Masken 0-7) aus der Spezifikation
  gegengeprüft — alle acht exakt korrekt. Maskenformeln 0-7 (`place_data()`) einzeln gegen die
  Spezifikationsformeln nachgerechnet (inkl. Operator-Präzedenz-Fallstricke bei Maske 5-7) —
  korrekt. Alignment-Koordinaten für Version 2-6 (`qr_align_coord[]`) gegen die offizielle Tabelle
  geprüft — korrekt. Pufferdimensionierung (`QRCODEGEN_BUF_MAX` = 408 Byte trotz irreführendem
  Kopfkommentar "196 bytes" — reiner Dokumentationsfehler, das tatsächliche Makroergebnis ist groß
  genug für jede unterstützte Version) — keine Überlaufmöglichkeit.

## Quelle
- `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`, Sparse-Checkout, Stand dieser
  Sitzung): `applications/external/nfctools/`, `fap_author` "wakdev", Original-Quelle
  `github.com/wakdev/nfctools-fz`, `fap_version` 1.3.
- API-87-Einordnung: reine Host-Logik ohne jeden Flipper-/NFC-HAL-Bezug (kein
  `furi_hal_*`/`nfc_*`-Aufruf im gesamten Encoder) — kein firmwareversionsabhängiges
  Kompatibilitätsrisiko.
- Referenzwerte für die Reed-Solomon-Blockaufteilung sowie die Gegenprobe-QR-Codes zur
  Verifikation stammen aus der etablierten Python-Bibliothek `qrcode` (reine Kapazitäts-/
  Blocktabellen, kein übernommener Code); Decodierung/Verifikation über die Standardbibliothek
  `zbar`/`pyzbar`.
- Eigener Fund: `helpers/qrcode/nfc_tools_qrcodegen.c`, `qr_data_cw[]` sowie ECC-Berechnung in
  `nfc_tools_qr_encode()`, mit Host-Repro bestätigt (siehe
  `firmware-mining/repro-nfctools-qr-v6-corrupt/`).
- Restlicher Umfang des Clusters (~35 Schreib-Szenen für Kontakt/Bitcoin/WLAN/Social/etc.)
  weiterhin für künftige Sitzungen offen; der Rückstellpunkt "Tag-Info-/QR-Code-/NDEF-Detail-
  Lesepfade" ist mit dieser Sitzung abgeschlossen (Version-7-Alignment-Fehler bewusst als eigener,
  praktisch folgenloser Restpunkt oben dokumentiert, nicht separat vorgemerkt, da ohne
  Encoder-Wiederverwendung mit größerer Werte-Obergrenze nicht erreichbar).
