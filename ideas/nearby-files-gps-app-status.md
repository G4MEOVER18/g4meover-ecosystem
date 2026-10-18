# Nearby Files (Stichoza) — GPS-Datei-Browser (SubGHz/NFC/RFID/iButton)

## Quelle

`github.com/Stichoza/flipper-nearby-files`, aktueller Stand (Commit `669a8c7`, `fap_version="1.7"`).
Beide Quelldateien (`nearby_files.c`, `gps_reader.c` samt Header) vollständig gelesen, dazu alle
sechs Szenen (`scenes/`) überflogen. Kein neu zu findender Port-Kandidat: die App liegt bereits
**in meinem eigenen `mntm-012`-Firmware-Baum**, über das `applications/external`-Submodul
(`Next-Flip/Momentum-Apps`, in meinem FW-Stand auf Commit `85b0cdb4e7dec08d16db6ceea3ec3fc5078bedb4`
gepinnt) unter `applications/external/nearby_files/`. Per `curl` gegen genau diesen gepinnten
Commit (`raw.githubusercontent.com/Next-Flip/Momentum-Apps/85b0cdb.../nearby_files/`) verifiziert:
mein Stand führt `fap_version="1.5"` — zwei Versionsstufen hinter dem aktuellen Upstream-`v1.7`.
Diff gegen die Upstream-Quelle zeigt den Versionsunterschied: meine `v1.5` hat kein `iButton`-
Scannen, keinen persistenten Baudraten-Konfigurationsspeicher (`saved_struct`) und keine
"Add GPS"-Schreibfunktion (`nearby_files_add_gps_to_file()`/`nearby_files_update_coordinates_in_file()`)
— alles erst in `v1.6`/`v1.7` hinzugekommen. Der unten beschriebene Fund betrifft eine Funktion, die
in beiden Versionen **identisch** vorhanden ist.

## Was

GPS-gestützter Datei-Browser: liest Position über ein per GPIO/UART angeschlossenes NMEA-GPS-Modul
(`gps_reader.c`, eigener Worker-Thread, `minmea`-Parser-Bibliothek), durchsucht `/ext/subghz`,
`/ext/nfc`, `/ext/lfrfid` (und ab `v1.6` zusätzlich `/ext/ibutton`) rekursiv nach `.sub`/`.nfc`/
`.rfid`/`.ibtn`-Dateien, liest aus jeder Datei die `Lat:`/`Lon:`-Textzeilen (von "SubGHz Wardriving"
beim Capture automatisch eingetragen, bzw. ab `v1.6` auch manuell über die App selbst nachtragbar),
berechnet die Haversine-Distanz zur aktuellen Position und zeigt die Dateien nach Entfernung
sortiert an (`[45m]`, `[1.3km]`, `[23km]`). Klick auf einen Eintrag startet die passende App
(Sub-GHz/NFC/125-kHz-RFID/iButton) direkt mit der Datei.

## Nutzen

Füllt die bislang dünnste Katalog-Kategorie "Recon / GPS / Wardriving" (bisher nur `FlipDeFlock`
und reines `GPS (NMEA) + WigleCSV`-Logging) um die Auswertungsseite: nicht nur Positionen loggen,
sondern eigene SubGHz-/NFC-/RFID-/iButton-Captures nach Entfernung zur aktuellen Position
durchsuchen — nützlich um bei einer Rückkehr an einen zuvor besuchten, autorisierten Testort gezielt
die dort bereits erfassten Signale/Karten wiederzufinden, ohne den gesamten SD-Karten-Bestand von
Hand durchzublättern.

## Aufwand

Keiner (Firmware-seitig) — die App ist bereits Teil meines `mntm-012`-Baums, kein eigener
Port/Build nötig. Für den unten beschriebenen Fix genügt ein additiver Patch auf die vorhandene
Datei. Hardware: ein externes NMEA-GPS-Modul am Erweiterungsport (Verkabelung identisch zu
`ezod/flipperzero-gps`, in der README selbst referenziert).

## API-87-Status

