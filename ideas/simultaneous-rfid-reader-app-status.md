# Simultaneous UHF RFID Reader (haffnerriley) — API-87-Status

Aus dem noch offenen Rückstellpunkt "restlicher NFC-/RFID-Werkzeug-Cluster" aufgegriffen:
**Simultaneous RFID Reader** war der letzte dort noch unbenannte Kandidat. Bei der Recherche
stellte sich heraus, dass es sich um kein RogueMaster-Bündel-Programm handelt, sondern um ein
eigenständiges Projekt, das selbst wiederum ausdrücklich auf dem bereits vollständig geprüften
`uhf_rfid` (frux-c, siehe `firmware-mining/uhf-rfid-ring-buffer-oob-read-fix.md` und die drei
`uhfrfid-*-fix.md`-Funde) aufbaut.

## Quelle
- `haffnerriley/Simultaneous-UHF-RFID-FlipperZero`, `main`-Branch, Commit
  `1dfefed1c67078da8c416b7b93c02eaa25e220fa` (07.05.2025), Lizenz laut Repo-`LICENSE`, App-Ordner
  `simultaneous_rfid_reader/` (`appid` `simultaneous_rfid_reader`, `fap_version` 1.1.2,
  `fap_category` "GPIO", `fap_author` "@RileyHaffner"). Eigenständig im Flipper-App-Katalog
  gelistet. 8513 Zeilen über Kern (`app.c`, elf `helpers/*.c`) und zwölf `views/*.c`-UI-Dateien.
  `application.fam` benennt `fap_description` ausdrücklich als Fortsetzung für M6E-Nano-/M7E-
  Hecto-/YRM100-Leser; das README bestätigt unter "Acknowledgements" wörtlich: "Most of the
  functionality for reading, writing, and interacting with the YRM100 module was modified from
  Frux-C's UHF-RFID App for the FlipperZero" — also direkt verwandt mit dem bereits dokumentierten
  `uhf_rfid`-Fund, nicht nur thematisch benachbart.

## Was
UHF-RFID-Lese-/Schreib-/Verwaltungs-App (860–960 MHz, EPC Gen2) mit zwei Hardware-Wegen:
- **YRM100** direkt über UART am Erweiterungsport (für alle Features: Lesen/Schreiben/Sperren/
  Kill aller vier Speicherbänke EPC/TID/Reserved/User, Konfiguration von RF-Leistung/Region/
  Baudrate/Zugangspasswort).
- **M6e Nano / M7E Hecto** indirekt über einen externen Raspberry Pi Zero, der über die
  ThingMagic-Mercury-API mit dem Modul spricht und die Ergebnisse als zeilenbasiertes
  Textprotokoll über eine zweite (Soft-)UART-Verbindung an den Flipper weiterreicht
  (`pi-zero-uhf.py` im Repo beigelegt) — bis zu 150 Tags/Sekunde laut Hersteller, Lesen/Speichern/
  Umbenennen/Löschen gespeicherter Tags auf der Flipper-SD-Karte.

## Nutzen für mich
Zweiter UHF-RFID-Anbindungsweg neben dem bereits dokumentierten `uhf_rfid`: gleiche
YRM100-Zielhardware, aber mit zusätzlichem Funktionsumfang (Lock/Kill-Operationen, Konfigurations-
menü, Multi-Tag-Übersicht) und einem alternativen M6E/M7E-Pfad über eine zusätzliche
Raspberry-Pi-Zero-Bridge für Leser, die kein direktes UART-Protokoll zum Flipper sprechen — relevant
für autorisierte physische Zugangskontroll-/Inventar-Audits mit UHF-Badges/-Etiketten, bei denen
der reine `uhf_rfid`-Funktionsumfang (nur YRM100, keine Lock/Kill-Operationen laut dessen eigener
Doku) nicht ausreicht.

