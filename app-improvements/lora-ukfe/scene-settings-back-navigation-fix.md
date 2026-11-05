# lora-ukfe — Settings-Scene verschluckt den Back-Taster, ohne jemals zurückzunavigieren

## Was
Ich habe die beiden bislang in keinem meiner `lora-ukfe`-Funde erwähnten Scene-Dateien
geprüft, `scenes/scene_settings.c` und `scenes/scene_status.c` (Quer-Check über alle
bisherigen `Quelle`-Abschnitte bestätigt: `scene_settings.c` kommt in keinem bisherigen
Fund vor). `scene_status.c` ist in Ordnung — sein Back-Zweig navigiert korrekt. In
`scene_settings.c` dagegen:

```c
bool scene_settings_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    bool consumed = false;
    if(event.type == SceneManagerEventTypeBack) {
        consumed = true;
    }
    return consumed;
}
```

Der Back-Zweig setzt `consumed = true`, ruft aber nie eine Navigationsfunktion auf. Im
`SceneManager`-Modell bedeutet `consumed == true` "die Scene hat das Event bereits selbst
behandelt" — der `SceneManager` unternimmt dann *nichts* von sich aus (keine automatische
`previous_scene()`, kein Verlassen der App). Der Navigationsstapel bleibt exakt so, wie er
war. Jeder Druck auf Back in dieser Scene ist dadurch wirkungslos; es gibt in der
`Settings`-Ansicht selbst (`VariableItemList` mit nur den Einträgen "Modus" und
"LoRa-Profil") auch sonst keinen Menüpunkt, der zurücknavigiert.

Dass das kein Sonderfall, sondern eine Abweichung vom eigenen etablierten Muster ist,
zeigt der Vergleich mit den drei anderen Scenes, die exakt dasselbe Back-Verhalten
brauchen (Blattknoten ohne eigene Unterebene) und es alle gleich lösen:

```c
// scene_status.c, scene_log.c, scene_payload_list.c — jeweils identisch:
} else if(event.type == SceneManagerEventTypeBack) {
    scene_manager_previous_scene(app->scene_manager);
    consumed = true;
}
```

`scene_menu.c` (die einzige Scene mit einer eigenen Unterebene) macht für die
Kategorie-Ebene dasselbe, nur mit eigener Rücksprunglogik statt eines reinen
`previous_scene()`-Aufrufs. `scene_settings.c` ist die einzige der fünf Scenes im
gesamten `scenes/`-Verzeichnis, die den `consumed = true`-Zweig ohne begleitenden
Navigationsaufruf verwendet — ein Copy-Paste-Auslassungsfehler gegenüber dem sonst
durchgängigen Muster.

Nebenbefund (kein eigener Fix hier, nur zur Einordnung): `UkfeSceneSettings` ist aktuell
von keiner Stelle aus erreichbar — `grep -rn scene_manager_next_scene` über den gesamten
Baum liefert nur drei Aufrufstellen (Programmstart → `UkfeSceneMenu`, zweimal → `UkfeSceneLog`
aus `scene_menu.c`), keine einzige zu `UkfeSceneSettings` oder `UkfeSceneModeMenu`. Beide
Scenes sind vollständig implementiert und im Handler-Array (`scene_handlers.c`) verdrahtet,
aber ohne Menüeintrag, der dorthin führt — vermutlich ein Rest aus der Zeit vor der
jetzigen Kategorie-/Katalog-Menüstruktur in `scene_menu.c`. Der Back-Fehler in
`scene_settings.c` ist davon unabhängig real (reine Code-Logik, unabhängig von der
Erreichbarkeit), wird aber erst sichtbar, sobald ich künftig einen Menüeintrag dorthin
ergänze — dann träfe ich sonst genau auf diese Sackgasse. Das Verdrahten eines
Menüeintrags selbst ist eine eigene Entscheidung (wo in der Kategorie-Struktur "Settings"
hingehört) und bewusst nicht Teil dieses Fixes.

## Nutzen
- Behebt den einzigen Navigationsfehler im gesamten `scenes/`-Verzeichnis: sobald
  `UkfeSceneSettings` erreichbar gemacht wird (eigener, hier nicht behandelter Schritt),
  verlässt sich der Back-Taster auf dasselbe Verhalten wie überall sonst in der App,
  statt den Nutzer in einer Sackgasse ohne Ausweg zurückzulassen.
- Rein additiv: ein ergänzter Funktionsaufruf, keine Änderung an `mode_change()`, den
  beiden `VariableItem`-Einträgen oder `scene_settings_on_enter/_on_exit`.
- Bringt `scene_settings.c` exakt auf das bereits an drei anderen Stellen im selben
  Verzeichnis verwendete Muster — keine neue Idee, nur das Schließen der einen Lücke.

## Betroffene Dateien
- `scenes/scene_settings.c` — `scene_settings_on_event()`: `UNUSED(ctx)` entfällt (der
  Kontext wird jetzt für den `scene_manager`-Zeiger gebraucht), neuer Aufruf
  `scene_manager_previous_scene(app->scene_manager)` im Back-Zweig, vor dem bestehenden
  `consumed = true`.