✅ Bereits vorhanden und gepinnt, kein Kompatibilitätsrisiko (die Firmware baut die App bereits
gegen meinen eigenen `mntm-012`-Stand). Einziger offener Punkt ist die Versionslücke `v1.5` → `v1.7`
oben — reine Feature-Nachrüstfrage für eine künftige Sitzung, keine Blockade.

## Fund: `nearby_files_process_gps_coordinates()` verwirft echte Treffer auf Äquator/Nullmeridian

In `nearby_files.c` (meine `v1.5`, Zeile 487 — identisch in Upstream-`v1.7`, dort Zeile 728)
entscheidet genau diese Zeile, ob eine aus einer Datei gelesene Koordinate als "hat GPS-Daten"
gilt:

```c
// Check if coordinates are valid (not zero)
if((float)latitude != 0.0f && (float)longitude != 0.0f) {
    // ... Datei bleibt in der Liste, Distanz wird berechnet ...
} else {
    // File has zero coordinates, treat as invalid
    furi_string_free(item->path);
    furi_string_free(item->name);
}
```

Die Absicht ist klar: eine Datei, deren `Lat:`/`Lon:`-Felder beide `0.000000` sind (typischer
"kein Fix"-Sentinel-Wert, falls eine App versehentlich ungültige Koordinaten einträgt), soll nicht
fälschlich als "100 % hier direkt daneben" in der Liste erscheinen. Die Bedingung prüft das aber
mit **`&&`** statt mit **`||`** — das deckt nicht "beide Achsen sind null", sondern verlangt "beide
Achsen sind ungleich null". Eine Datei, deren Koordinate auf **einer einzigen** Achse exakt `0.0`
ist — Breitengrad `0.0` (Äquator) oder Längengrad `0.0` (Nullmeridian, Greenwich) —, hat eine
vollkommen reale, gültige GPS-Position, wird aber von dieser Bedingung ebenso verworfen wie der
echte `(0,0)`-Sentinel: `item->path`/`item->name` werden freigegeben, die Datei verschwindet
stillschweigend aus der "Nearby Files"-Liste, obwohl sie gültige Koordinaten enthält.

Kein konstruierter Randfall: "SubGHz Wardriving" trägt die Koordinate bei jedem Capture automatisch
ein, unabhängig davon, wo genau sich der Nutzer befindet — jeder autorisierte Test exakt auf dem
Nullmeridian (durch Greenwich/London oder weite Teile Spaniens, Frankreichs, Algeriens, Ghanas
verlaufend) oder auf dem Äquator trifft diese Bedingung mit der jeweils anderen Achse ungleich
null. Die betroffene Datei wird dann ohne jede Fehlermeldung einfach nicht gelistet — der Nutzer
sieht nur eine kürzere Liste, nicht, dass eine Datei fehlt.

## Fix (additiv, ein Operator)

`&&` durch `||` ersetzen — damit verwirft die Bedingung nur noch den echten `(0,0)`-Sentinel, jede
Koordinate mit mindestens einer Achse ungleich null bleibt erhalten:

```c
if((float)latitude != 0.0f || (float)longitude != 0.0f) {
```

Patch in `ideas/nearby-files-zero-coordinate-filter-fix/nearby_files_c.patch` (gegen die exakte
`v1.5`-Quelle, wie sie über das `applications/external`-Submodul in meinem eigenen `mntm-012`-Baum
liegt). Gegen zwei frische, unabhängige Zweitkopien (direkt vom gepinnten Momentum-Apps-Commit
`85b0cdb4e7dec08d16db6ceea3ec3fc5078bedb4` geladen) mit `git apply --check` verifiziert, real
angewendet, beide Endstände per `md5sum` byte-für-byte identisch zueinander und zum erwarteten
Ergebnis. Derselbe Fix gilt unverändert für Upstream-`v1.7`, dort an der entsprechenden Zeile 728.

## Testhinweis (lokal nachvollzogen)

