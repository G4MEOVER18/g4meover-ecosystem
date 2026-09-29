# Archive-Browser: Cursor-Reset wird bei Ordnern mit mehr als 220 Einträgen übersprungen

## Was
Ich habe den `dev`-Branch von `Next-Flip/Momentum-Firmware` (frischer Klon, `git fetch --depth=200
origin dev`) nach dem letzten geprüften Stand (`Loading`-Widget, Fortsetzung 7) weiter auf kleine,
seither dazugekommene Fixes durchgesehen und bin auf Commit `757cca02` gestoßen: „Archive: Fix
wrong cursor when opening dirs with more than 220 files" (@WillyJL). Betroffen ist
`applications/main/archive/helpers/archive_browser.c::archive_list_item_cb()` — dieselbe Datei ist
in meiner eigenen Basis (Tag `mntm-012`, gegen frischen Klon verifiziert) unverändert vorhanden,
der Fix fehlt mir also tatsächlich.

Wenn der Datei-Browser-Worker das Ende einer neu geladenen Ordnerliste meldet (`is_last == true`),
läuft folgender Block:

```c
// mntm-012 (fehlerhaft):
if(model->item_cnt <= BROWSER_SORT_THRESHOLD) {   // BROWSER_SORT_THRESHOLD == 220
    FuriString* selected = NULL;
    if(model->item_idx >= 0) {
        selected = furi_string_alloc_set(files_array_get(model->files, model->item_idx)->path);
    }
    files_array_sort(model->files);
    if(selected != NULL) {
        for(uint32_t i = 0; i < model->item_cnt; i++) {
            if(!furi_string_cmp(files_array_get(model->files, i)->path, selected)) {
                model->item_idx = i;
                break;
            }
        }
    }
    if(model->item_idx < 0) {          // <-- Reset liegt INNERHALB des Schwellenwert-Blocks
        model->item_idx = 0;
    }
}
```

`BROWSER_SORT_THRESHOLD` ist mit `220` in `applications/services/gui/modules/file_browser_worker.h`
definiert (bei mir unverändert, gegen frischen `mntm-012`-Klon abgeglichen) — Ordner mit mehr
Einträgen werden aus Performancegründen nicht sortiert. `model->item_idx` wird von
`archive_folder_open_cb()` aus dem `file_idx`-Parameter des Workers übernommen; dieser ist laut
`file_browser_worker.c::browser_folder_init()` standardmäßig `-1` und wird nur dann auf einen
gültigen Index gesetzt, wenn beim Scannen ein zuvor übergebener Ziel-Dateiname tatsächlich
gefunden wird — beim einfachen Hineinnavigieren in einen Ordner ohne Ziel-Datei (der Normalfall)
bleibt er `-1`.

Weil der komplette `if(model->item_cnt <= BROWSER_SORT_THRESHOLD)`-Block bei Ordnern mit **mehr
als 220 Einträgen** vollständig übersprungen wird, wird in diesem Fall auch der
`item_idx < 0`-Reset nie ausgeführt — `model->item_idx` bleibt bei `-1`, obwohl der gesamte übrige
Code in dieser Datei (Navigations-Arithmetik in den Tasten-Handlern der zugehörigen
`archive_browser_view.c`, der Cursor-Hervorhebungsvergleich `model->item_idx == idx` in
`draw_list()`, die Scrollbar-Positionsberechnung `elements_scrollbar_pos(..., model->item_idx,
...)`) einen nicht-negativen Index voraussetzt. Konkret nachvollzogen: In `draw_list()`
(`archive_browser_view.c`) läuft `idx` je sichtbarer Zeile über `CLAMP((uint32_t)(i +
model->list_offset), model->item_cnt, 0u)` und ist damit stets `>= 0`; der Vergleich
`model->item_idx == idx` trifft mit `item_idx == -1` auf keine der sichtbaren Zeilen zu — die
hervorgehobene Cursor-Zeile wird beim Öffnen eines solchen Ordners also gar nicht gezeichnet.
Bei der ersten Navigationstaste (`archive_browser_view.c`, Scroll-Handler) rechnet der
Wraparound-Ausdruck `((model->item_idx - scroll_speed) + model->item_cnt) % model->item_cnt` mit
dem unerwarteten Wert `-1` statt `0` weiter und springt dadurch auf eine vom Nutzer nicht erwartete
Position tief in der Liste statt an den Anfang — exakt der vom Autor beschriebene „wrong cursor".

Gegen meine eigene `mntm-012`-Basis (frischer Klon, Tag `mntm-012`) verifiziert: Die Datei ist an
dieser Stelle byte-identisch zum hier zitierten Vorzustand, der Fund/Fix ist also unverändert
anwendbar.

## Nutzen
- Behebt einen bei mir bestätigt vorhandenen, vom Momentum-Maintainer selbst gefundenen und
  gefixten UI-Bug: Öffne ich (oder navigiere per SD-Karten-Browser/CLI/App-Loader) einen Ordner
  mit mehr als 220 Einträgen ohne vorherige Ziel-Datei — z. B. einen gut gefüllten `SubGHz`-,
  `NFC`- oder `apps`-Ordner —, fehlt die Cursor-Hervorhebung beim Öffnen, und die erste
  Navigationstaste springt an eine unerwartete Stelle statt an den Anfang der Liste.