## API-87-Status
✅ läuft unverändert. Der YRM100-UART-Pfad (`helpers/yrm100x_uart.c`, `helpers/uart_helper.c`)
nutzt exakt dieselbe `furi_hal_serial_control_acquire()`/`_init()`/`_set_br()`/
`_async_rx_start()`/`_tx()`/`_tx_wait_complete()`/`_control_release()`/`_deinit()`-Aufkette über
`FuriHalSerialIdUsart` sowie dieselbe `expansion_enable()`/`_disable()`-Klammerung und
`furi_hal_power_is_otg_enabled()`/`_enable_otg()`/`_disable_otg()`-Wächterlogik, die bereits
mehrfach wörtlich gegen `G4MEOVER18/G4MEOVER-FW` verifiziert wurde (`uhf_rfid`, `uart_sniff`,
`rogue_ap_detector` — siehe `ideas/uart-sniff-api-status.md`,
`ideas/rogue-ap-detector-h10301-writer-api-status.md`, `firmware-mining/
uhf-rfid-ring-buffer-oob-read-fix.md`) — keine erneute Einzelprüfung nötig, kein Firmware-Patch
erforderlich. Der M6E/M7E-Pfad über die zweite Soft-UART läuft über denselben, bereits
verifizierten `furi_hal_serial_*`-Satz (`helpers/uart_helper.c` wird für beide Leserfamilien
gemeinsam genutzt). Kein Erweiterungsport-Konflikt mit einem eigenen Standardmodul (reine
UART-Nutzung wie bei `uhf_rfid`).

## Quelltext-Review und Fund

Cluster-Umfang dieser Sitzung bewusst auf das projektspezifisch **neue** Teilstück begrenzt: den
von `uhf_rfid` übernommenen YRM100-Kern (`yrm100x_module.c`/`_tag.c`/`_worker.c`/`_buffer.c`) habe
ich nicht erneut Zeile für Zeile geprüft (der dort bereits dokumentierte Ringpuffer-Fund aus
`uhf_rfid` betrifft eine andere, hier nicht vorhandene Datei — dieser Fork verwendet für den
YRM100-Antwortpuffer `helpers/yrm100x_buffer.c`, nicht `uhf_buffer.c`; eine erneute Volldurchsicht
dieses übernommenen Teils bleibt für eine künftige Sitzung offen). Stattdessen auf das Stück
geprüft, das es im Original-`uhf_rfid` gar nicht gibt: die M6E/M7E-Pi-Zero-Brücke
(`helpers/ring_buffer.c`/`helpers/uart_helper.c`), die laut README-Danksagung direkt von Derek
Jamisons `flipper-zero-tutorials`-UART-Demo übernommen wurde.

### Fund: `ring_buffer_extract_line()` — Off-by-one verwechselt eine Leerzeile mit einem Ringpuffer-Umlauf

`helpers/ring_buffer.c` implementiert einen klassischen Byte-Ringpuffer (`ring_buffer_size =
4096`) für das zeilenbasierte Pi-Zero-Textprotokoll (Trennzeichen `'\n'`, siehe `app.h:6`
`#define LINE_DELIMITER '\n'`, `app.h:8` `#define INCLUDE_LINE_DELIMITER false`, gesetzt in
`app.c:126`/`view_config.c:519,535`). Nach dem Füllen (`ring_buffer_add()`) und dem Auffinden
eines Trennzeichens (`ring_buffer_find_delim()`, liefert den absoluten Index im physischen
4096-Byte-Array) entscheidet `ring_buffer_extract_line()` anhand eines Vergleichs, ob die Zeile
"in einem Stück" liegt oder über die physische Pufferkante gewickelt ("wrapped") ist:

