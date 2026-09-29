# Loading-Widget: Label + Progress-Bar (Port aus Unleashed)

## Was
Ich habe das Firmware-Feature-Mining-Fenster gegen Unleashed-Firmware (`DarkFlippers/unleashed-firmware`,
`dev`) fortgesetzt (letzter geminter Kandidat: `subghz_txrx.c`-Dangling-Pointer aus PR #1104,
siehe `subghz-tx-transmitter-dangling-pointer-cleanup.md`) und dabei das generische
`Loading`-GUI-Widget (`applications/services/gui/modules/loading.c`/`.h`) geprüft. Bei mir
(mntm-012) zeigt `Loading` ausschließlich die zentrierte Spinner-Animation — Unleashed hat das
Widget seither um zwei zusammengehörige, additive Fähigkeiten erweitert:

- Commit `69c03681` ("show loading progress when unpacking assets from apps", 10.09.2026, von
  `xMasterX`): neue Funktionen `loading_set_progress(Loading*, float)` und
  `loading_reset_progress(Loading*)` — eine optionale Fortschrittsleiste unter der
  Spinner-Animation, gespeist über einen `float`-Anteil (0.0–1.0), inkl. Redraw-Unterdrückung bei
  wiederholt demselben Wert.
- Commit `9e4951074` ("[GUI]: Let the Loading view show a label next to the spinner", PR #1169,
  23.09.2026, von `Mykhailo Shevchuk`, API 88.13, `Closes #1168`): neue Funktion
  `loading_set_text(Loading*, const char*)` — ein optionales Label rechts neben dem (dann nach
  links gerückten) Spinner; ist eine Progress-Bar zusätzlich aktiv, sitzt sie unter dem Label
  statt unter dem Spinner. Laut Commit-Nachricht ersetzt das bei Unleashed eine eigene, private
  Kopie derselben "Spinner + Label + Bar"-Anzeige, die zuvor nur in der NFC-App
  (`views/loading_label`) existierte — mit dem Widget-Umbau kann jede App dieselbe Anzeige nutzen.

Beide Commits ändern ausschließlich `loading.c`/`.h`; kein anderer Aufrufer im Baum wurde von
Unleashed in diesen beiden Commits angepasst (die tatsächliche Verdrahtung — `loader.c` für die
App-Entpack-Fortschrittsanzeige, `nfc_app.c` für das UID-Dictionary-Label — kam in späteren,
eigenen Commits, siehe "Nicht mitübernommen" unten). Bei mir sind `loading.c`/`.h` byte-identisch
zum unveränderten mntm-012-Basisstand (per `diff` gegen einen frischen, unauthentifizierten
`git clone --depth 1 --branch mntm-012 Next-Flip/Momentum-Firmware` sowie gegen meine eigene
`G4MEOVER18/G4MEOVER-FW`-Basis bestätigt — beide identisch, kein lokaler Vorbestand zu diesem
Fund).

