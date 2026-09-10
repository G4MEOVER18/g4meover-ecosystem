# UHF RFID [(Q)M100] — externe App aus RogueMaster-Bündel (YRM100-UART-Ringpuffer liest über die Heap-Grenze hinaus)

## Was
Ich habe aus dem noch offenen Rückstellpunkt "restlicher NFC-Werkzeug-Cluster" den
UHF-RFID-Zweig aufgegriffen: **UHF RFID [(Q)M100]** (`applications/external/uhf_rfid/`, `appid`
`uhf_rfid`, `fap_category` "GPIO", `fap_author` "frux-c", Quelle laut `application.fam`
`github.com/frux-c/uhf_rfid/tree/dev`, `fap_version` 1.0). Die App spricht über den
Erweiterungsport (UART, `FuriHalSerialIdUsart`) ein extern angeschlossenes YRM100-/M100-UHF-
RFID-Modul an (860–960-MHz-EPC-Gen2-Lesegerät, Fremdhardware — deshalb bisher zusammen mit
`chameleon`/`nfc_relay` als "braucht Zweitgerät" zurückgestellt; die Quelltextprüfung selbst
braucht diese Hardware aber nicht). Mit 721 Zeilen über vier Kerndateien
(`uhf_buffer.c`, `uhf_tag.c`, `uhf_worker.c`, `uhf_module.c`) plus UART-/Szenen-Code
vollständig lesbar in einer Sitzung — ich habe den gesamten Nicht-Szenen-Kern gelesen und dabei
den eigenen Empfangs-Ringpuffer (`uhf_buffer.c`, 71 Zeilen) geprüft.

Stromversorgung/Erweiterungsport-Handling selbst sauber: `uhf_app_main()` ruft
`expansion_disable()` vor und `expansion_enable()` nach der UART-Nutzung in korrekter
Reihenfolge auf (identisch zum bereits gegen `mntm-012` verifizierten Muster von `uart_sniff`/
`rogue_ap_detector`), und schaltet die 5-V-OTG-Versorgung für das externe Modul nur dann ab,
wenn sie von der App selbst eingeschaltet wurde (`is_5v_enabled_by_app`-Wächter) — kein Fund in
diesem Teil.

## Fund: `uhf_buffer_get_data()` liest nach einem Pufferüberlauf über das Ende der 250-Byte-Heap-Allokation hinaus
`uhf_module.c` sammelt jede Modul-Antwort in einem einzigen `Buffer` (`uart->buffer`,
`uhf_buffer_alloc(UHF_UART_RX_BUFFER_SIZE)` mit `UHF_UART_RX_BUFFER_SIZE = 250`, `uhf_uart.h`).
Vor jedem Kommando wird der Puffer zurückgesetzt (`uhf_buffer_reset()`), danach füllt die
UART-RX-ISR (`uhf_uart_default_rx_callback()`) ihn Byte für Byte über
`uhf_buffer_append_single()`, bis entweder das Terminator-Byte `FRAME_END` (`0x7E`) auftaucht
oder ein Timeout greift. Anschließend liest `setup_and_send_rx()` (`uhf_module.c:19-21`) den
Puffer so aus, als wäre er ein einfacher linearer Byte-Block ab Index 0:

```c
uint8_t* data = uhf_buffer_get_data(buffer);
size_t length = uhf_buffer_get_size(buffer);
if(!length) return M100EmptyResponse;
if(data[0] != FRAME_START || data[length - 1] != FRAME_END) return M100ValidationFail;
if(checksum(data + 1, length - 3) != data[length - 2]) return M100ChecksumFail;
```

`uhf_buffer.c` implementiert `Buffer` aber tatsächlich als **echten Ringpuffer**:
`uhf_buffer_append_single()`/`_append()` schreiben immer an `tail`, und sobald `size == capacity`
erreicht ist, wird bei jedem weiteren Byte zusätzlich `head` um eins weitergeschoben (ältestes
Byte wird überschrieben, klassisches Ringpuffer-Verhalten):

```c
bool uhf_buffer_append_single(Buffer* buf, uint8_t data) {
    if(buf->closed) return false;
    buf->data[buf->tail] = data;
    buf->tail = (buf->tail + 1) % buf->capacity;
    if(buf->size < buf->capacity) {
        buf->size++;
    } else {
        buf->head = (buf->head + 1) % buf->capacity;   // <-- head wird > 0
    }
    return true;
}

uint8_t* uhf_buffer_get_data(Buffer* buf) {
    return &buf->data[buf->head];   // Zeiger irgendwo INNERHALB des 250-Byte-Blocks
}
```