```c
void ring_buffer_extract_line(RingBuffer* rb, size_t delim_index, FuriString* line) {
    if(delim_index > rb->ring_buffer_read) {
        // line is in one chunk
        furi_string_set_strn(
            line,
            (char*)&rb->ring_buffer[rb->ring_buffer_read],
            delim_index - rb->ring_buffer_read + (rb->include_delimiter ? 1 : 0));
    } else {
        // line is split across the buffer wrap, so we need to copy it in two chunks
        furi_string_set_strn(
            line, (char*)&rb->ring_buffer[rb->ring_buffer_read],
            ring_buffer_size - rb->ring_buffer_read);
        for(size_t i = 0; i < delim_index; i++) {
            furi_string_push_back(line, (char)rb->ring_buffer[i]);
        }
        ...
    }
    rb->ring_buffer_read = delim_index + 1;
    ...
}
```

Die Bedingung prüft `delim_index > rb->ring_buffer_read` — mit **echtem** `>` statt `>=`. Ein
tatsächlicher Ringpuffer-Umlauf liegt aber genau dann vor, wenn `delim_index < ring_buffer_read`
ist (das Trennzeichen liegt physisch *vor* dem aktuellen Lesezeiger, weil `write` zwischenzeitlich
über das Pufferende gelaufen ist). Der Fall `delim_index == ring_buffer_read` — das Trennzeichen
ist exakt das nächste ungelesene Byte, d. h. eine **leere Zeile** direkt am aktuellen Lesezeiger
(zwei aufeinanderfolgende `\n`, oder die allererste empfangene Zeile ist bereits leer) — ist
**kein** Umlauf, wird aber durch das strikte `>` trotzdem in den `else`-Zweig geschickt.

Die Folge: `furi_string_set_strn(line, &ring_buffer[read], ring_buffer_size - read)` kopiert bis zu
mehrere tausend Byte aus dem 4096-Byte-Puffer — weit über die tatsächlich jemals empfangenen
Bytes hinaus, in den Bereich, der nach `ring_buffer_alloc()` (einfaches `malloc()`, keine
Nullinitialisierung) nie beschrieben wurde. Statt der korrekten leeren Zeile liefert die Funktion
eine um Größenordnungen zu lange "Zeile" aus größtenteils unbestimmtem Heap-Inhalt, die
anschließend unverändert an `helper->process_line`-Callback weitergereicht wird — in dieser App
an die Pi-Zero-Protokoll-Parser (`parse_epc_string()`/`parse_mem_string()` etc. im Begleit-
Python-Skript senden feste Zeilenformate; auf Flipper-Seite wertet der jeweilige UART-Prozess-Code
die empfangene `FuriString*` direkt aus). Erreichbarkeit: ausschließlich im M6E-/M7E-Pfad über die
Pi-Zero-Brücke (der direkte YRM100-Pfad nutzt einen anderen Parser,
`helpers/yrm100x_buffer.c`, nicht betroffen); ausgelöst durch zwei unmittelbar aufeinanderfolgende
`\n`-Bytes im seriellen Strom vom Pi-Zero-Skript — bei einem zeilenbasierten `print()`-Protokoll
(Python-Standard-`end='\n'`) ein realistischer, wiederkehrender Fall (eine leere Statuszeile, ein
doppeltes Zeilenende durch eine Formatierungslücke im Begleitskript, oder einfach die erste Zeile
nach einem frischen Verbindungsaufbau), kein konstruierter Extremfall.

### Fix (additiv, ein Zeichen)
```c
if(delim_index >= rb->ring_buffer_read) {
```
`>` → `>=`. Keine Strukturänderung, keine weitere Anpassung nötig: der jetzt korrekt nur noch für
echte Umläufe (`delim_index < read`) betretene `else`-Zweig bleibt unverändert richtig (siehe
Host-Repro, Fall 4). Patch
(`simultaneous-rfid-reader-ring-buffer-empty-line-fix/ring_buffer_c.patch`) gegen drei frische,
unabhängige Zweitkopien des Original-Repos mit `git apply --check` verifiziert, real angewendet,
Ergebnis per `md5sum` byte-für-byte abgeglichen — identisch (`08a7482f3fcb2da649fd0ab8f2ebb154`).