Host-Repro (`ideas/repro-nearby-files-zero-coordinate-filter/repro.c`) bildet ausschließlich die
Filterbedingung selbst nach (ALT = `&&`, NEU = `||`), kein Hardware-/Storage-Zugriff nötig. Acht
Prüfgruppen unter
`gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined` bei
`-O0`/`-O1`:

1. Zwei Regressionsfälle (gewöhnliche Koordinaten, beide Achsen ungleich null) — ALT und NEU
   stimmen überein (Datei bleibt).
2. Echter `(0,0)`-Sentinel — ALT und NEU stimmen überein (Datei wird verworfen).
3. Fund: Nullmeridian (Breite 51.5074, Länge exakt 0.0, London) — ALT verwirft fälschlich, NEU
   behält die Datei.
4. Fund, andere Achse: Äquator (Breite exakt 0.0, Länge -78.5) — ALT verwirft fälschlich, NEU
   behält die Datei.
5. Nahe, aber nicht exakt null (0.000001/0.000001) — beide Varianten verhalten sich wie bei
   gewöhnlichen Koordinaten, keine Nebenwirkung des Fixes.
6. Explizite Abweichungs-/Übereinstimmungsprüfung: ALT und NEU weichen **nur** in den beiden
   Fund-Fällen voneinander ab, stimmen in jedem anderen Fall überein.
7. Äquivalenz-Sweep über ein 1521-Punkte-Gitter (alle Breiten -90.0..90.0, Längen -180.0..180.0 in
   0.1°-Schritten, Null auf beiden Achsen ausgespart): ALT und NEU liefern überall identisch
   "behalten", sobald keine Achse exakt null ist.
8. Gegenprobe: ein absichtlich wieder auf `&&` zurückgesetztes "NEU" reproduziert exakt denselben
   Fund wie ALT in den beiden Einzelachsen-Null-Fällen — bestätigt, dass die Prüfungen 3/4 wirklich
   den `&&`/`||`-Unterschied testen und nicht zufällig durchlaufen.

Ergebnis beider Durchläufe (`-O0`/`-O1`): **3352 Prüfungen bestanden**, identischer Output beider
Optimierungsstufen, keine Compiler-Warnungen, keine Sanitizer-Befunde.

Lokal zu tun: Patch gegen `applications/external/nearby_files/nearby_files.c` im eigenen
Firmware-Baum anwenden, mit `fbt`/`ufbt` bauen und flashen. Funktionstest: eine `.sub`-Datei auf der
SD-Karte von Hand mit `Lat: 51.507400` und `Lon: 0.000000` versehen (oder `Lat: 0.000000` und einem
beliebigen Längengrad ungleich null), App mit gültigem GPS-Fix starten — vor dem Fix fehlt die Datei
in der Liste, nach dem Fix erscheint sie mit korrekter Entfernungsangabe. Eine Datei mit echten
`Lat: 0.000000`/`Lon: 0.000000` bleibt vor und nach dem Fix unverändert ausgeschlossen.

## Sicherheitsrahmen

Rein lesende Auswertung eigener, bereits erfasster Captures (SubGHz/NFC/RFID/iButton) nach
Entfernung — kein Sende-/Angriffsverhalten, passt unverändert in den defensiven Rahmen aus
`CONSTRAINTS.md` Punkt 5. Der Fund ist ein reiner Anzeigefehler (fälschlich fehlende Einträge),
kein Ziel-Sicherheitsrisiko.

## Nächster Schritt

Versionslücke `v1.5` → `v1.7` als eigenen, separaten Rückstellpunkt vormerken: das Nachziehen
würde über das `applications/external`-Submodul-Pin in `G4MEOVER-FW` laufen (Commit-Bump auf einen
Momentum-Apps-Stand mit `v1.7`), nicht über einen eigenen App-Patch — eigene Sitzung wert, sobald
Firmware-Feature-Mining wieder an der Reihe ist, inklusive Prüfung der drei neuen `v1.6`/`v1.7`-
Funktionen (`iButton`-Scan, Baudraten-Persistenz, "Add GPS"-Schreibpfad) auf eigene Fehler.
