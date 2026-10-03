# i2c Tools — Sniffer data_index-Überlauf (Port-Vorschlag inkl. Fix)

Aufgegriffen aus dem in `ideas/i2ctools-app-status.md` ("Quelltext-Review (Speichersicherheit)
und gefundene Einschränkung") bereits dokumentierten, aber damals bewusst ohne eigenen
Fix-Vorschlag zurückgestellten Fund im I2C-Sniffer von `NaejEL/flipperzero-i2ctools`. Der
dortige Verzicht war eine Fehleinordnung als reine Dritt-App-Einschränkung (analog zur
Abtastraten-Diskrepanz beim GPIO Logic Analyzer) — tatsächlich handelt es sich aber um dieselbe
Fehlerklasse (fehlende Kapazitätsprüfung vor einem Pufferzugriff), die bei allen anderen
`applications/external`-Funden in diesem Repo (NFC Sniffer, NFC Fuzzer, Mifare Fuzzer, NFC
Comparator, NFC/RFID Detector, NFC Canary) konsequent einen Port-Vorschlag inkl. Fix erhalten
hat. Für diese Sitzung nachgeholt, statt eine neue Codefläche zu öffnen.

## Was
Repo: `NaejEL/flipperzero-i2ctools` (GPL-3.0), Commit `1840c30` (unverändert seit der letzten
Prüfung). Datei `i2csniffer.c::SCLcallback()` — der rein passive I2C-Bus-Mitschnitt über zwei
GPIO-Interrupt-Callbacks (SCL: `GpioModeInterruptRise`, SDA: `GpioModeInterruptRiseFall`).

Jedes `i2cFrame`-Element (`frames[MAX_RECORDS]`, `MAX_RECORDS = 128`) besitzt feste
`data[MAX_MESSAGE_SIZE]`/`ack[MAX_MESSAGE_SIZE]`-Arrays (`MAX_MESSAGE_SIZE = 128`) sowie ein
`uint8_t data_index`-Feld, das angibt, welches Byte des aktuellen Frames gerade befüllt wird.
Bei jeder steigenden SCL-Flanke liest `SCLcallback()` entweder ein Datenbit (`bit_index < 8`)
oder, nach acht Datenbits, das ACK/NACK-Bit — und zählt dabei `data_index` unbedingt hoch:

```c
uint8_t frame = i2c_sniffer->frame_index;
uint8_t bit = i2c_sniffer->frames[frame].bit_index;
uint8_t data_idx = i2c_sniffer->frames[frame].data_index;
if(bit < 8) {
    i2c_sniffer->frames[frame].data[data_idx] <<= 1;
    i2c_sniffer->frames[frame].data[data_idx] |= (int)furi_hal_gpio_read(pinSDA);
    i2c_sniffer->frames[frame].bit_index++;
} else {
    i2c_sniffer->frames[frame].ack[data_idx] = !furi_hal_gpio_read(pinSDA);
    i2c_sniffer->frames[frame].data_index++;   // <- keine Prüfung gegen MAX_MESSAGE_SIZE
    i2c_sniffer->frames[frame].bit_index = 0;
}
```

Anders als `frame_index` (wird in `SDAcallback()` bei jeder Start-Bedingung erhöht und *vor*
jedem Gebrauch gegen `MAX_RECORDS` geprüft — bei Überlauf `clear_sniffer_buffers()`) gibt es für
`data_index` keinen entsprechenden Reset- oder Guard-Pfad. `data_index` bleibt für die gesamte
Dauer einer einzelnen I2C-Transaktion konstant demselben Frame-Slot zugeordnet (`frame_index`
ändert sich nur bei Start/Stop, nicht innerhalb von `SCLcallback()`).

Eine einzelne I2C-Transaktion mit mehr als `MAX_MESSAGE_SIZE` (128) übertragenen Bytes ohne
zwischenzeitliche Stop-/Start-Bedingung — z. B. ein EEPROM-/Flash-Sequential-Read mit großem
Blockzähler, oder ein absichtlich so konstruiertes Zielgerät — lässt `data_idx` auf 128 wachsen
und indiziert `data[]`/`ack[]` damit außerhalb ihrer deklarierten Grenzen (gültiger Bereich
0–127). Da `data_index` ein `uint8_t` ist, wächst der Index dabei bis 255 weiter (danach
Überlauf-Umlauf auf 0), bevor die Transaktion durch eine Stop-Bedingung beendet wird — der
betroffene Speicherbereich bleibt dabei auf das `data[]`+`ack[]`-Feldpaar desselben Frame-Slots
begrenzt (`data`+`ack` liegen als zwei direkt aufeinanderfolgende Byte-Arrays im selben
`i2cFrame`, zusammen exakt 256 Byte groß — der Index deckt also beide ab, ohne in den nächsten
Frame-Slot vorzudringen). Mit `-fsanitize=undefined` (Array-Bounds-Prüfung, Teil der
`undefined`-Gruppe) ist der OOB-Zugriff direkt als "index out of bounds for type 'uint8_t
[128]'" nachweisbar — siehe Host-Repro unten.

Da der Callback in einem GPIO-EXTI-Interrupt-Kontext läuft, ist der praktische Effekt auf
Hardware eine stille Verwechslung von Datenbytes und ACK-Flags innerhalb des betroffenen Frames
(kein kontrollierter Fehler), nicht notwendigerweise ein sofortiger Absturz — abhängig vom
exakten Timing und davon, ob die App zwischenzeitlich auf denselben Speicher zugreift.

## Nutzen
Der I2C-Sniffer ist laut `ideas/i2ctools-app-status.md` das einzige bereits katalogisierte
Werkzeug, das einen laufenden I2C-Bus-Mitschnitt direkt auf dem Flipper-Display anzeigt (ohne
PC-Kopplung wie beim GPIO Logic Analyzer). Der Fix macht das Werkzeug robust gegen genau den
Fall, den eine Sniffer-Anwendung typischerweise antrifft: ein Zielsystem mit einer einzelnen,
langen I2C-Transaktion (sequentielles Lesen eines größeren Speicherblocks) — ohne Fix kann
dieser alltägliche Fall die App während eines autorisierten Hardware-Assessments zum Absturz
bringen oder die angezeigten Mitschnittdaten verfälschen, genau in dem Moment, in dem die
Mitschnittdaten gebraucht werden.

## Betroffene Dateien
- `i2csniffer.c::SCLcallback()` — einzige Änderung.

Geprüft und nicht betroffen: `clear_sniffer_buffers()` (iteriert immer bis `MAX_MESSAGE_SIZE`,
unabhängig von `data_index`, kein Fund), `SDAcallback()` (eigener, bereits korrekt gegen
`MAX_RECORDS` geprüfter Zähler `frame_index`), `i2cscanner.c`/`i2csender.c` (feste
`uint8_t[MAX_I2C_ADDR + 1]`/`uint8_t[2]`-Arrays, Schleifenindizes bleiben nachweislich innerhalb
ihrer Grenzen, kein Fund — bereits in der vorherigen Sitzung geprüft).

## Risiko
Sehr gering. Eine zusätzliche, rein additive Grenzprüfung (`if(data_idx >= MAX_MESSAGE_SIZE)
return;`) vor dem bestehenden Code, keine Änderung an Struktur, Protokoll-Logik oder
GPIO-Konfiguration. Für jede Transaktion mit ≤ 128 Byte (der bisher einzige in der Praxis
funktionierende Fall, siehe Host-Repro-Äquivalenztest) identisches Verhalten zur unveränderten
Fassung. Für Transaktionen > 128 Byte werden weitere Bits/Bytes dieses einen Frames nicht mehr
aufgezeichnet (der Frame "friert" bei 128 Byte ein), statt die ohnehin nicht mehr darstellbare
Information (die App zeigt pro Frame nur `MAX_MESSAGE_SIZE` Byte an) durch wild überschriebene
ACK-Flags/Datenbytes zu verfälschen oder die App abstürzen zu lassen.

## Port-Skizze
Patch in `firmware-mining/i2ctools-sniffer-data-index-oob-fix/i2csniffer_c.patch`:

```diff
@@ -72,6 +72,12 @@ void SCLcallback(void* _i2c_sniffer) {
     uint8_t frame = i2c_sniffer->frame_index;
     uint8_t bit = i2c_sniffer->frames[frame].bit_index;
     uint8_t data_idx = i2c_sniffer->frames[frame].data_index;
+    if(data_idx >= MAX_MESSAGE_SIZE) {
+        // Frame already holds MAX_MESSAGE_SIZE bytes (a single transaction without an
+        // intervening Stop/Start condition exceeded the fixed data[]/ack[] capacity) --
+        // stop recording further bits for this frame instead of indexing past data[]/ack[].
+        return;
+    }
     if(bit < 8) {
         i2c_sniffer->frames[frame].data[data_idx] <<= 1;
         i2c_sniffer->frames[frame].data[data_idx] |= (int)furi_hal_gpio_read(pinSDA);
```

`git apply --check` vorab gegen eine frische, unabhängige Kopie von
`NaejEL/flipperzero-i2ctools` (Commit `1840c30`, derselbe Stand wie in
`ideas/i2ctools-app-status.md` verifiziert) geprüft, der Patch real angewendet und
byte-für-byte mit der beabsichtigten Fassung abgeglichen — keine Abweichung.

## Testhinweis
Host-Repro (`SCLcallback()` 1:1 transkribiert, GPIO-Lesewert durch einen Funktionsparameter
ersetzt, Struct-Layout unverändert) in
`firmware-mining/repro-i2ctools-sniffer-data-index-oob/repro.c`. Fünf Szenarien:
1. Regression bei 20 Byte (weit unter Kapazität).
2. Grenzfall bei exakt 128 Byte (volle Kapazität, noch kein Überlauf) — ALT/NEU identisch.
3. Fund: 129. Byte auf dem letzten Frame-Slot (`frame_index = MAX_RECORDS - 1`) — eigener
   `fork()`-Kindprozess, da `-fno-sanitize-recover=undefined` den Fund fatal macht.
4. Fix: NEU mit 129. Byte plus 300 weiteren Taktpulsen — `data_index` bleibt bei 128
   eingefroren, kein Crash, das letzte gültige Byte (Index 127) bleibt unverändert.
5. Äquivalenztest über alle Transaktionslängen 1–128 Byte — ALT/NEU byte-identisch.

Unter `gcc -std=c11 -O0`/`-O1 -Wall -Wextra -fsanitize=address,undefined
-fno-sanitize-recover=undefined` verifiziert: ALT bricht im Fund-Szenario reproduzierbar mit
`UndefinedBehaviorSanitizer: runtime error: index 128 out of bounds for type 'uint8_t [128]'`
ab, NEU übersteht dasselbe Szenario sowie 300 weitere Taktpulse ohne Fehler, Regressions- und
Grenzfall-Szenario sowie der Äquivalenztest liefern byte-identische Ausgabe zwischen ALT und
NEU — 10/10 Einzelprüfungen bestanden, ALLE CHECKS BESTANDEN, keine weiteren ASan-/
UBSan-Meldungen, keine Compiler-Warnungen.

Lokaler Hardware-Test nach dem Port: eigenes I2C-Gerät mit einer Transaktion > 128 Byte ohne
Stop-Bedingung mitschneiden (z. B. ein EEPROM mit Sequential-Read über mehr als 128 Byte und
deaktiviertem Auto-Increment-Stop, oder ein künstlich langer Sender-Testlauf) — App darf dabei
weder abstürzen noch die ersten 128 Byte des Mitschnitts verfälschen. Für Transaktionen ≤ 128
Byte (der in `ideas/i2ctools-app-status.md` für die lokale Bauprobe vorgesehene
Normalfall: bekanntes EEPROM/RTC-Modul) ist laut Host-Repro keine Verhaltensänderung zu
erwarten.