### Host-Repro
`repro-simultaneous-rfid-reader-ring-buffer-empty-line/`: `ring_buffer_old.c`/`ring_buffer_new.c`
(unverändert bzw. mit dem Ein-Zeichen-Fix) byteidentisch aus `helpers/ring_buffer.c` übernommen,
nur `#include <furi.h>` durch einen minimalen `FuriString`-Host-Mock (`furi_mock.c`/`.h`, reines
`malloc`/`memcpy`-Äquivalent der real genutzten `furi_string_alloc/_free/_set_strn/_push_back`)
ersetzt. Der Puffer wird vor jedem Testfall mit einem Wächter-Byte `0xAA` vorbelegt (simuliert
frisch allokierten, nie beschriebenen Heap-Speicher, da `malloc()` keine Nullgarantie gibt).
Vier Fälle, `gcc -std=c11 -Wall -Wextra -O0`/`-O1 -fsanitize=address,undefined
-fno-sanitize-recover=undefined`:
- Fall 1 (Regression, normale Zeile `"AB\n"`, kein Wrap): ALT und NEU identisch korrekt.
- Fall 2 (der Fund, `"\n\n"`, Leerzeile ganz am Pufferanfang, `delim_index == read == 0`, **kein**
  echter Wrap): **ALT liefert Länge 4096 statt 0**, erste 32 ausgelesene Bytes zeigen die
  Wächter-Bytes `0xAA` statt der tatsächlichen (leeren) Zeile; **NEU liefert korrekt Länge 0**.
- Fall 3 (realistischster Trigger, `"OK\n\nCD\n"`, drei Zeilen in Folge inkl. einer echten
  Leerzeile zwischen zwei Nutzzeilen): ALT liefert für Zeile 2 ebenfalls Länge 4096 statt 0
  (Zeile 1 und 3 bleiben davon unberührt korrekt); NEU liefert alle drei Zeilen korrekt
  (`"OK"`, Länge 0, `"CD"`).
- Fall 4 (Regression, echter Ringpuffer-Umlauf: Zeile `"XY"` liegt physisch über die 4096-Byte-
  Kante verteilt, `delim_index(0) < read(4094)`): ALT und NEU liefern identisch korrekt `"XY"` —
  der eigentliche Zwei-Chunk-Kopierpfad selbst bleibt unverändert richtig, nur die Bedingung,
  wann er betreten wird, wurde korrigiert.

Tatsächliches Ergebnis (lokal ausgeführt): ALT **2 von 10 Prüfungen fehlgeschlagen** (Fall 2 und
3) bei `-O0` und `-O1` identisch, keine Compiler-Warnungen, kein ASan-/UBSan-Fund (der Zugriff
bleibt innerhalb der 4096-Byte-Allokation — reiner Logikfehler, kein Speicherzugriffsfehler). NEU:
**ALL CHECKS PASSED (10 total)** bei beiden Optimierungsstufen, identischer Output.

## Betroffene Dateien
- `simultaneous_rfid_reader/helpers/ring_buffer.c` — `ring_buffer_extract_line()`, einzige
  Fix-Stelle (eine Zeichenänderung).
- `simultaneous_rfid_reader/helpers/uart_helper.c` — keine Änderung nötig, nutzt den Puffer nur
  über die bereits korrekte Aufrufkette (`ring_buffer_find_delim()` → `ring_buffer_extract_line()`
  → `process_line`-Callback).