Sobald eine einzelne Modul-Antwort die 250 Byte übersteigt (ohne dass vorher `FRAME_END`
gesehen wurde — z. B. eine kombinierte TID+USER-Bank-Lesung, eine Multi-Tag-Inventur-Antwort
mit mehreren aneinandergereihten Tag-Datensätzen, oder einfach eine gestörte Verbindung, bei
der das echte Terminator-Byte ausbleibt), wird `head > 0`. `uhf_buffer_get_data()` gibt dann
einen Zeiger zurück, der **innerhalb** der 250-Byte-Allokation liegt, während der Aufrufer ihn
weiterhin so behandelt, als stünden ab dort `length` (bis zu `capacity` = 250) zusammenhängende,
gültige Bytes — `data[length - 1]` zeigt dadurch bis zu `head` Bytes **hinter das Ende der
Allokation**, ebenso der komplette `checksum(data + 1, length - 3)`-Bereich. Kein Codepfad in
`uhf_module.c` behandelt den Puffer je als Ringpuffer (kein zweites Segment, kein Modulo beim
Lesen) — die ganze restliche App geht implizit davon aus, dass `get_data()` bei Index 0 der
aktuellen Antwort beginnt, was nur stimmt, solange nie ein Überlauf stattfand. Betroffen sind
alle sechs `uhf_buffer_get_data()`-Aufrufstellen in `uhf_module.c` (Geräte-/Modul-Info,
Tag-Lesen, Tag-Schreiben, Löschen — jede Antwortauswertung der App).

Neben dem Speicherzugriffsfehler ist die praktische Folge für mich als Nutzer eine stille
Dateninkonsistenz: selbst dort, wo der Lesezugriff zufällig noch innerhalb gültigen
Heap-Speichers bleibt, sind die "gelesenen" Bytes nach einem Überlauf nicht mehr die tatsächlich
empfangenen (das älteste Antwortsegment wurde überschrieben, `head` zeigt auf einen späteren,
durch den Ringpuffer-Umlauf verschobenen Startpunkt) — eine als Tag-ID/EPC/TID/User-Daten
angezeigte oder gespeicherte Antwort wäre nach einem Überlauf falsch, ohne dass die App das als
Fehler erkennt (die `FRAME_START`/`FRAME_END`/Checksum-Prüfung direkt danach kann den Defekt
zwar oft, aber nicht garantiert abfangen, je nachdem wie die verschobenen Bytes zufällig
aussehen).

## Host-Repro
Eigenständiges Host-Repro, `uhf_buffer.c`/`.h` unverändert (byteidentisch zum Original-Repo-Stand)
verwendet, Zugriffsmuster von `setup_and_send_rx()` 1:1 nachgebaut (`data[0]`, `data[length-1]`,
`data[length-2]`, `checksum(data+1, length-3)`), Pufferkapazität exakt wie
`UHF_UART_RX_BUFFER_SIZE = 250`:

```
gcc -O1 -Wall -Wextra -fsanitize=address,undefined repro_harness.c uhf_buffer_old.c -o repro_old
./repro_old small   # 40-Byte-Antwort, passt ohne Überlauf
./repro_old exact    # 250-Byte-Antwort, exakt an der Kapazitätsgrenze
./repro_old large    # 300-Byte-Antwort (FRAME_END erst als letztes Byte), ueberschreitet die Kapazitaet
```

Tatsächliche Ausgabe (lokal ausgeführt):
```
--- ALT small (40 B) ---
[normal 40B response] resp_len=40 capacity=250 size=40 head=0 tail=40 closed=1
        data[0]=0xBB data[len-1]=0x7E data[len-2]=0x89 checksum=0x32
small-response frame recognized: yes (expected: yes)

--- ALT exact (250 B) ---
[exact 250B response] resp_len=250 capacity=250 size=250 head=0 tail=0 closed=1
        data[0]=0xBB data[len-1]=0x7E data[len-2]=0xE3 checksum=0x52
exact-capacity response frame recognized: yes (expected: yes)

--- ALT large (300 B) ---
==ERROR: AddressSanitizer: heap-buffer-overflow ... READ of size 1
0x51100000016b is located 49 bytes after 250-byte region [...]
    #1 uhf_buffer_alloc uhf_buffer_old.c:7
SUMMARY: AddressSanitizer: heap-buffer-overflow repro_harness.c:77 in run_case
==ABORTING
```

Fall 1 (40 B) und Fall 2 (exakt 250 B, Kapazitätsgrenze ohne Umlauf) laufen sauber durch — das ist
der weit überwiegende reale Fall (die meisten Modul-Antworten wie reine EPC-Bank-Lesungen bleiben
deutlich unter 250 Byte). Fall 3 (300 B, entspricht z. B. einer größeren TID+USER-Bank-Antwort
oder einer Multi-Tag-Inventur) löst den Ringpuffer-Umlauf aus und liefert einen
AddressSanitizer-Abbruch mit `heap-buffer-overflow`, genau an der Stelle, die `data[length-1]`
in `setup_and_send_rx()` entspricht — **49 Byte hinter dem Ende der 250-Byte-Allokation**.

