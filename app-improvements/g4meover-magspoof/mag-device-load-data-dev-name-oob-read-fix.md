# g4meover-magspoof — Heap-OOB-Read bei `dev_name` nach `mag_device_load_data()`

## Was
Meine MagSpoof-App basiert auf `zacharyweiss/magspoof_flipper`. `mag_device_load_data()`
(`mag_device.c`) übernimmt beim Laden einer `.mag`-Datei den Dateinamen (ohne Endung) als
Anzeigenamen des Geräts:

```c
FuriString* filename;
filename = furi_string_alloc();
path_extract_filename(path, filename, true); // true = Endung abschneiden
strncpy(mag_dev->dev_name, furi_string_get_cstr(filename), MAG_DEV_NAME_MAX_LEN);
```

`dev_name` ist ein `char[MAG_DEV_NAME_MAX_LEN + 1]` = `char[23]` in `MagDevice`
(`mag_device.h`), `MagDevice` selbst wird in `mag_device_alloc()` mit reinem `malloc()` ohne
Nullinitialisierung angelegt (`MagDevice* mag_dev = malloc(sizeof(MagDevice));`). `strncpy()`
schreibt nach C-Standard **kein** Terminierungszeichen, sobald die Quelle mindestens so lang
wie die angegebene Länge ist — genau der Fall, sobald der Dateiname (ohne `.mag`-Endung)
22 Zeichen oder länger ist. Der ungenutzte letzte Puffer-Byte (`dev_name[22]`) behält dann den
uninitialisierten Heap-Wert von `malloc()`, der mit hoher Wahrscheinlichkeit kein Nullbyte ist.

Das ist bei mir über jede Datei mit einem 22+ Zeichen langen Namen im `.mag`-Ordner erreichbar
(z. B. selbst unter einem entsprechend langen Namen gespeicherte Karte, oder eine Datei, die
ich mit `dolphin`/PC einfach entsprechend benannt in den Ordner kopiere) — kein präpariertes
Dateiformat nötig, nur ein langer Dateiname.

Ob sich das danach auswirkt, hängt vom Aufrufpfad ab:
- **Interaktiv über „Load file"** (`mag_file_select()`): dort folgt direkt
  `mag_device_set_name(mag_dev, mag_dev->dev_name)` — ein Selbst-Kopie-Aufruf
  (`strlcpy(dst, dst, MAG_DEV_NAME_MAX_LEN)` mit `dst == src`). Das habe ich mit einem
  eigenständigen Nachbau des in meiner Ziel-Toolchain (`arm-none-eabi-gcc`/newlib,
  OpenBSD-Referenzimplementierung) verwendeten `strlcpy()` durchgerechnet: bei einer echten
  Selbst-Kopie laufen Lese- und Schreibzeiger stets synchron, sodass das von `strlcpy()` selbst
  gesetzte Terminierungszeichen innerhalb des Puffers (Index 21) sofort vom nachfolgenden
  Scan gelesen wird — dieser eine Aufrufpfad bleibt also unkritisch (kein OOB), reparaturiert
  `dev_name` aber auch nur zufällig als Nebeneffekt der Selbst-Kopie.
- **App-Start mit Dateipfad-Argument** (`mag.c`, `mag_device_load_data(mag->mag_dev, mag->args,
  true);` — z. B. „Favorite"/„Open with" aus dem Dateiverwalter): hier folgt **keinerlei**
  `mag_device_set_name()`-Aufruf. `dev_name` bleibt in diesem Pfad ohne jede Terminierung, falls
  der Dateiname 22+ Zeichen hat.

Jeder spätere `%s`-Zugriff auf `mag_dev->dev_name` liest dann über das Ende der 23-Byte-Struktur
hinaus, bis zufällig irgendwo im Heap ein Nullbyte auftaucht — ein waschechter
Out-of-Bounds-Heap-Read mit von unbeteiligtem Heap-Inhalt abhängiger Länge:
- `scenes/mag_scene_emulate.c`: `furi_string_printf(tmp_str, "%s\r\n", mag->mag_dev->dev_name);`
- `scenes/mag_scene_delete_confirm.c`: `furi_string_printf(tmp_str, "\e#Delete %s?\e#",
  mag_dev->dev_name);`
- `scenes/mag_scene_saved_info.c`: `furi_string_printf(tmp_str, "%s\r\n", mag->mag_dev->dev_name);`
- `mag_device.c`, `mag_device_delete()`-Fallback (`use_load_path` falsch/leer):
  `furi_string_printf(file_path, "%s/%s%s", MAG_APP_FOLDER, mag_dev->dev_name,
  MAG_APP_EXTENSION);`

Mit einem eigenständigen, hostseitigen Nachbau von `mag_device_load_data()`s Namensteil
(gleiches `char[23]`-Layout, `malloc()` ohne Zero-Init, `strncpy()` wie im Original) unter ASan
verifiziert: ein 22-Zeichen-Dateistamm erzeugt zuverlässig einen `heap-buffer-overflow`-Fund bei
`strlen(dev_name)` — „READ of size 24 ... 0 bytes after 23-byte region", exakt an der
Pufferende-Grenze (siehe Testhinweis).

