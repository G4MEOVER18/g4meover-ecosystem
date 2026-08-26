# NFC EMV: dateikontrollierte Längen ungebunden beim Laden einer `.nfc`-Datei — Unleashed vs. mntm-012

Quelle: Unleashed-Firmware-Commit `2631895419075d7df8d71be7e3d93465fcc6a491` ("[NFC] Bound
file-controlled lengths in the EMV .nfc loader", PR #1056, @mishamyte, gemerged 31.07.2026), aus
dem `unlshd-091`-Fenster als der zuletzt noch offene Restfund neben dem bereits portierten
kartenkontrollierten TLV-Längen-Fix (`firmware-mining/emv-tlv-length-bounds-fix.md`). Gegen Tag
`mntm-012` geprüft: `lib/nfc/protocols/emv/emv.c` ist byteidentisch zum hier zitierten
Vorher-Stand — der Fund ist bei mir vollständig vorhanden.

Anders als beim bereits portierten Poller-Fix ist das Bedrohungsmodell hier nicht die Karte,
sondern die Datei: `emv_load()` wird beim Öffnen eines gespeicherten `.nfc`-Datensatzes aus dem
Dateisystem aufgerufen (eigenes Archiv, per SD-Karte/USB/Bluetooth/`bad_usb` empfangene Datei,
geteilte Capture). Eine präparierte `.nfc`-Datei überschreibt Speicher, sobald sie geladen wird —
kein Kartenkontakt nötig.

## Was

`emv_load()` (`lib/nfc/protocols/emv/emv.c`) liest mehrere Felder direkt aus der `.nfc`-Datei
(`FlipperFormat`) und verwendet ihre Werte ungeprüft als Schreibziel-/Schreiblängen in die
heap-allokierte `EmvApplication` (Teil von `EmvData`, `malloc(sizeof(EmvData))` in `emv_alloc()`):

```c
// lib/nfc/protocols/emv/emv.c, emv_load() -- bei mir (mntm-012) so vorhanden:
flipper_format_read_string(ff, "Cardholder name", temp_str);
strcpy(app->cardholder_name, furi_string_get_cstr(temp_str)); // Ziel nur 25 Byte, Datei-String beliebig lang

flipper_format_read_string(ff, "Application name", temp_str);
strcpy(app->application_name, furi_string_get_cstr(temp_str)); // Ziel nur 17 Byte

flipper_format_read_string(ff, "Application label", temp_str);
strcpy(app->application_label, furi_string_get_cstr(temp_str)); // Ziel nur 17 Byte

uint32_t pan_len;
if(!flipper_format_read_uint32(ff, "PAN length", &pan_len, 1)) break;
app->pan_len = pan_len;                                         // (uint32_t -> uint8_t, aber s.u.)

if(!flipper_format_read_hex(ff, "PAN", app->pan, pan_len)) break; // Ziel nur 10 Byte, pan_len bis zu 4G

uint32_t aid_len;
if(!flipper_format_read_uint32(ff, "AID length", &aid_len, 1)) break;
app->aid_len = aid_len;

if(!flipper_format_read_hex(ff, "AID", app->aid, aid_len)) break; // Ziel nur 16 Byte
```

Drei unabhängige Fehlerklassen in derselben Funktion:

- **Drei unbegrenzte `strcpy()`** in `char[]`-Felder fester Breite (`cardholder_name[24+1]`,
  `application_name[16+1]`, `application_label[16+1]`) — ein beliebig langer String-Wert in der
  Datei überschreibt den Heap hinter dem jeweiligen Feld.
- **`pan_len`/`aid_len` als `uint32_t` aus der Datei gelesen und ungeprüft an
  `flipper_format_read_hex()` als Schreiblänge übergeben**, bevor sie in das 8-Bit-Feld
  zugewiesen werden (`app->pan_len = pan_len` steht sogar *vor* dem eigentlichen `read_hex`-Aufruf
  im Fall von `pan_len`, ändert aber nichts: der schreibende Zugriff selbst benutzt weiterhin den
  ungetruncateten 32-Bit-Wert). `app->pan` ist nur `uint8_t[10]`, `app->aid` nur `uint8_t[16]` —
  ein Wert wie 200 bzw. 255 in der Datei schreibt entsprechend viele Byte in ein 10- bzw.
  16-Byte-Feld.
- Ein zu großer `pan_len` liefe zusätzlich in jeder späteren Anzeige-/Renderfunktion, die über
  `app->pan_len` Byte aus `app->pan` iteriert, über das Array hinaus (Folgefehler, nicht separat
  repliziert, siehe Testhinweis).

Nebenbefund aus demselben Commit, bei mir ebenfalls vorhanden: die drei
`flipper_format_read_string()`-Rückgabewerte werden verworfen — schlägt ein Lesen fehl, bleibt
`temp_str` auf dem Wert des vorherigen Schlüssels stehen und wird trotzdem per `strcpy` in das
nächste Feld kopiert. Nur erreichbar, wenn ein Schlüssel fehlt (das lässt den Ladevorgang ohnehin
später scheitern), speist aber genau den oben beschriebenen Überlauf. Zusätzlich in `emv_save()`:
`(uint32_t*)&app.pin_try_counter` liest den Zeiger auf ein `uint8_t`-Feld als `uint32_t*` und
schreibt drei benachbarte Fremd-Byte mit in die Datei (kein Speicherfehler beim Laden, aber ein
Datenleck/Garbage-Wert in jeder gespeicherten `.nfc`-Datei).

## Nutzen

- Schließt eine Speicherüberschreibung, die allein durch das **Öffnen** einer präparierten
  `.nfc`-Datei ausgelöst wird — kein NFC-Lesevorgang, kein Kartenkontakt nötig. Relevant für mich,
  weil ich Captures zwischen eigenen Geräten austausche und testweise Dateien aus fremden
  Quellen (CTF-Material, Pentest-Testsets) öffne, bevor ich weiß, ob sie vertrauenswürdig sind.
- Eigenständiger Angriffsvektor gegenüber dem bereits portierten Poller-Fix (kartenkontrolliert)
  — schließt dieselbe EMV-Absicherungswelle aus `unlshd-091` vollständig ab.
- Verhält sich für regulär von mir selbst gespeicherte `.nfc`-Dateien identisch zu vorher (siehe
  Testhinweis): kein Funktionsverlust im Erfolgsfall, nur Ablehnung/Kürzung bei Werten, die
  ohnehin nicht in die vorgesehenen Felder passen.
- Behebt zusätzlich den `pin_try_counter`-Schreibfehler in `emv_save()` (drei Fremd-Byte landen
  sonst mit in jeder von mir gespeicherten `.nfc`-Datei).

## Betroffene Dateien (Portumfang)

- `lib/nfc/protocols/emv/emv.c`
  - Neu: `#define TAG "EMV"`, `emv_load_string()` (per `strlcpy()` begrenzte Kopie mit
    `FURI_LOG_W`-Warnung bei Kürzung) und `emv_load_length()` (liest den `uint32_t`-Wert, lehnt
    bei Überschreitung von `max_len` mit `FURI_LOG_E` ab, setzt `*len` nur bei Erfolg).
  - `emv_load()`: alle drei `strcpy()`-Aufrufe durch `emv_load_string()` ersetzt; `pan_len`/
    `aid_len` durch `emv_load_length()` vor dem jeweiligen `read_hex`-Aufruf geprüft, `uint8_t`
    statt `uint32_t`, Zuweisung an `app->pan_len`/`app->aid_len` erst **nach** dem erfolgreichen
    `read_hex`-Aufruf (beschreibt dann immer, was tatsächlich gelesen wurde). Nicht mehr benötigtes
    `temp_str` in `emv_load()` entfernt (jetzt lokal in `emv_load_string()` allokiert/freigegeben).
  - `emv_save()`: `pin_try_counter`-Schreibfehler behoben (lokale `uint32_t`-Kopie statt
    Zeiger-Fehlcast), nicht benutztes `temp_str` entfernt.

Keine Änderung an `emv.h` nötig — die Feldgrößen selbst sind bereits korrekt, nur die Kopier-/
Prüflogik beim Laden fehlt.

## Risiko

- Kleiner, lokal begrenzter Umfang: eine Implementierungsdatei, zwei neue statische
  Hilfsfunktionen plus Ersetzung bestehender Zeilen in `emv_load()`/`emv_save()` — keine
  Struktur-/API-Änderung, keine neuen Felder.
- Verhaltensänderung im Fehlerfall: eine `.nfc`-Datei mit überlanger `PAN length`/`AID length`
  wird jetzt beim Laden abgelehnt (`emv_load()` gibt `false` zurück), statt Speicher zu
  überschreiben. Für alle von mir selbst über `emv_save()` erzeugten Dateien ändert sich nichts,
  da `pan_len`/`aid_len` dort bereits die tatsächliche Array-Länge sind (max. 10 bzw. 16) — nur
  eine manuell manipulierte oder aus einer fremden Quelle stammende Datei kann den abgelehnten
  Fall überhaupt auslösen.
- Namensfelder werden bei Überlänge gekürzt statt den Ladevorgang abzubrechen (bewusste
  Asymmetrie aus dem Upstream-Diff: `emv_save()` schreibt diese Schlüssel auch für eine Karte ohne
  entsprechenden Tag, ein Ladevorgang darf daran nicht scheitern; die Längenfelder dagegen werden
  als Schreiblänge für binäre Felder gebraucht, ein gekürzter Wert wäre schlechter als ein
  abgelehnter Ladevorgang, siehe Upstream-Commit-Nachricht).
- Der Build (`fbt`/`ufbt`) deckt jede Signatur-Abweichung als Compile-Fehler auf. `strlcpy()` ist
  auf meiner Basis bereits an mehreren Stellen im Einsatz (u. a. `lib/toolbox/path.c`,
  `lib/momentum/settings.c`, `applications/services/storage/storages/storage_ext.c`) und Teil der
  API-87-Symboltabelle (`targets/f7/api_symbols.csv`) — keine neue Abhängigkeit.

## Port-Skizze

1. `lib/nfc/protocols/emv/emv.c` gegen meine mntm-012-Basis gegenlesen — bei der Prüfung für
   diesen Vorschlag byteidentisch zum zitierten Vorher-Stand.
2. `#define TAG "EMV"` nach den bestehenden Includes ergänzen.
3. `emv_load_string()` und `emv_load_length()` als neue statische Hilfsfunktionen vor `emv_load()`
   einfügen (siehe Upstream-Diff/Quelle).
4. In `emv_load()`: die drei `strcpy()`-Blöcke durch je einen `emv_load_string()`-Aufruf ersetzen;
   `pan_len`/`aid_len` auf `uint8_t` umstellen, `emv_load_length()` vor dem jeweiligen
   `read_hex`-Aufruf einfügen, `app->pan_len`/`app->aid_len` erst nach erfolgreichem `read_hex`
   setzen. `FuriString* temp_str`-Allokierung/-Freigabe am Anfang/Ende der Funktion entfernen.
5. In `emv_save()`: `pin_try_counter` über eine lokale `uint32_t`-Kopie schreiben statt über
   `(uint32_t*)&app.pin_try_counter`. Nicht benutztes `temp_str` entfernen.
6. Host-Repro (`repro.c`, unten) mit `gcc -O0 -g -Wall -Wextra -fsanitize=address,undefined`
   bauen und laufen lassen — bereits von mir lokal ausgeführt, siehe Testhinweis.
7. Build: `ufbt` (bzw. `fbt firmware_all`).
8. Funktionstest lokal: ein vorhandenes, mit meinem eigenen Gerät gespeichertes EMV-`.nfc`-Capture
   erneut öffnen (Ergebnis unverändert: PAN/AID/Kartenname wie vorher, keine Kürzung, kein
   abgelehntes Laden). Danach eine Kopie der Datei per Texteditor manipulieren (`PAN length` auf
   z. B. `200` setzen) und versuchen zu laden — vor dem Fix bricht das reale Verhalten
   undefiniert ab (Speicherüberschreibung, potenziell Absturz/Fehlverhalten), nach dem Fix wird
   der Ladevorgang sauber mit `false` abgelehnt (per `FURI_LOG_E` im Log sichtbar).

## Testhinweis (lokal, ohne Hardware — reine Datei-/Speicherlogik)

Host-kompilierbares Mini-Repro (`repro-emv-nfc-loader-length-bounds/repro.c`) aller drei
Einzelfälle (Namensfeld-`strcpy`, `PAN length`, `AID length`), mit heap-allokiertem Zielfeld wie
in der echten `EmvData` (`malloc()`, analog zu `emv_alloc()`). ALT ist 1:1 die bei mir vorhandene
Logik, NEU die gebundene Fassung aus Commit `2631895419`. Jeder Testfall läuft isoliert in einem
`fork()`-Kindprozess. Gebaut/ausgeführt mit `gcc -O0 -g -Wall -Wextra
-fsanitize=address,undefined`:

```
== Fall A: Cardholder-Name strcpy() (Ziel: cardholder_name[25]) ==
  ALT, realistischer Name (18 Zeichen): kein Absturz
  NEU, realistischer Name (18 Zeichen): kein Absturz
  ALT, ueberlanger Dateiwert (299 Zeichen, Ziel nur 25 Byte): ABSTURZ (von ASan/Signal erkannt)
  NEU, ueberlanger Dateiwert (299 Zeichen, Ziel nur 25 Byte): kein Absturz

== Fall B: "PAN length" (Ziel: pan[10]) ==
  ALT, realistische Laenge (8): kein Absturz
  NEU, realistische Laenge (8): kein Absturz
  ALT, dateikontrollierte Laenge (200, Ziel nur 10 Byte): ABSTURZ (von ASan/Signal erkannt)
  NEU, dateikontrollierte Laenge (200, Ziel nur 10 Byte): kein Absturz

== Fall C: "AID length" (Ziel: aid[16]) ==
  ALT, realistische Laenge (7): kein Absturz
  NEU, realistische Laenge (7): kein Absturz
  ALT, dateikontrollierte Laenge (255, Ziel nur 16 Byte): ABSTURZ (von ASan/Signal erkannt)
  NEU, dateikontrollierte Laenge (255, Ziel nur 16 Byte): kein Absturz

OK: ALT stuerzt in allen drei Faellen bei dateikontrolliertem Ueberlaengen-Wert unter
ASan als heap-buffer-overflow ab, NEU (Kuerzung/Ablehnung aus Commit 2631895419) nicht.
Fuer realistische Werte verhalten sich ALT und NEU identisch.
```

Die von ASan gemeldete Fehlerklasse ist in allen drei Fällen dieselbe
(`heap-buffer-overflow ... WRITE`, Zugriff unmittelbar hinter dem `malloc()`-allokierten
Zielfeld) — beispielhaft für Fall B (10-Byte-Feld, 200-Byte-Schreibversuch):

```
==...==ERROR: AddressSanitizer: heap-buffer-overflow on address ...
WRITE of size 1 at ... thread T0
0x... is located 0 bytes after 11-byte region [...] allocated by thread T0 here:
    #0 ... in malloc ...
```

Bestätigt: die ALT-Fassung überschreibt in allen drei Fällen den jeweiligen heap-allokierten
Puffer bei dateikontrolliertem Überlängen-Wert über seine Grenze hinaus; die NEU-Fassung kürzt
(Namensfelder, kosmetisch) bzw. lehnt den Ladevorgang kontrolliert ab (Längenfelder). Für
realistische, selbst gespeicherte Werte verhalten sich beide Fassungen identisch — kein
Funktionsverlust im Regelfall. Der `pin_try_counter`-Schreibfehler in `emv_save()` ist ein reiner
Datei-Inhaltsfehler (keine Speicherüberschreibung) und wurde nicht separat host-repliziert.

## Quelle

- `DarkFlippers/unleashed-firmware`, Commit `2631895419075d7df8d71be7e3d93465fcc6a491` (PR #1056,
  @mishamyte, gemerged 31.07.2026): „[NFC] Bound file-controlled lengths in the EMV .nfc loader" —
  `lib/nfc/protocols/emv/emv.c`.
- Momentum-Firmware, Tag `mntm-012`: `lib/nfc/protocols/emv/emv.c` (byteidentisch zum zitierten
  Vorher-Stand) als Vergleichsbasis.
- Damit ist das gesamte `unlshd-091`-EMV-Absicherungsfenster abgeschlossen: kartenkontrollierte
  TLV-Längen im Poller (`firmware-mining/emv-tlv-length-bounds-fix.md`, Commit `15e208c0`) und
  dateikontrollierte Längen im `.nfc`-Loader (dieser Vorschlag, Commit `2631895419`).
