# lora-ukfe — Settings-Scene über neuen Menüpunkt erreichbar gemacht

## Was
Ich habe den in `scene-settings-back-navigation-fix.md` bewusst offen gelassenen
Nebenbefund aufgegriffen: `UkfeSceneSettings` ist vollständig implementiert und im
Scene-Handler-Array verdrahtet, aber ohne Menüeintrag, der dorthin führt — der
damalige Back-Navigations-Fix in `scene_settings.c` blieb dadurch bis jetzt ohne im
Betrieb sichtbare Wirkung.

`scenes/scene_menu.c` baut auf der Kategorie-Ebene (`s_cat < 0`) ein Submenü aus den
acht `CATALOG`-Einträgen (`command_catalog.h`) plus einem festen `"Log"`-Eintrag
(`IDX_LOG = 200`). Ein Eintrag zu `UkfeSceneSettings` fehlte dort komplett — bestätigt
durch `grep -rn scene_manager_next_scene` über den gesamten Baum: genau drei
Aufrufstellen (Programmstart → `UkfeSceneMenu`, zweimal → `UkfeSceneLog` aus
`scene_menu.c`), keine zu `UkfeSceneSettings`.

Fix: ein neuer Eintrag `"Einstellungen"` auf der Kategorie-Ebene, mit eigener Konstante
`IDX_SETTINGS = 202` (liegt oberhalb von `CATALOG_COUNT = 8`, kollidiert also nicht mit
den Kategorie-Indizes 0–7), der `menu_callback()` direkt zu `UkfeSceneSettings`
navigieren lässt — analog zum bestehenden `IDX_LOG`-Zweig.

Nebenbefund (kein eigener Fix hier): `scenes/scene_mode_menu.c` (`UkfeSceneModeMenu`)
ist ebenso unerreichbar und bietet exakt dieselbe Funktion wie der bereits vorhandene
`"Modus"`-Eintrag in `scene_settings.c` — beide setzen `app->mode` aus derselben
`UkfeMode`-Auswahl, nur über unterschiedliche Widgets (eigenes `Submenu` vs.
`VariableItemList`-Eintrag in Settings). Für `UkfeSceneModeMenu` ergänze ich hier
bewusst **keinen** Menüeintrag — es wäre reine Duplikation der jetzt erreichbaren
Settings-Funktion. Ob die Scene komplett entfernt wird oder für einen anderen Zweck
umgewidmet wird, ist eine eigene Entscheidung für eine künftige Sitzung.

## Nutzen
- Macht `UkfeSceneSettings` ("Modus"-Umschaltung, LoRa-Profil-Anzeige) zum ersten Mal
  über die App-GUI erreichbar, nicht nur über Code-Pfade, die nie aufgerufen werden.
- Macht den bereits dokumentierten, aber bislang folgenlosen Back-Navigations-Fix
  (`scene-settings-back-navigation-fix.md`) tatsächlich wirksam: sobald beide Patches
  zusammen angewendet sind, führt Back in der Settings-Ansicht wieder zurück zum Menü,
  statt in einer Sackgasse zu enden.
- Rein additiv: zwei neue Zeilen in `build_menu()`/`menu_callback()`, keine bestehende
  Kategorie-/Funktions-Navigation verändert.

## Betroffene Dateien
- `scenes/scene_menu.c` — neue Konstante `IDX_SETTINGS`, ein neuer
  `submenu_add_item()`-Aufruf in der Kategorie-Ebene von `build_menu()`, ein neuer
  Dispatch-Zweig in `menu_callback()`.

## Patch
`settings-scene-menu-entry-fix/scene_menu.patch` — gegen eine frische Zweitkopie von
`G4MEOVER18/lora-ukfe` (`0dba410a`) mit `git apply --check` UND `patch -p1` an zwei
unabhängigen frischen Kopien verifiziert, Ergebnis laut `md5sum` in beiden Fällen und
beiden Prüfmethoden byte-identisch.

```c
#define IDX_LOG      200
#define IDX_SETTINGS 202
#define IDX_BACK     201
...
        submenu_add_item(app->submenu, "Log", IDX_LOG, menu_callback, app);
        submenu_add_item(app->submenu, "Einstellungen", IDX_SETTINGS, menu_callback, app);
...
    if(index == IDX_LOG) { scene_manager_next_scene(app->scene_manager, UkfeSceneLog); return; }
    if(index == IDX_SETTINGS) { scene_manager_next_scene(app->scene_manager, UkfeSceneSettings); return; }
    if(index == IDX_BACK) { s_cat = -1; build_menu(app); return; }
```

## Host-Repro
Reine Dispatch-/Index-Logik (kein Hardware-, UART- oder GUI-Zugriff).
`menu_callback()` 1:1 nachgebaut (reale Konstanten `IDX_LOG`/`IDX_SETTINGS`/`IDX_BACK`
und reales `CATALOG_COUNT = 8` aus `command_catalog.h`, GUI-Aufrufe durch
aufgezeichnete Ziel-Events ersetzt):

