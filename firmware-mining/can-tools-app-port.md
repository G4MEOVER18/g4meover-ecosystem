# CAN Tools — externe App von Matthew KuKanich (DBC-Erstellung, CAN-Signaldekodierung)

## Was

Ich habe gezielt nach einer bislang komplett fehlenden Kategorie gesucht: Fahrzeug-CAN-Bus als
Angriffs-/Auditfläche, über meine bisherige reine Bus-*Analyse*- und Debug-Port-Sammlung (UART
Sniff, GPIO Logic Analyzer, i2c Tools, RS485 Modbus, SWD Probe) und den Chip-*Dump* (SPI Flash
Dump) hinaus. Im RogueMaster-Bündel (`applications/external/`, Branch `420`) liegen dafür fünf
eigenständige Apps: `can_bus_attack`, `can_commander`, `can_tools`, `can_transceiver`, `canbus` —
alle brauchen zusätzliche Hardware (ein externes CAN-Transceiver-Modul, meist MCP2515, am
Erweiterungsport), da der Flipper selbst keinen CAN-Transceiver besitzt.

Als ersten, kleinsten und am klarsten abgeschlossenen Fund aus diesem Cluster **CAN Tools**
(`MatthewKuKanich/FlipperCANTools`, Lizenz laut Repo MIT, App-ID `can_tools`, Kategorie `Tools`)
gewählt: einzige der fünf Apps **ohne** jede Hardware-Abhängigkeit — reine DBC-Datei-Erstellung
und Signaldekodierung, komplett offline nutzbar (z. B. um am Tisch eine `.dbc`-Datei für ein
Fahrzeug anzulegen oder zu testen, bevor man sie am Fahrzeug mit `can_commander`/`canbus`
einsetzt). Vollständig gelesen (eine Quelldatei, `cantools_app.c`, 2419 Zeilen).

Die App bietet vier Funktionen über ein Scene-Manager-Menü: **DBC Maker** (Signal-Felder wie
CAN-ID/Startbit/Bitlänge/Endianness/Signedness/Skalierung per Texteingabe erfassen, als
`BO_`/`SG_`-Zeilen im DBC-Format auf die SD-Karte schreiben), **View DBC** (gespeicherte Signale
einer `.dbc`-Datei auflisten/anzeigen/bearbeiten/löschen), **Decode Data** (eine CAN-ID + bis zu
8 Datenbyte manuell eingeben, gegen alle Signale derselben CAN-ID aus der aktiven `.dbc`-Datei
dekodieren) und **DBC Files** (mehrere `.dbc`-Dateien unter `apps_data/can_tools/` verwalten,
neue anlegen). Ein fünfter Menüpunkt "Decode log" ist laut Quellcode noch ein reiner Platzhalter
("Coming soon").

## Nutzen für mich

Schließt die Lücke zwischen reiner Bus-*Analyse* und tatsächlicher *Interpretation* von
CAN-Frames: `can_commander`/`canbus`/`can_transceiver` liefern rohe CAN-Frames vom Bus, aber ohne
eine DBC-Definition bleiben die Byte-Inhalte bedeutungslose Hex-Werte. CAN Tools lässt mich eine
DBC-Datei für ein Zielfahrzeug am Schreibtisch aufbauen/pflegen und einzelne Frames offline
gegenprüfen (z. B. aus einer vorher mit einer der Hardware-Apps aufgezeichneten Botschaft), bevor
ich mit echter Hardware am Fahrzeug arbeite — eigenständige, komplementäre Funktion, keine
Überschneidung mit den vier Hardware-Apps desselben Clusters, die in künftigen Sitzungen einzeln
aufgegriffen werden können.

## API-87-Kompatibilität (mntm-012)

Gegen eine frische, unabhängige Zweitkopie von `Next-Flip/Momentum-Firmware` (Tag `mntm-012`)
geprüft — die App nutzt ausschließlich Standard-GUI-/Storage-/Stream-APIs, keine
Erweiterungsport-Pins oder sonstige Hardware-HAL-Aufrufe (passend zum reinen Offline-Charakter):

- `gui/modules/{submenu,widget,text_input,byte_input}.h`: `submenu_reset/_set_header/_add_item`,
  `widget_reset/_add_text_scroll_element`, `text_input_reset/_set_header_text/
  _set_result_callback`, `byte_input_set_header_text/_set_result_callback` — alle Signaturen
  byteidentisch vorhanden, inklusive `byte_input_set_result_callback()`s sechs Parametern
  (Instanz, Input-Callback, Changed-Callback, Kontext, Bytepuffer, Puffergröße) exakt wie von
  `cantools_decode_byte_input_on_enter()` aufgerufen.
- `storage/storage.h`: `storage_simply_mkdir/_dir_open/_dir_read/_dir_close/_common_stat/
  _file_alloc/_free` — byteidentisch vorhanden.