## Testhinweis (lokal, YRM100- oder Pi-Zero-Hardware nötig für den Funktionstest)
Patch gegen die eigene `Simultaneous-UHF-RFID-FlipperZero`-Kopie anwenden, mit `ufbt`/`fbt`
bauen und auf den Flipper flashen (reine Ein-Zeichen-Bedingungsänderung, kein neues Symbol, sollte
ohne Compile-Fehler durchlaufen). Funktionstest (M6E-/M7E-Pfad über die Pi-Zero-Brücke): im
Begleitskript `pi-zero-uhf.py` gezielt eine leere Zeile senden (z. B. testweise ein zusätzliches
`print()` ohne Argumente vor einer regulären Antwortzeile einfügen) und beobachten, ob die
App die folgende reguläre Zeile weiterhin korrekt parst (vor dem Fix: durch die überlange
"Leerzeile" verschiebt sich die gesamte nachfolgende Zeilenzuordnung, die eigentliche Antwort wird
nicht mehr korrekt erkannt; nach dem Fix: die Leerzeile wird folgenlos übersprungen, die nächste
reguläre Zeile kommt unverändert an). Regressionsfall: normale Mehrfach-Tag-Lesesequenz ohne
Leerzeilen muss vor und nach dem Fix identisch funktionieren (siehe Host-Repro Fall 1/4 für den
reinen Datenstrukturnachweis ohne Hardware).

## Sicherheitsrahmen
Rein defensiver Fund an einem eigenen Werkzeug für eigene/autorisierte UHF-RFID-Hardware — kein
Fremdangriff, keine Umgehung einer Schutzfunktion, passt zum eigenen defensiven/autorisierten
Rahmen.

## Risiko
Gering: additive Einzelzeichen-Änderung an einer bestehenden Vergleichsbedingung, keine
Signatur-/Strukturänderung. Der unveränderte Zwei-Chunk-Kopierpfad selbst wird nicht berührt
(Host-Repro Fall 4 bestätigt das für echte Umlauf-Fälle explizit).

## Nächster Schritt (ursprünglich, inzwischen teilweise aufgegriffen)
Lokal bauen/flashen und wie oben beschrieben mit der Pi-Zero-Brücke verifizieren; danach ggf.
Upstream-PR gegen `haffnerriley/Simultaneous-UHF-RFID-FlipperZero` erwägen (eigenständiger Fund im
von Derek Jamisons Tutorial übernommenen Teil, nicht an eigene Firmware gebunden). Weiterhin
offen: vollständige Erstprüfung des aus `uhf_rfid` übernommenen YRM100-Kerns dieses Forks
(`yrm100x_module.c`/`_tag.c`/`_worker.c`/`_buffer.c`) sowie der zwölf UI-`views/*.c`-Dateien, in
einer künftigen Sitzung.

## Update: YRM100-Kern erstgeprüft — zweiter Fund (TID-/User-Bank-Overflow)

Den oben als offen vermerkten YRM100-Kern dieses Forks (`yrm100x_module.c` 612 Zeilen,
`yrm100x_tag.c` 151 Zeilen, `yrm100x_worker.c` 238 Zeilen, `yrm100x_buffer.c` 75 Zeilen, alle vier
erstmals vollständig gelesen, gegen eigenen frischen Klon Commit `1dfefed1c67078da8c416b7b93c02eaa25e220fa`,
identisch zum oben dokumentierten Stand) aufgegriffen.

Eine erste Auffälligkeit (`Buffer.closed` in `uhf_buffer_alloc()` nie initialisiert, nur per
`malloc()` reserviert) erwies sich beim Gegenlesen als folgenlos: jede einzige YRM100-Kommando-
funktion läuft über `setup_and_send_rx()`, und die ruft als allerersten Schritt
`uhf_buffer_reset(buffer)` auf — das setzt `closed` vor jeder tatsächlichen Nutzung zuverlässig
auf `false`. Kein eigener Fund.

Eine zweite Auffälligkeit (`_m100_info_helper()` alloziert `*info` exakt `payload_len` Byte ohne
Platz für eine Terminierung und würde bei einem zweiten Aufruf mit größerer Antwort über die
ursprüngliche Allokation hinausschreiben) betrifft ausschließlich
`m100_get_hardware_version()`/`_software_version()`/`_manufacturers()` — erreichbar nur über
`verify_module_connected()` im `UHFWorkerStateVerify`-Worker-Zustand. Dieser Zustand wird in der
gesamten App nirgends an `uhf_worker_start()` übergeben (`grep` über alle zwölf `views/*.c` sowie
`app.c`: kein einziger Treffer für `UHFWorkerStateVerify`) — toter Code in diesem Fork, kein
eigener Port-Fund, da nicht erreichbar.