## Nutzen
- Schließt einen aus einem gewöhnlichen, langen Dateinamen erreichbaren Heap-OOB-Read, der je
  nach Heap-Layout zu einem Absturz (unmapped-Page-Zugriff) oder zur Anzeige/Einbettung
  unbeteiligter Heap-Bytes als Text führen kann (Karten-Löschbestätigung, Karteninfo-Anzeige,
  Emulieren-Bildschirm, Lösch-Dateipfad).
- Ein-Zeilen-Fix an der eigentlichen Ursache (fehlende Terminierung direkt beim Laden), statt
  sich auf einen zufällig reparierenden Folgeaufruf in nur einem von zwei Aufrufpfaden zu
  verlassen — behebt damit sowohl den bereits als unkritisch analysierten interaktiven Pfad
  robust mit als auch den tatsächlich betroffenen App-Start-mit-Pfad-Pfad.
- Reine String-/Puffergrößen-Änderung, keine Struct- oder API-Änderung; bereits vorhandene
  `strlcpy()`-Semantik (schon an anderer Stelle in `mag_device_set_name()` verwendet) wird nur
  konsequent auch beim Laden eingesetzt.
- Vollständig ohne Hardware nachrechenbar (reine Heap-/String-Logik).

## Betroffene Dateien (lokal, in meiner MagSpoof-App)
- `mag_device.c` — `mag_device_load_data()`: die eine `strncpy(...)`-Zeile für `dev_name`.

Keine Änderung an `mag_device.h`, `mag_device_set_name()`, den Szenen-Dateien oder am
`.mag`-Dateiformat nötig — der Fix schließt die Lücke direkt an der Quelle, alle lesenden
Stellen (Szenen, `mag_device_delete()`) profitieren automatisch mit.

## Patch
```c
// mag_device.c -- mag_device_load_data()
bool mag_device_load_data(MagDevice* mag_dev, FuriString* path, bool show_dialog) {
    bool parsed = false;

    FuriString* filename;
    filename = furi_string_alloc();
    path_extract_filename(path, filename, true);
    strlcpy(
        mag_dev->dev_name,
        furi_string_get_cstr(filename),
        sizeof(mag_dev->dev_name)); // NEU: strlcpy statt strncpy, volle Puffergroesse statt
                                     // MAG_DEV_NAME_MAX_LEN -- garantiert Terminierung
                                     // unabhaengig von der Dateinamenslaenge und vom
                                     // aufrufenden Codepfad (mag_file_select() vs.
                                     // App-Start mit Dateipfad-Argument in mag.c).

    /* ... Rest der Funktion unveraendert ... */
```

Einzige Änderung: `strncpy(mag_dev->dev_name, furi_string_get_cstr(filename),
MAG_DEV_NAME_MAX_LEN)` → `strlcpy(mag_dev->dev_name, furi_string_get_cstr(filename),
sizeof(mag_dev->dev_name))`. `strlcpy()` ist bereits über `mag_device_set_name()` im selben
Übersetzungsmodul verfügbar (kein neuer Include nötig); `sizeof(mag_dev->dev_name)` ist
`MAG_DEV_NAME_MAX_LEN + 1` (23), also die tatsächliche Puffergröße statt der um 1 zu kleinen
Konstante — dadurch nutzt der Fix, anders als der bestehende `mag_device_set_name()`-Aufruf,
auch das letzte Byte des Puffers für ein volles 22-Zeichen-Namensfeld statt nur 21 nutzbare
Zeichen zu belassen.

## Testhinweis (ohne Hardware)
Eigenständiges Reproduktionsprogramm der reinen Heap-/String-Logik (keine Furi-/Flipper-Typen
nötig): `app-improvements/g4meover-magspoof/repro-devname-load-oob-read/` — `repro_old.c`
(aktueller Stand, `strncpy`) und `repro_new.c` (mit Fix, `strlcpy` auf volle Puffergröße,
inkl. `strlcpy()`-Referenzimplementierung identisch zur newlib/OpenBSD-Fassung meiner
`arm-none-eabi-gcc`-Toolchain):

```sh
gcc -g -O0 -Wall -Wextra -fsanitize=address,undefined -o repro_old repro_old.c
gcc -g -O0 -Wall -Wextra -fsanitize=address,undefined -o repro_new repro_new.c
./repro_old   # 22-Zeichen-Dateistamm -> ASan: heap-buffer-overflow, READ of size 24,
              # "0 bytes after 23-byte region" -- exakt an der dev_name-Pufferende-Grenze,
              # Programm bricht mit SIGABRT ab (exit != 0)
./repro_new   # 22-Zeichen-Stamm: len=22, korrekt terminiert. 50-Zeichen-Stamm: sauber auf
              # 22 Zeichen gekappt und terminiert. 9-Zeichen-Stamm (Regressionsfall): len=9,
              # identisch zum bisherigen Verhalten. "ALL CHECKS PASSED", exit 0
```