Nach dem Fix (`uhf_buffer_new.c`, siehe unten) derselbe Lauf ohne ASan-Befund:
```
--- NEU large (300 B) ---
[oversized 300B response] resp_len=300 capacity=250 size=250 head=0 tail=0 closed=1
        data[0]=0xBB data[len-1]=0x08 data[len-2]=0xE3 checksum=0x52
oversized-response frame recognized as valid: no (expected: no, once truncated safely)
```
`head` bleibt `0`, der Puffer stoppt kontrolliert bei 250 Byte, und da das echte `FRAME_END`-Byte
der 300-Byte-Antwort dadurch nie ankommt, erkennt die bereits vorhandene
`data[length-1] != FRAME_END`-Prüfung die abgeschnittene Antwort korrekt als ungültig
(`M100ValidationFail`) — ein kontrollierter, für den Nutzer sichtbarer Fehlerfall statt eines
Speicherzugriffsfehlers oder stillschweigend falscher Tag-Daten. Fall 1 und 2 liefern unter NEU
byteidentische Ausgaben wie unter ALT (Regressionsfälle bestätigt keine Verhaltensänderung im
Normalfall).

## Betroffene Dateien
- `uhf_buffer.c` — `uhf_buffer_append_single()` und `uhf_buffer_append()`, einzige Fix-Stelle.
- `uhf_module.c` — keine Änderung nötig, alle sechs `uhf_buffer_get_data()`-Aufrufstellen
  profitieren automatisch vom Fix, da sie weiterhin denselben (jetzt tatsächlich korrekten)
  "linearer Puffer ab Index 0"-Vertrag nutzen.

## Risiko
Gering. Zwei zusätzliche Prüfzeilen (`if(buf->size >= buf->capacity) return false;`) direkt am
Anfang der beiden Schreibfunktionen, keine Signaturänderung, keine neue Abhängigkeit. Das
bisherige Ringpuffer-Überschreib-Verhalten (`head` weiterschieben) wird komplett entfernt, weil
es vom einzigen tatsächlichen Aufrufer (`uhf_module.c`, immer "einmal befüllen bis Reset",
niemals ein gleitendes Fenster über mehrere Antworten hinweg) nirgends benötigt wird — die
Regressionsfälle (40 B, exakt 250 B) belegen, dass sich für die reale Nutzung nichts ändert.

## Port-Skizze / Fix
In `uhf_buffer.c`, an den Anfang von `uhf_buffer_append_single()` und der Schleife in
`uhf_buffer_append()` je eine Kapazitätsprüfung, die weitere Bytes verwirft statt das älteste
Byte zu überschreiben (Konsequenz: `head` bleibt für den einzigen real genutzten
"Sammle-bis-Reset"-Anwendungsfall dauerhaft `0`, `get_data()` ist damit immer ein gültiger,
`size` Bytes langer, zusammenhängender Block):

```c
bool uhf_buffer_append_single(Buffer* buf, uint8_t data) {
    if(buf->closed) return false;
    if(buf->size >= buf->capacity) return false;
    buf->data[buf->tail] = data;
    buf->tail = (buf->tail + 1) % buf->capacity;
    buf->size++;
    return true;
}

bool uhf_buffer_append(Buffer* buf, uint8_t* data, size_t data_size) {
    if(buf->closed) return false;
    for(size_t i = 0; i < data_size; i++) {
        if(buf->size >= buf->capacity) return false;
        buf->data[buf->tail] = data[i];
        buf->tail = (buf->tail + 1) % buf->capacity;
        buf->size++;
    }
    return true;
}
```
`uhf_buffer_get_data()`, `uhf_buffer_get_size()`, `uhf_buffer_reset()` u. a. bleiben unverändert.

## Testhinweis (lokal nachvollzogen)
`firmware-mining/repro-uhf-rfid-ring-buffer-oob-read/` — `uhf_buffer_old.c`/`uhf_buffer_new.c`
(unverändert bzw. mit Fix), `uhf_buffer.h` (unverändert), `repro_harness.c`. Host-kompilierbar,
keine Flipper-/UART-/Modul-Abhängigkeiten (Befehl und tatsächliche Ausgabe siehe Host-Repro
oben).