## Nutzen
- Jede eigene oder portierte App, die aktuell nur den nackten Spinner zeigt, kann künftig ohne
  weiteren Infrastruktur-Aufwand sagen, *was* gerade passiert (`loading_set_text()`, z. B. "Reading
  UID", "Unpacking …") und/oder *wie weit* (`loading_set_progress()`), statt den Nutzer bei
  längeren Operationen (großes Dictionary, viele Dateien, langsames Protokoll) im Unklaren zu
  lassen, ob das Gerät noch lebt.
- Rein additiv und mit eigener Redraw-Unterdrückung: die drei neuen Funktionen sind reine
  Opt-in-API — kein bestehender Aufrufer im Baum (`archive.c`, `bad_usb_app.c`, `ibutton.c`,
  `infrared_app.c`, `nfc_app.c`, `loader.c`, `loader_menu.c`, `js_app.c` — per Grep über den
  gesamten Aufrufstellenbestand bestätigt) ruft sie auf, und `loading_draw_callback()` nimmt für
  den Default-Zustand (`text` leer, `progress_shown == false`) exakt denselben Berechnungspfad wie
  vorher (siehe Host-Repro unten) — keine Verhaltensänderung für bestehenden Code.
- Ebnet den Weg für spätere eigene Ports, die dieselbe Fortschrittsanzeige brauchen (z. B. ein
  künftiger Port des in derselben Unleashed-Session eingeführten "NFC: Show progress while a large
  per-UID (CUID) dictionary loads on Read"-Features, laut Changelog PR #1167) — ohne die
  eigentliche Verdrahtung an dieser Stelle vorwegzunehmen.

## Betroffene Dateien (lokal)
Unified-Diffs beiliegend:
`firmware-mining/loading-widget-label-and-progress-bar/{loading_c,loading_h}.patch`.

1. `applications/services/gui/modules/loading.c`:
   - `LoadingModel` um `FuriString* text`, `bool progress_shown`, `float progress` erweitert.
   - Neue Konstanten (`LOADING_ICON_SIZE`, `LOADING_BAR_WIDTH/_HEIGHT/_GAP/_BLOCK`,
     `LOADING_LABEL_*`) für die Geometrie.
   - Neue statische Funktion `loading_draw_label()` für den Label-Zweig.
   - `loading_draw_callback()`: verzweigt bei nicht-leerem `text` in `loading_draw_label()`
     (`return` danach), sonst unverändert der alte Spinner-Zweig, jetzt zusätzlich mit optionaler
     Progress-Bar darunter, wenn `progress_shown` gesetzt ist.
   - `loading_alloc()`/`loading_free()`: `text` wird mit-alloziert/-freigegeben,
     `progress_shown`/`progress` initialisiert.
   - Drei neue öffentliche Funktionen: `loading_set_progress()`, `loading_reset_progress()`,
     `loading_set_text()`.
2. `applications/services/gui/modules/loading.h`: Deklarationen plus Doc-Kommentare der drei
   neuen Funktionen.

Keine andere Datei betroffen — insbesondere keine bestehende Aufrufstelle geändert.

## Risiko
- **Host-seitig verifiziert, nicht auf echtem Display:** Das Repro (siehe unten) prüft die reine
  Arithmetik/Zustandslogik (Icon-Position im Default-Fall, Zweig-Auswahl, Clamp- und
  Redraw-Unterdrückungslogik der drei Setter) 1:1 gegen den echten Funktionskörper — nicht die
  tatsächliche Pixelausgabe von `canvas_draw_icon()`/`elements_progress_bar()`/
  `elements_multiline_text_aligned()`, die den echten GUI-/Canvas-Stack brauchen. Das optische
  Layout (Label-Zeilenumbruch bei mehr als "zwei Zeilen à ca. 15 Zeichen", wie im Header-Kommentar
  dokumentiert) ist daher nur durch Quelltext-Lesen, nicht durch dieses Repro abgedeckt — vor
  echter Nutzung mit einem konkreten Label-Text am Gerät gegenprüfen.
- API-Abhängigkeiten geprüft: `elements_progress_bar()` (bereits in `elements.h` vorhanden),
  `furi_string_equal_str()`/`furi_string_empty()`/`furi_string_set()` sowie das `CLAMP()`-Makro
  (`furi/core/core_defines.h`) sind bei mir (API 87.1) bereits vorhanden — keine neue
  API-Abhängigkeit, kein Symbol-Fehlbestand zu erwarten.
- Rein additiv, keine bestehende Struct-Größe für andere Verbraucher relevant (`LoadingModel` ist
  privat zu `loading.c`, `Loading` bleibt ein opaker Zeiger).
- Nicht mitübernommen (bewusst): die eigentliche Verdrahtung in `loader.c`
  (`loading_set_progress()`/`loading_reset_progress()` beim App-Entpacken) und in `nfc_app.c`
  (`loading_set_text()` für die CUID-Dictionary-Anzeige) — das sind jeweils eigenständige,
  größere Änderungen an aktiv produktivem Code (Entpack-Fortschritt bzw. NFC-Dictionary-Ablauf),
  kein "klein & sicher"-Fund mehr. Port-Skizze für eine spätere Sitzung:
  - `applications/services/loader/loader.c`: `loading_set_progress(loader->loading, (float)done /
    (float)total)` beim Entpacken je Datei, `loading_reset_progress(loader->loading)` danach.
  - `applications/main/nfc/nfc_app.c`: zweite `Loading`-Instanz mit `loading_set_text(...)` für
    das CUID-Dictionary-Label, dabei laut Unleashed-Commit-Nachricht bewusst eine *separate*
    Instanz, damit Label/Bar nie auf dem einfachen Spinner-Bildschirm erscheinen.

## Host-Repro
`firmware-mining/repro-loading-widget-label-and-progress-bar/repro.c` — reine Arithmetik-/
Zustandslogik, 1:1 aus dem echten Funktionskörper transkribiert (Canvas-/FuriString-Aufrufe durch
die äquivalente Byte-/Zeichenkettensemantik ersetzt, siehe Kommentare im Repro):

1. **Default-Icon-Position ALT vs. NEU** über den vollen 256×256-Canvas-Größenraum: bei
   `progress_shown == false` liefert `neu_default_xy()` für jede getestete Kombination exakt
   dasselbe `(x, y)` wie die pristine `alt_default_xy()` — 65536 Prüfungen.
2. **Zweig-Trennung**: leerer Text nimmt nie den neuen Label-Zweig, jeder nicht-leere Text immer.
3. **`loading_set_progress()`-Clamp**: Rand- und Ausreißerwerte (negativ, 0, 0.3333, 1, >1, sehr
   groß) landen korrekt in `[0.0, 1.0]`.
4. **Redraw-Unterdrückung** von `loading_set_progress()`/`loading_reset_progress()`: erster Aufruf
   meldet `changed`, ein wiederholter identischer Wert nicht, ein neuer Wert wieder, Reset einer
   sichtbaren Bar meldet `changed`, Reset einer bereits verborgenen nicht.
5. **`loading_set_text()`-Änderungserkennung**: identischer Text kein `changed`, anderer Text
   schon, `NULL` wird wie `""` behandelt (inkl. Sonderfall "erstmals auf `NULL`/`""` gesetzt, wenn
   vorher ein echter Text stand").

Unter ASan/UBSan bei `-O0` und `-O1` ausgeführt: **ALLE CHECKS BESTANDEN** (65560 Einzelprüfungen),
keine Compiler-Warnungen (`-Wall -Wextra`).

Patch-Verifikation: `git apply --check` vorab gegen eine frische Kopie meiner eigenen
`G4MEOVER18/G4MEOVER-FW`-Basis (byte-identisch zum mntm-012-Ausgangsstand) für beide Patches
bestanden, real angewendet und das Ergebnis byte-für-byte (`diff`) gegen den tatsächlichen
Unleashed-`dev`-Stand abgeglichen — identisch.

## Quelle
- `DarkFlippers/unleashed-firmware`, Branch `dev`, `applications/services/gui/modules/loading.c`/
  `.h`, Commits `69c036819ef2b8415ac3c579104b039e43f91ec6` ("show loading progress when unpacking
  assets from apps") und `9e4951074bdc93d0c1acb93e3af3ac21e007ad83` ("[GUI]: Let the Loading view
  show a label next to the spinner (#1169)", `Closes #1168`) — per authentifizierungsfreiem
  `git fetch --depth=400 origin dev` bezogen, History-Suche (`git log -- <Pfad>`) bestätigt: keine
  weitere Änderung an diesen beiden Dateien zwischen `9e4951074` und dem aktuellen `dev`-Head
  (`15bca58e`, Changelog-Update).
- Aufrufstellen-Suche der neuen Funktionen im aktuellen `dev`-Stand (`loader.c`, `nfc_app.c`) rein
  informativ für den Port-Skizze-Abschnitt, nicht mitübernommen.
- Meine eigene `G4MEOVER18/G4MEOVER-FW`-Basis, `main`-Branch, sowie ein frischer,
  unauthentifizierter `git clone --depth 1 --branch mntm-012 Next-Flip/Momentum-Firmware` zum
  Abgleich des Ausgangsstands (beide byte-identisch zueinander und zum in den Patches
  vorausgesetzten Kontext).