- `stream/stream.h` + `stream/buffered_file_stream.h`: `buffered_file_stream_alloc/_open/_close`,
  `stream_read_line/_write_cstring/_write_char/_write_format` — byteidentisch vorhanden.
- `dialogs/dialogs.h`: `dialog_message_alloc/_free/_set_header/_set_text/_set_buttons/_show` —
  byteidentisch vorhanden.

Kein Port-Hindernis: die App kann unverändert aus `applications/external/can_tools/` übernommen
werden (reines FAP, kein Firmware-Core-Patch nötig).

## Fund: `dbc_parse_sg_line()` liest zwei Zeichen ohne Längenprüfung über das Zeilenende hinweg

`dbc_load_file()` liest beim Öffnen einer `.dbc`-Datei (z. B. einer mit `can_commander`/`canbus`
am Fahrzeug erstellten oder einer von einem anderen Nutzer/Fahrzeugmodell-Katalog übernommenen
Datei) jede Zeile per `strlcpy()` in einen lokalen `char buf[256]`-Stack-Puffer und reicht sie an
`dbc_parse_sg_line()` weiter, sobald sie mit `SG_` beginnt. Dieser Parser extrahiert Startbit und
Bitlänge über zwei durch `strtol()` begrenzte Felder (`...|...@`) und liest anschließend
**bedingungslos** zwei weitere Zeichen für Endianness und Signedness:

```c
p = end + 1;

char endian_char = *p++;   // kein Bound-Check: ist nach dem "@" ueberhaupt noch ein Zeichen da?
char sign_char = *p++;     // kein Bound-Check: und danach noch eins?
```

Anders als bei jedem anderen Feld im selben Parser (Startbit, Bitlänge, Skalierung, Offset,
Min/Max, Einheit — jeweils mit `if(end == p || *end != '…') return false;` bzw. einer
Anführungszeichen-Prüfung abgesichert) gibt es hier **keine** Prüfung, ob nach dem `@` überhaupt
noch Zeichen folgen. Für eine Zeile, die exakt mit `@` endet (z. B. durch einen unvollständigen
Export, eine manuell editierte oder eine aus einem anderen DBC-Dialekt konvertierte Datei), liegt
die erste Leseposition auf dem Nullterminator der Zeile selbst — noch innerhalb des 256-Byte-
Puffers, aber bereits falscher Inhalt (kein echtes Endianness-Zeichen) — und die **zweite**
Leseposition liegt ein Byte **dahinter**. Das liegt in aller Regel noch innerhalb des
deklarierten Arrays (dann nur ein falscher, uninitialisierter Signedness-Wert für diese eine
Zeile, stillschweigend ohne Fehlermeldung), wird aber zu einem echten Lesezugriff **außerhalb**
des Arrays, sobald die Zeile (nach der durch `strlcpy()` auf 255 Zeichen + Nullterminator
begrenzten Kürzung) exakt 255 Byte lang ist und mit `@` endet — dann liegt der Nullterminator
exakt auf dem letzten gültigen Index des Puffers (Index 255), und der zweite Zeichenzugriff
liegt auf Index 256, einen Byte hinter dem deklarierten `char buf[256]`. Der Signalname-Scan
selbst begrenzt die Zeilenlänge dabei nicht (nur die spätere Kopie nach `sig->signal_name`
kappt auf `TEXT_INPUT_SIZE`), ein entsprechend langer Signalname reicht als Auslöser.

Betrifft nur das Laden aus einer Datei (`dbc_load_file()`, über "DBC Files" → vorhandene Datei
auswählen) — die eigene DBC-Maker-Eingabemaske validiert Startbit/Bitlänge/Endianness/
Signedness beim manuellen Anlegen bereits vollständig (`validate_endianness()`/
`validate_signedness()`), ist von diesem Fund also nicht betroffen.

## Host-Repro

Vier Szenarien: (1) Regressionsfall — vollständige, gültige `SG_`-Zeile, ALT und NEU liefern
identische `DbcSignal`-Felder; (2) Fundfall 1 — Zeile endet direkt nach `@` (`"SG_ X : 0|1@"`),
ALT liest 1 Byte hinter dem Nullterminator (noch innerhalb des Arrays, aber uninitialisiert),
NEU lehnt die Zeile kontrolliert ab; (3) Fundfall 2 — Zeile exakt 255 Byte lang (per
Signalname-Füllung konstruiert, `strlen() == 255`), Nullterminator exakt auf `buf[255]`: ALT
liest `buf[256]`, **außerhalb** des deklarierten Arrays — von AddressSanitizer als
`stack-buffer-overflow READ of size 1` erkannt (zusätzlich direkt, ohne Fork-Isolation,
gegengeprüft: `AddressSanitizer: stack-buffer-overflow … READ of size 1 … This frame has 1
object(s): [32, 288) 'line' <== Memory access at offset 288 overflows this variable` — 288 - 32
= 256, exakt 1 Byte hinter dem deklarierten Array), NEU lehnt die Zeile ab, bevor `buf[255]`/
`buf[256]` gelesen werden. Unter `gcc -std=c11 -O0`/`-O1 -Wall -Wextra
-fsanitize=address,undefined -fno-sanitize-recover=undefined` verifiziert: alle vier Szenarien
bestätigt, identischer Output beider Optimierungsstufen, keine Compiler-Warnungen — in
`firmware-mining/repro-can-tools-sg-line-parser-oob-read/repro.c`.