### Fund: `uhf_tag_set_tid()`/`uhf_tag_set_user()` ohne Längenprüfung gegen die feste 200-Byte-Bank

`yrm100x_tag.h` legt `TIDMemoryBank`/`UserMemoryBank` mit je einem festen `uint8_t data[MAX_BANK_SIZE]`
an (`MAX_BANK_SIZE` = 200). Die Setter in `yrm100x_tag.c`:

```c
void uhf_tag_set_tid(UHFTag* uhf_tag, uint8_t* data_in, size_t size) {
    memcpy(uhf_tag->tid->data, data_in, size);
    uhf_tag->tid->size = size;
}

void uhf_tag_set_user(UHFTag* uhf_tag, uint8_t* data_in, size_t size) {
    memcpy(uhf_tag->user->data, data_in, size);
    uhf_tag->user->size = size;
}
```

kopieren `size` Byte ungeprüft in diesen 200-Byte-Puffer — anders als `uhf_tag_set_kill_pwd()`/
`_set_access_pwd()` im selben File, die ihre feste 4-Byte-Zielgröße vorher explizit gegen `size`
prüfen. Aufrufer ist `m100_read_label_data_storage()` in `yrm100x_module.c`:

```c
size_t ptr_offset = 5 /*<-ptr offset*/ + uhf_tag_get_epc_size(uhf_tag) + 3 /*<-pc + ul*/;
size_t bank_data_length = payload_len - (ptr_offset - 5 /*dont include the offset*/);

if(bank == TIDBank) {
    uhf_tag_set_tid(uhf_tag, data + ptr_offset, bank_data_length);
} else if(bank == UserBank) {
    uhf_tag_set_user(uhf_tag, data + ptr_offset, bank_data_length);
}
```

`payload_len` ist das 16-Bit-Längenfeld (`data[3]`/`data[4]`) der vom Reader-Modul über UART
zurückgesendeten Antwort — die einzige vorherige Prüfung in `setup_and_send_rx()` ist
Rahmen-Start/-Ende und eine Prüfsumme über den gesamten Frame, keine inhaltliche Prüfung, ob
`payload_len` zu dem tatsächlich angeforderten `word_count` passt. Der eigene Aufrufer
`read_bank_till_max_length()` (`yrm100x_worker.c`) fragt zwar nie mehr als 64 Worte (128 Byte) in
einer einzelnen binären Suche an, erzwingt aber nicht, dass die Antwort diese Grenze auch
einhält — eine non-konforme oder manipulierte Antwort (fehlerhaftes/kompromittiertes Lesegerät,
UART-Störung mit zufällig passender Prüfsumme, oder ein Tag, der dem Lesegerät eine größere
Wortzahl als angefragt meldet und vom Lesegerät unverändert durchgereicht wird) kann
`bank_data_length` über 200 hinaus treiben. Weder `m100_read_label_data_storage()` noch die
beiden Setter selbst klemmen diesen Wert — ein Heap-Buffer-Overflow-Write über das Ende von
`tid->data[200]` bzw. `user->data[200]` hinaus, direkt im Pfad, der laut README-Zweck der App
(Tag-Speicherbänke lesen) bei jedem TID-/User-Bank-Lesevorgang durchlaufen wird. `uhf_tag_set_epc()`
ist von derselben fehlenden Prüfung zwar ebenfalls nicht befreit, aber durch die Herkunft ihrer
Länge (`epc_len` aus einem einzelnen Protokoll-Kontroll-Byte, maximal 31 × 2 = 62 Byte) strukturell
unter 200 begrenzt — kein eigener Fund dort, nicht geändert.