```
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -O0 -o repro repro.c && ./repro
6/6 Pruefungen bestanden
ALLE CHECKS PASSED
```

Sechs Prüfungen: (1) ALT — `UkfeSceneSettings` bleibt unerreicht, selbst wenn
`IDX_SETTINGS` dispatcht würde, da kein Submenü-Eintrag dafür existiert; (2) NEU —
Auswahl von `"Einstellungen"` navigiert zu Settings, unabhängig vom
Kategorie-Cursor; (3) NEU — `"Log"` bleibt unverändert; (4) NEU — eine normale
Kategorie-Auswahl bohrt sich weiterhin unverändert durch; (5) NEU — `"< zurueck"`
setzt den Kategorie-Cursor weiterhin korrekt zurück; (6) Index-Kollisionsprüfung —
alle am Menü-Root vergebenen IDs (0–7 für Kategorien, 200/201/202) sind paarweise
verschieden. Bei `-O0` und `-O1` identischer Output, keine Compiler-Warnungen, keine
Sanitizer-Befunde — in `repro-settings-scene-menu-entry/repro.c`.

Funktionaler Test danach lokal auf dem Flipper: App starten, im Root-Menü den neuen
Eintrag `"Einstellungen"` auswählen (nach den acht Kategorien und `"Log"`), prüfen
dass die Settings-Ansicht mit `"Modus"`- und `"LoRa-Profil"`-Einträgen erscheint, Back
drücken — **setzt den bereits dokumentierten `scene-settings-back-navigation-fix`
voraus**, sonst bleibt Back dort wirkungslos stehen.

## Risiko
- Sehr niedrig: zwei ergänzte Zeilen plus eine neue Konstante, keine bestehende
  Kategorie-Navigation, kein bestehender Dispatch-Zweig verändert oder entfernt.
- `IDX_SETTINGS = 202` liegt weit oberhalb von `CATALOG_COUNT = 8` — kein
  Kollisionsrisiko mit künftig ergänzten Katalog-Kategorien, solange deren Zahl nicht
  über 200 wächst (bereits implizite Voraussetzung des bestehenden
  `IDX_LOG = 200`/`IDX_BACK = 201`-Schemas).
- Ohne den separat dokumentierten Back-Navigations-Fix in `scene_settings.c` führt
  dieser Patch allein zu einer neu erreichbaren, aber per Back nicht verlassbaren
  Ansicht — beide Patches gehören für den lokalen Einsatz zusammen appliziert.
- Kein Sicherheits-/Missbrauchsrisiko: reine GUI-Navigationslogik, keine Protokoll-,
  Speicher- oder Funkänderung.

## Testhinweis
Siehe Host-Repro oben (vollständig ohne Hardware verifizierbar). Für den vollen
funktionalen Test lokal beide Patches anwenden (`settings-scene-menu-entry-fix` +
`scene-settings-back-navigation-fix`), bauen, flashen, neuen Menüeintrag anwählen,
Back-Verhalten prüfen.

## Quelle
- `G4MEOVER18/lora-ukfe` (main, `0dba410a`): `scenes/scene_menu.c`
  (`build_menu`, `menu_callback`), `command_catalog.h` (`CATALOG`/`CATALOG_COUNT`,
  Gegenprobe für den Index-Kollisionsbereich), `scenes/scenes.h`/
  `scenes/scene_handlers.c` (Scene-Enum/Handler-Tabelle, bestätigt dass
  `UkfeSceneSettings` bereits vollständig verdrahtet war), `scenes/scene_mode_menu.c`
  (Nebenbefund: Funktionsduplikat zum jetzt erreichbaren `"Modus"`-Eintrag in
  `scene_settings.c`), eigener vorheriger Fund `scene-settings-back-navigation-fix.md`
  (Ausgangspunkt dieses Fixes).

## Offen
Weiterhin offen (unverändert aus vorherigen Sitzungen): `texkom` Phase 2-5, NFC/RFID-
Parser-Teilaspekt, MIFARE-Plus-/Ultralight-AES Phase 3 Rest, PSA-Bruteforce-Krypto-
Konstanten, `sanitize_filename()`-Zeichenbereichs-Nebenfund, neun verbleibende
FeliCa-Multisystem-Teilfixe aus PR #4465, LFRFID-Hitag-S-Dispatch-Integration
(Hardware-Test), `hitagmicro.c`/`.h`-Dispatch-Integration (blockiert an fehlender
Config-Bitwert-Quelle), GlitchTrigger-Sweep-UI-Auto-Hit-Status. Neu aus diesem Fund:
Entscheidung über `UkfeSceneModeMenu` (entfernen oder umwidmen, da jetzt reine
Duplikation der erreichbaren Settings-Funktion) für eine künftige Sitzung
zurückgestellt.