## Patch
`scene-settings-back-navigation-fix/scene_settings.patch` — gegen eine frische
Zweitkopie von `G4MEOVER18/lora-ukfe` (`0dba410a`) mit `git apply --check` verifiziert,
real angewendet, Ergebnisdatei per `md5sum` mit der Patch-Quelle abgeglichen — identisch.

```c
bool scene_settings_on_event(void* ctx, SceneManagerEvent event) {
    LораUkfeApp* app = ctx;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }
    return consumed;
}
```

## Host-Repro
Reine Navigationsstapel-Logik (kein Hardware-, UART- oder GUI-Zugriff). Wie beim
FlipperJammer-Fund `scene-run-exit-lands-on-duration-picker-fix` habe ich den
`SceneManager` als minimales Push/Pop-Modell nachgebaut (`handle_back_event()` ruft den
Scene-Handler; bei `consumed == false` poppt es automatisch eine Ebene, bei
`consumed == true` passiert nichts von sich aus) und beide Varianten von
`scene_settings_on_event()` eins zu eins aus dem Patch übernommen:

```
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -O0 -o repro repro.c && ./repro
ALT:  vor Back: Settings | nach Back: Settings (handled=1)
NEU:  vor Back: Settings | nach Back: Menu (handled=1)
9/9 Pruefungen bestanden
ALLE CHECKS PASSED
```

Drei Szenarien: (1) ALT bleibt nach einem und nach drei Back-Drücken unverändert auf
`Settings` stehen — die Sackgasse ist deterministisch, nicht nur ein Rand-Timing-Fall;
(2) NEU landet nach einem Back-Druck korrekt wieder auf `Menu`; (3) NEU verhält sich
identisch zu einem Modell der bereits korrekten Geschwister-Scenes, ALT weicht
nachweislich davon ab. Bei `-O0` und `-O1` identischer Output, keine
Compiler-Warnungen, keine Sanitizer-Befunde — in
`repro-scene-settings-back-navigation/repro.c`.

Funktionaler Test danach lokal auf dem Flipper: sobald `UkfeSceneSettings` über einen
Menüeintrag erreichbar ist, dorthin navigieren und Back drücken — vor dem Fix bleibt die
Ansicht stehen, nach dem Fix geht es zurück zum vorherigen Bildschirm.

## Risiko
- Sehr niedrig: eine Zeile ergänzt, keine bestehende Logik verändert oder entfernt.
  `mode_change()` (Moduswahl) und die `VariableItemList`-Einträge selbst bleiben
  unangetastet.
- `UNUSED(ctx)` entfällt, weil `ctx` jetzt gebraucht wird — keine Funktionsänderung,
  nur die bisher ungenutzte Parameterwarnung-Unterdrückung fällt weg, da nicht mehr
  nötig.
- Da `UkfeSceneSettings` aktuell unerreichbar ist (siehe oben), hat dieser Fix bis zum
  Verdrahten eines Menüeintrags keine im Betrieb sichtbare Auswirkung — er beseitigt
  aber die Sackgasse, auf die ich sonst bei genau diesem nächsten Schritt träfe.
- Kein Sicherheits-/Missbrauchsrisiko: reine GUI-Navigationslogik, keine Protokoll-,
  Speicher- oder Funkänderung.

## Testhinweis
Siehe Host-Repro oben (vollständig ohne Hardware verifizierbar). Der funktionale
Flipper-Test setzt einen künftigen Menüeintrag zu `UkfeSceneSettings` voraus — bis dahin
nur per Host-Repro nachvollziehbar.

## Quelle
- `G4MEOVER18/lora-ukfe` (main, `0dba410a`): `scenes/scene_settings.c`
  (`scene_settings_on_event`), zum Abgleich `scenes/scene_status.c`,
  `scenes/scene_log.c`, `scenes/scene_payload_list.c`, `scenes/scene_menu.c` (jeweils
  `on_event`, etabliertes Back-Muster), `scenes/scenes.h`/`scenes/scene_handlers.c`
  (Scene-Enum/Handler-Tabelle, Gegenprobe für "keine Aufrufstelle zu
  `UkfeSceneSettings`/`UkfeSceneModeMenu`"), `lora_ukfe.c` (`app_alloc`, einzige
  `scene_manager_next_scene`-Aufrufstellen-Suche).

## Offen
Weiterhin offen (unverändert aus vorherigen Sitzungen): `texkom` Phase 2-5, NFC/RFID-
Parser-Teilaspekt, MIFARE-Plus-/Ultralight-AES Phase 3 Rest, PSA-Bruteforce-Krypto-
Konstanten, `sanitize_filename()`-Zeichenbereichs-Nebenfund, neun verbleibende
FeliCa-Multisystem-Teilfixe aus PR #4465, LFRFID-Hitag-S-Dispatch-Integration
(Hardware-Test), `hitagmicro.c`/`.h`-Dispatch-Integration (blockiert an fehlender
Config-Bitwert-Quelle), GlitchTrigger-Sweep-UI-Auto-Hit-Status. Neu aus diesem Fund:
`UkfeSceneSettings`/`UkfeSceneModeMenu` bleiben ohne Menüeintrag unerreichbar — ob und wo
in der Kategorie-Struktur von `scene_menu.c` ein Zugang dafür ergänzt wird, ist eine
eigene, hier bewusst nicht mitentschiedene Frage für eine künftige Sitzung.