### Fix (additiv, zwei Zeilen)
```c
void uhf_tag_set_tid(UHFTag* uhf_tag, uint8_t* data_in, size_t size) {
    if(size > MAX_BANK_SIZE) size = MAX_BANK_SIZE;
    memcpy(uhf_tag->tid->data, data_in, size);
    uhf_tag->tid->size = size;
}

void uhf_tag_set_user(UHFTag* uhf_tag, uint8_t* data_in, size_t size) {
    if(size > MAX_BANK_SIZE) size = MAX_BANK_SIZE;
    memcpy(uhf_tag->user->data, data_in, size);
    uhf_tag->user->size = size;
}
```
Klemmt an der Stelle, die die feste Zielgröße tatsächlich kennt (`MAX_BANK_SIZE`, bereits im selben
Header definiert) — kein Eingriff in `m100_read_label_data_storage()` oder die
Antwortlängen-Arithmetik nötig. Patch
(`ideas/simultaneous-rfid-reader-yrm100-tid-user-bank-overflow-fix/yrm100x_tag_c.patch`) gegen eine
frische, unabhängige Zweitkopie mit `patch --dry-run` verifiziert, real angewendet, Ergebnis per
`md5sum` gegen eine dritte, unabhängige Zweitkopie byte-für-byte abgeglichen — identisch.

### Host-Repro
`ideas/repro-simultaneous-rfid-reader-yrm100-tid-user-bank-overflow/repro.c`: `UHFTag`/
`TIDMemoryBank`/`UserMemoryBank`/`EPCMemoryBank`-Structs sowie `uhf_tag_alloc()`/`_free()`/
`_set_epc()`/`_set_tid()`/`_set_user()` byte-für-byte aus `yrm100x_tag.h`/`.c` übernommen (ALT ohne
Klemme, NEU mit der Fix-Zeile über ein Compile-Flag), dazu die Antwort-Auswerte-Logik aus
`m100_read_label_data_storage()` (Zeilen 256–277) 1:1 in eine eigene `apply_bank_response()`-
Testfunktion übertragen. Drei Fälle, `gcc -std=c17 -Wall -Wextra -O0`/`-O1
-fsanitize=address,undefined -fno-sanitize-recover=undefined`:
- Fall 1 (Regression): ehrliche 128-Byte-TID-Antwort (eigener 64-Wort-Anfrage-Deckel eingehalten) —
  ALT und NEU identisch korrekt gespeichert.
- Fall 2 (der Fund, TID-Bank): Antwort meldet 300 Byte Nutzdaten (> `MAX_BANK_SIZE`). **ALT: vom
  AddressSanitizer abgefangener `heap-buffer-overflow`-Schreibzugriff der Größe 300, exakt am Ende
  der 208-Byte-`UHFTag`-Teilallokation** (`WRITE of size 300 ... 0 bytes after 208-byte region`),
  Prozess bricht vor Erreichen der Prüfung ab; **NEU: sauber auf 200 Byte geklemmt**, `tid->size ==
  200`.
- Fall 3 (derselbe Fund über die User-Bank, 512 gemeldete Byte): identisches Bild — ALT bricht
  unter ASan ab, NEU klemmt korrekt auf 200.

Tatsächliches Ergebnis (lokal ausgeführt, beide Optimierungsstufen identisch): ALT stürzt bereits
bei Fall 2 unter ASan ab (Fall 1 lief zuvor sichtbar durch, `PASS`); NEU **ALL CHECKS PASSED
(3 total, 0 failed)**, keine Compiler-Warnungen.

### Betroffene Dateien
- `simultaneous_rfid_reader/helpers/yrm100x_tag.c` — `uhf_tag_set_tid()`/`uhf_tag_set_user()`,
  einzige Fix-Stellen.
- `simultaneous_rfid_reader/helpers/yrm100x_module.c` — `m100_read_label_data_storage()`, Quelle
  der ungeklemmten `bank_data_length`, keine Änderung nötig (Fix sitzt bewusst in den Settern, die
  die tatsächliche Zielgröße kennen).