Für den lokalen Funktionstest an echter Hardware (YRM100-/M100-Modul am Erweiterungsport nötig):
eine Lesung anstoßen, die eine Antwort über 250 Byte erzeugt (TID- **und** USER-Bank desselben
Tags in einem Lauf lesen, bei einem Tag mit größerem User-Speicher; oder mehrere Tags gleichzeitig
im Lesefeld für eine Multi-Tag-Inventur-Antwort). Vor dem Fix: je nach zufälliger
Pufferverschiebung entweder ein App-Absturz (harter Fehler statt einer sauberen Fehlermeldung)
oder eine als erfolgreich gemeldete Lesung mit falschen/vertauschten Tag-Daten. Nach dem Fix:
entweder eine kontrollierte "Validation Fail"/"Empty Response"-Meldung bei Antworten über 250 Byte,
oder — falls die tatsächliche Antwortgröße das schon vorher zuließ — unverändert eine korrekte
Lesung. Regressionsfall (normale Einzelbank-Lesung, deutlich unter 250 Byte) muss vor und nach
dem Fix identisch funktionieren.

## Zusätzlich geprüft, kein Fund
- `uhf_uart.c`: `uhf_uart_default_rx_callback()` schließt den Puffer korrekt genau einmal (der
  scheinbare doppelte `uhf_buffer_append_single()`-Aufruf beim `FRAME_END`-Byte ist harmlos, da
  `uhf_buffer_close()` das `closed`-Flag vor dem zweiten Aufruf setzt und dieser dadurch sofort
  `false` zurückgibt); `expansion_disable()`/`_enable()`-Reihenfolge und 5-V-OTG-Wächter in
  `uhf_app.c` bereits oben als fundfrei vermerkt.
- `uhf_tag.c`/`uhf_worker.c`: reine Tag-Metadaten-Verwaltung bzw. Worker-Thread-Lifecycle
  (`furi_thread_flags_wait`/`_set` mit klar getrennten Start-/Stop-Flags), kein eigener
  Pufferzugriff über den bereits gefundenen Fund hinaus.
- `uhf_module.c` selbst über den beschriebenen Fund hinaus: `checksum()`/`crc16_genibus()` sind
  reine Schleifen über die als Parameter übergebene Länge, kein eigener Bug; die
  `FRAME_START`/`FRAME_END`/Checksum-Validierungsreihenfolge in `setup_and_send_rx()` selbst ist
  korrekt (sobald sie auf einem tatsächlich linearen Puffer arbeitet, siehe Fix).
- `MAX_BUFFER_SIZE 200` (`uhf_buffer.h:6`) ist eine ungenutzte, irreführende Konstante (die
  tatsächliche Kapazität kommt ausschließlich aus `UHF_UART_RX_BUFFER_SIZE = 250`,
  `uhf_uart.h:8`, das beim `uhf_buffer_alloc()`-Aufruf übergeben wird) — keine funktionale
  Auswirkung, da nirgends referenziert, hier nur der Vollständigkeit halber vermerkt.

## Quelle
- `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`, Sparse-Checkout,
  Commit `f1ee8b2405f8ca6c133a306f2911ac5347c4c4f5`): `applications/external/uhf_rfid/`,
  `fap_author` "frux-c", Original-Quelle `github.com/frux-c/uhf_rfid/tree/dev`, `fap_version` 1.0.
- API-87-Einordnung: betroffener Code (`uhf_buffer.c`) ist reine, firmwareunabhängige
  Ringpuffer-Arithmetik ohne jeden Flipper-API-Bezug — kein firmwareversionsabhängiges
  Kompatibilitätsrisiko. Die UART-/Expansion-Aufrufstellen selbst (`furi_hal_serial_control_*`,
  `furi_hal_serial_*`, `expansion_enable/disable`) folgen exakt demselben, bereits mehrfach gegen
  `mntm-012` verifizierten Muster wie `uart_sniff`/`rogue_ap_detector`/`ble_scanner`
  (siehe `ideas/uart-sniff-api-status.md`, `ideas/rogue-ap-detector-h10301-writer-api-status.md`).
- Eigener Fund: `uhf_buffer.c`, `uhf_buffer_append_single()`/`uhf_buffer_append()`, mit Host-Repro
  bestätigt (siehe `firmware-mining/repro-uhf-rfid-ring-buffer-oob-read/`).
- Restlicher NFC-/RFID-Werkzeug-Cluster (`chameleon`, `nfc_relay` — beide zwingend Fremd-/
  Zweitgerät; NFC-Tools-Schreib-Szenen/Passwort-Entfernung) weiterhin für künftige Sitzungen
  offen. `uhf_rfid`-Szenen-Code (`scenes/*.c`, reine UI-Ablaufsteuerung über den bereits geprüften
  Kern) nicht mitgeprüft.