- Für meinen Pentest-Alltag relevant, weil ich Zielordner mit vielen gesammelten Captures/Dumps
  (SubGHz-Sessions, NFC-Dumps) regelmäßig genau in dieser Größenordnung ansammle — der Bug trifft
  also nicht nur einen Exotenfall, sondern meinen eigenen Archiv-Workflow.
- Fix ist winzig (reine Umklammerung einer bestehenden Bedingung, kein neuer Code, keine neue
  Logik) und rein additiv im Sinne von „kein bisher funktionierender Fall wird enger" — siehe
  Risiko.

## Betroffene Dateien (lokal)
- `applications/main/archive/helpers/archive_browser.c` —
  `archive_list_item_cb()`: das bestehende `if(model->item_idx < 0) { model->item_idx = 0; }` aus
  dem `if(model->item_cnt <= BROWSER_SORT_THRESHOLD) { ... }`-Block herausziehen und direkt danach
  (auf gleicher Ebene, vor `if(archive_is_file_list_load_required(model))`) erneut platzieren —
  siehe Patch. Keine andere Datei betroffen, kein neuer Include, keine neue Konstante.

## Risiko
- Sehr niedrig: Für Ordner mit `item_cnt <= BROWSER_SORT_THRESHOLD` (220) ändert sich nichts — der
  Reset lief dort schon vorher unbedingt am Ende desselben Blocks, jetzt läuft er unbedingt direkt
  danach; beobachtbares Verhalten identisch (durch Host-Repro exhaustiv bestätigt, siehe unten).
- Für Ordner mit mehr als 220 Einträgen und bereits nicht-negativem `item_idx` (z. B. weil eine
  Ziel-Datei gefunden wurde) ändert sich ebenfalls nichts — der neue Reset greift nur bei
  `item_idx < 0`.
- Einzige Verhaltensänderung: Ordner mit mehr als 220 Einträgen und `item_idx == -1` (keine
  Ziel-Datei) bekommen jetzt ebenfalls den Reset auf `0` — exakt der beabsichtigte Fix, keine neue
  Nebenwirkung ersichtlich, da `0` bereits der reguläre Startwert für den Cursor in jedem anderen
  Codepfad dieser Datei ist (siehe z. B. Zeile mit `model->item_idx = 0;` im Root-Reset weiter
  unten in derselben Datei).
- Host-Repro deckt ausschließlich die reine Kontrollfluss-/Grenzwert-Arithmetik ab (wann läuft der
  Reset, mit welchem Ergebnis), nicht die tatsächliche Bildschirmausgabe oder die
  Datei-Browser-Worker-Integration (Sortier-/Pfadsuch-Schleife wird bewusst ausgelassen, siehe
  Repro-Kommentar — sie ist für den Fund irrelevant, da sie bei `item_idx < 0` nichts verändert).
  Der eigentliche visuelle Effekt (fehlende Cursor-Hervorhebung, Sprung bei erster Navigation)
  lässt sich nur an einem echten, > 220 Einträge großen SD-Karten-Ordner auf dem Gerät bestätigen.

## Port-Skizze
1. `applications/main/archive/helpers/archive_browser.c::archive_list_item_cb()` öffnen, den
   `if(model->item_cnt <= BROWSER_SORT_THRESHOLD) { ... }`-Block in `with_view_model(...)`
   aufsuchen.
2. Patch anwenden (siehe `archive-oversized-folder-cursor-reset-skip-fix/archive_browser_c.patch`,
   `git apply --check` vorab gegen frische Kopie meiner eigenen `G4MEOVER18/G4MEOVER-FW`-Basis
   verifizieren) — verschiebt ausschließlich die schließende Klammer des
   Schwellenwert-`if`-Blocks nach oben (vor den Reset) und lässt den Reset danach unbedingt laufen.
3. Build: `ufbt` (bzw. `fbt firmware_all`) — reine Kontrollfluss-Umklammerung, kein
   Signatur-/API-Wechsel, sollte ohne Fehler durchlaufen.
4. Funktionstest lokal: Auf der SD-Karte einen Ordner mit mehr als 220 Dateien anlegen (z. B. per
   Skript leere `.txt`-Platzhalter erzeugen, keine echten Capture-Dateien nötig), im Archiv/
   Datei-Browser hineinnavigieren (ohne vorher eine bestimmte Datei ausgewählt zu haben) — vorher:
   keine sichtbare Cursor-Hervorhebung beim Öffnen, erste Navigationstaste springt an eine
   unerwartete Position; nachher: Cursor steht sofort sichtbar auf dem ersten Eintrag, Navigation
   verhält sich ab dem ersten Tastendruck wie erwartet.

## Quelle
- `Next-Flip/Momentum-Firmware`, `dev`-Branch, Commit `757cca0279bf2cbb711c56d69e8c86b2b17662d2`
  (@WillyJL, 2026-08-18): „Archive: Fix wrong cursor when opening dirs with more than 220 files" —
  `applications/main/archive/helpers/archive_browser.c`.
- Momentum-Firmware, Tag `mntm-012`: dieselbe Datei als Vergleichsbasis (Fix dort noch nicht
  vorhanden, Bug bestätigt reproduzierbar vorhanden).
- `applications/services/gui/modules/file_browser_worker.h` (Tag `mntm-012`): Definition von
  `BROWSER_SORT_THRESHOLD` (`220`), unverändert gegenüber `dev`.