Bei mir lokal reproduziert: `repro_old` bricht unter ASan mit
„AddressSanitizer: heap-buffer-overflow ... READ of size 24 ... 0 bytes after 23-byte region"
ab (Absturz, exit code 1); `repro_new` durchläuft alle drei Fälle (22-Zeichen-Grenzfall,
50-Zeichen-Kappung, 9-Zeichen-Regressionsfall) mit „ALL CHECKS PASSED" und exit code 0.

Funktionaler Test nach dem Patch, lokal auf dem Flipper (kein Sender/Empfänger nötig):
1. `ufbt` bauen, App starten.
2. Eine Karte unter einem 22+ Zeichen langen Namen speichern (z. B.
   `MyTestMagstripeCard0123`), danach über „Load file" laden — Anzeige des Namens auf
   „Saved Info"/„Delete?"/„Emulate" muss den vollen (ggf. auf 22 Zeichen gekappten) Namen ohne
   sichtbare Zeichenmüll-Reste zeigen.
3. Kritischer Testschritt (bislang unkritischer Pfad wird real relevant): dieselbe Datei über
   den Dateiverwalter direkt öffnen bzw. als „Favorite" starten (App-Start mit
   Dateipfad-Argument, `mag.c`) statt über „Load file" — auch hier darf auf keinem der drei
   genannten Bildschirme zusätzlicher Text nach dem Namen erscheinen; ein Absturz beim direkten
   Öffnen einer entsprechend lang benannten Datei wäre vor dem Fix der schlimmste Fall.
4. Regressionsfall: eine normal kurz benannte Karte (< 22 Zeichen, wie die mitgelieferten
   Beispieldateien `TestMagstripe.mag`/`SamyExpiredCard.mag`) zeigt weiterhin exakt denselben
   Namen wie vor dem Fix.
5. Erst danach der eigentliche Hardware-Funktionstest (Sender/Empfänger) — das ist der Teil,
   der bei mir lokal passiert.

## Risiko
- Niedrig: Die Änderung betrifft ausschließlich eine Kopierfunktion/Puffergröße in einer
  bereits bestehenden Namens-Übernahme, keine Struct- oder API-Änderung, kein neuer Include.
  `strlcpy()` ist im selben Modul bereits über `mag_device_set_name()` in Gebrauch, also
  nachweislich für diese Toolchain verfügbar.
- Verhalten im Normalfall (Dateiname unter 22 Zeichen) bleibt unverändert — durch den
  Host-Repro-Regressionsfall bestätigt (identische Länge/Inhalt mit und ohne Fix).
- Einzige sichtbare Verhaltensänderung im Grenzfall: bei genau 22 Zeichen langen Namen wird
  jetzt das volle 22. Zeichen erhalten (vorher durch die zu kleine `MAG_DEV_NAME_MAX_LEN`-Länge
  in `mag_device_set_name()`s Aufrufpfad indirekt auf 21 nutzbare Zeichen begrenzt) — eine
  Verbesserung, keine Regression.
- Der interaktive `mag_file_select()`-Pfad war nach eigener Nachrechnung bereits unkritisch
  (siehe „Was"); der Fix macht die Terminierungsgarantie aber unabhängig vom Aufrufpfad, statt
  sich auf einen zufälligen Nebeneffekt zu verlassen — relevant, falls `mag_device_load_data()`
  künftig von weiteren Stellen aus aufgerufen wird.
- Kein zusätzliches Sicherheits-/Missbrauchsrisiko — der Fix schließt ausschließlich eine
  bestehende Lese-Lücke, eröffnet keine neue Funktion.

## Quelle
- `zacharyweiss/magspoof_flipper` (main, `9e782404`): `mag_device.c`
  (`mag_device_load_data`, `mag_device_set_name`, `mag_device_alloc`, `mag_device_delete`),
  `mag_device.h` (`MagDevice`, `MAG_DEV_NAME_MAX_LEN`), `mag.c` (App-Start-Ladepfad mit
  `mag->args`), `scenes/mag_scene_emulate.c`, `scenes/mag_scene_delete_confirm.c`,
  `scenes/mag_scene_saved_info.c`.
- `lib/toolbox/path.c` (`path_extract_filename`) aus meinem eigenen `mntm-012`-Firmwarebaum
  (Tag-Commit `e1784e74`, unverändert gegen den bisherigen Quellenstand) — bestätigt, dass die
  Endung vor der Namensübernahme abgeschnitten wird und der Dateiname selbst nicht künstlich
  begrenzt ist.
- `strlcpy()`-Referenzverhalten: OpenBSD-Originalimplementierung (`strlcpy.c`), wie sie in der
  newlib-C-Bibliothek meiner `arm-none-eabi-gcc`-Toolchain enthalten ist (Bounded-Copy-Schleife
  gefolgt von einem unbeschränkten Terminierungs-Scan bei Kappung — genau der Mechanismus, der
  bei einer echten Selbst-Kopie synchron bleibt, bei getrennten Quell-/Zielpuffern mit fehlender
  Terminierung dagegen über den Puffer hinausliest).