### Testhinweis (lokal, YRM100-Hardware nötig für den Funktionstest)
Patch gegen die eigene `Simultaneous-UHF-RFID-FlipperZero`-Kopie anwenden, mit `ufbt`/`fbt` bauen.
Regressionsfall: normales Lesen der TID-/User-Bank eines eigenen, protokollkonformen UHF-Tags muss
vor und nach dem Fix identisch funktionieren (siehe Host-Repro Fall 1). Der eigentliche Fund lässt
sich ohne ein absichtlich non-konformes Testgerät nicht risikofrei am echten Lesegerät auslösen —
der Host-Repro deckt die Speichersicherheit datenstrukturell ab (Fall 2/3).

### Sicherheitsrahmen
Defensiver Fund an einem eigenen Werkzeug für eigene/autorisierte UHF-RFID-Hardware: die Klemme
schützt das eigene Gerät davor, dass eine fehlerhafte oder absichtlich non-konforme
Tag-/Lesegerät-Antwort beim autorisierten Testen eigener Zutrittsmedien zu einem Speicherfehler auf
dem Flipper führt — keine Umgehung einer fremden Schutzfunktion.

### Risiko
Gering: additive Zwei-Zeilen-Klemme an der bestehenden Zielgröße, keine Signatur-/Strukturänderung,
Host-Repro Fall 1 bestätigt den unveränderten Normalfall explizit.

## Fund 3: `view_read.c::convert_to_hex_string()` — kein Nullterminator, Rückgabewert nirgends freigegeben

Die zuletzt noch offenen zwölf UI-`views/*.c`-Dateien jetzt vollständig gelesen (4620 Zeilen).
Fund im zentralen Passwort-Hex-Formatierer `convert_to_hex_string()` (`view_read.c`), identisch
zum bereits dokumentierten `convertToHexString()`-Fehler im Ursprungsprojekt `uhf_rfid`
(`firmware-mining/uhfrfid-hexstring-oob-read-fix.md`) — hier mit in die Views dieses Forks
gewandert: fehlende `+1` bei Allokation/Kopie für den Nullterminator (Heap-Buffer-Overflow-Read
bei allen fünf Aufrufstellen in `view_config.c`/`view_kill.c`/`view_lock.c`, die den Rückgabewert
per `snprintf(..., "%s", ...)` weiterverarbeiten) sowie ein an keiner der fünf Aufrufstellen
behobenes Speicherleck (anders als bei `uhf_rfid`, wo stattdessen ein unconditional `free()` im
Leerfall abstürzt). Zusätzlicher Nebenfund in `view_config.c:731–735`: eine tote `strncpy()`-
Anweisung auf einen sofort verworfenen `convert_to_hex_string()`-Rückgabewert, die den im
Kommentar behaupteten Effekt ("Modify the value of the AP") gar nicht erzielt — die tatsächliche
Aktualisierung von `App->ApTempBuffer` passiert ausschließlich über den separat registrierten
`byte_input`-Widget-Callback.

Vollständiger Befund, Patch (`view_read_c.patch`, nur die eine Funktion) und Host-Repro unter
AddressSanitizer/UBSan/LeakSanitizer (drei Szenarien: Overflow-Read, Leck trotz reinem
Funktions-Fix, saubere Kombination aus Fix + `free()` an der Aufrufstelle) in
`ideas/simultaneous-rfid-reader-convert-to-hex-string-no-terminator-fix.md`.

## Nächster Schritt (aktualisiert)
YRM100-Kern und alle zwölf UI-`views/*.c`-Dateien dieses Forks jetzt vollständig gelesen, kein
offener Erstprüfungs-Rückstand mehr. Weiterhin offen: lokaler Build-/Funktionstest aller drei
bisher gelieferten Fixes (`ring_buffer_extract_line()`, `uhf_tag_set_tid()`/`_set_user()`,
`convert_to_hex_string()`); das in Fund 3 benannte, bewusst nicht mitgezogene Speicherleck an den
fünf Aufrufstellen sowie der tote `strncpy()`-Nebenfund in `view_config.c:731–735` als
eigenständige Nacharbeit für eine künftige Sitzung.