## Betroffene Datei

- `applications/external/can_tools/cantools_app.c` — Funktion `dbc_parse_sg_line()`.

## Port-Skizze

1. App unverändert aus `applications/external/can_tools/` (Branch `420` des RogueMaster-
   Bündels) in meine eigene `applications/external/can_tools/`-Struktur übernehmen (reines FAP,
   keine Firmware-Core-Änderung).
2. Patch `firmware-mining/can-tools-app-port/cantools_app_c.patch` danach anwenden — fügt eine
   additive Prüfung direkt vor den beiden bisher ungeschützten Zeichenzugriffen ein, nach
   demselben Muster wie die bereits vorhandenen Feld-Prüfungen im selben Parser:

```c
    long bit_length = strtol(p, &end, 10);
    if(end == p || *end != '@') return false;
    p = end + 1;

    if(!p[0] || !p[1]) return false;   // NEU
    char endian_char = *p++;
    char sign_char = *p++;
```

   Patch per `git apply --check` gegen eine frische, unabhängige Zweitkopie des RogueMaster-
   Bündels (Branch `420`, nur `applications/external/can_tools/`) verifiziert, real angewendet,
   byte-für-byte mit der Arbeitskopie abgeglichen — identisch.

## Testhinweis (lokal, ohne Hardware)

1. `ufbt`/`fbt`-Build gegen die eigene `mntm-012`-Basis mit `can_tools` in
   `applications/external/`.
2. Am Gerät: "DBC Files" → "Create DBC" → mit "DBC Maker" mehrere gültige Signale anlegen,
   "View DBC" → Signale korrekt angezeigt (Regressionstest, unverändert durch den Patch).
3. Fundnachvollzug (erfordert eine präparierte `.dbc`-Datei auf der SD-Karte, z. B. per PC/
   Kartenleser in `apps_data/can_tools/` abgelegt, mit einer `SG_`-Zeile, die exakt mit `@`
   endet): vor dem Patch keine sichtbare Fehlermeldung, aber ein zufälliges/falsches
   Signedness-Zeichen für dieses Signal (sichtbar z. B. als unerwartet negativer statt positiver
   dekodierter Wert in "Decode Data"); nach dem Patch wird die betroffene Zeile beim Laden
   stillschweigend übersprungen (gleiches Verhalten wie bei den bereits vorhandenen, ebenso
   kommentarlos verworfenen ungültigen Zeilen des Parsers).
4. Mit und ohne Patch: "Decode Data" mit einer regulären, bereits vorhandenen Signaldefinition
   durchführen — identisches Ergebnis vor/nach dem Patch (Regressionstest für den Normalfall).

## Risiko

Niedrig. Additive Prüfung, identisches Rückgabeverhalten (`return false`, bereits der
bestehende Fehlerpfad des Parsers für jede andere unvollständige Zeile) wie alle übrigen
Feld-Validierungen im selben Parser — keine Verhaltensänderung für vollständige, gültige
`SG_`-Zeilen (per Host-Repro-Regressionsfall belegt). Betrifft ausschließlich das Laden
externer `.dbc`-Dateien, nicht die eigene DBC-Maker-Eingabemaske.

Separat notiert, bewusst nicht in diesem Patch behoben: `sanitize_filename()` (selbe Datei)
prüft Zeichen mit `(c >= 'A' && c <= 'z')` statt `(c >= 'A' && c <= 'Z')` — lässt dadurch fünf
zusätzliche ASCII-Zeichen (`[`, `\`, `]`, `^`, `` ` ``) unersetzt durch den Dateinamen-Filter.
Da `.` (für `..`-Traversal) bereits nicht im erlaubten Zeichensatz enthalten ist und Flipper-
Pfade `/` statt `\` als Trenner verwenden, ohne erkennbare Sicherheitswirkung — eigenständiger,
kleinerer Folgefund für eine künftige Sitzung.

## Sicherheitsrahmen

Reiner Lese-/Parserfund in einer Offline-Hilfsanwendung (kein Netzwerk-/Funkkanal, keine
Interaktion mit einem Fahrzeug). Keine Kartendaten oder echte Zugangsdaten beteiligt. Fix ist
eine kontrollierte Ablehnung einer unvollständigen Zeile, keine neue Funktionalität.

## Quelle

- `MatthewKuKanich/FlipperCANTools` (direktes Repo) bzw. `applications/external/can_tools/` im
  RogueMaster-Bündel (`RogueMaster/flipperzero-firmware-wPlugins`, Branch `420`).
