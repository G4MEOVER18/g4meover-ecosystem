# UHF RFID (YRM100) — Port-Vorschlag: physische Back-Taste beendet die App statt zurückzunavigieren

## Was
Ich habe den seit mehreren Sitzungen offenen Rückstellpunkt "uhf_rfid-Szenen-Code (reine
UI-Ablaufsteuerung, nicht mitgeprüft)" aufgegriffen und alle 14 Szenen-C-Dateien der App
(`applications/external/uhf_rfid/scenes/`, ~1050 Zeilen) vollständig gelesen — zuvor waren nur
`uhf_app.c` und die Core-Dateien geprüft (siehe `firmware-mining/uhfrfid-hexstring-oob-read-fix.md`).

Fund: in **7 von 9 nicht-Wurzel-Szenen** wird das physische Zurück-Ereignis
(`SceneManagerEventTypeBack`) entweder gar nicht behandelt oder mit einem falschen Struct-Feld
verglichen — mit der Folge, dass `on_event()` in jedem dieser Fälle `consumed = false`
zurückgibt. Ich habe im eigenen `mntm-012`-Firmwarebaum (`applications/services/gui/
view_dispatcher.c:305-318`) nachvollzogen, was das konkret bedeutet: liefert der Navigations-
Callback (der am Ende auf die Scene-`on_event()`-Funktion der aktuellen Szene führt) `false`
zurück, ruft der View Dispatcher sofort `view_dispatcher_stop()` auf — **die physische
Zurück-Taste beendet die gesamte App**, statt eine Ebene zurückzuspringen.

Zwei getrennte Fehlerbilder, beide mit demselben Effekt:

**1. Fehlendes/falsches Feld beim Vergleich (3 Szenen).** `uhf_scene_read_tag_success.c`,
`uhf_scene_write_tag_success.c` und `uhf_scene_module_info.c` prüfen:
```c
if(event.event == SceneManagerEventTypeBack) {   // BUG: vergleicht das falsche Feld
    uhf_app->worker->state = UHFWorkerStateStop;
}
```
`SceneManagerEvent` hat laut `scene_manager.h` zwei getrennte Felder — `type`
(`SceneManagerEventTypeCustom=0`/`Back=1`/`Tick=2`) und `event` (der 32-Bit-Custom-Event-Code,
nur bei `type==Custom` sinnvoll). Ein echtes Zurück-Ereignis wird laut
`scene_manager.c:60-65` als `{.type = SceneManagerEventTypeBack}` aufgebaut — `.event` bleibt
dabei auf `0` (C-Initialisierungsregel: nicht explizit gesetzte Felder werden genullt). Der Code
vergleicht aber `event.event` (bleibt `0`) gegen den Zahlenwert `SceneManagerEventTypeBack` (`1`)
— die Bedingung ist für ein echtes Zurück-Ereignis **niemals wahr**. Die Nachbarszene
`uhf_scene_device_info.c` (dieselbe App, dieselbe Widget-Struktur mit Links-/Rechts-Tasten) macht
es *richtig*: `else if(event.type == SceneManagerEventTypeBack) { scene_manager_previous_scene(...);
consumed = true; }` — ein klarer Beleg, dass es sich um eine Kopiervorlage handelt, bei der beim
Duplizieren `type` durch `event` ersetzt wurde.

**2. Gar keine Behandlung (4 Szenen).** `uhf_scene_read_tag.c`, `uhf_scene_write_tag.c`,
`uhf_scene_delete.c` und `uhf_scene_saved_menu.c` prüfen in `on_event()` ausschließlich
`event.type == SceneManagerEventTypeCustom`-Zweige (bzw. bei `read_tag.c`/`write_tag.c` nur den
einen erwarteten `UHFCustomEventWorkerExit`-Code) und fallen für jedes andere Ereignis auf
`consumed = false` zurück — inklusive eines echten Zurück-Ereignisses.
`uhf_scene_settings.c` ist der Extremfall: `on_event()` lautet dort wortwörtlich
`UNUSED(uhf_app); UNUSED(event); return false;` — **jedes** Ereignis wird ignoriert.

Ich habe geprüft, ob eine der vier zugrunde liegenden GUI-View-Komponenten
(`Widget`/`Popup`/`Submenu`/`VariableItemList`) die physische Zurück-Taste selbst abfängt, bevor
sie den Scene Manager überhaupt erreicht (dann wäre der Fehler folgenlos) — anhand der
`_input_callback()`-Funktionen aus `applications/services/gui/modules/{widget,popup,submenu,
variable_item_list}.c` im eigenen `mntm-012`-Baum: keine der vier behandelt `InputKeyBack`
explizit, alle vier geben für diese Taste `consumed = false` zurück (der Fall fällt jeweils durch
den `default:`-Zweig bzw. wird von keiner der geprüften `case`-Bedingungen erfasst). Einzige
Ausnahme im gesamten App-Bestand: `Popup` ruft bei **jeder** kurzen Tastendruck-Art (nicht nur den
eigentlich erwarteten) den gesetzten `popup->callback` auf und markiert dabei `consumed = true`
(`popup_view_input_callback()`, `applications/services/gui/modules/popup.c:80-90`) — das betrifft
`uhf_scene_save_success.c`/`uhf_scene_delete_success.c` (beide setzen einen Popup-Callback für den
Erfolgs-Timeout) und macht deren physische Zurück-Taste bereits heute unkritisch (sie lösen
lediglich denselben Callback vorzeitig aus, kein App-Abbruch). `uhf_scene_read_tag.c`/
`write_tag.c` verwenden dieselbe `Popup`-View, setzen aber **keinen** Callback
(`popup_set_header()`/`popup_set_icon()` ohne `popup_set_callback()`) — dort greift die
Back-Taste tatsächlich durch, siehe Fund 2 oben. `uhf_scene_save_name.c` (TextInput) ist ebenfalls
unkritisch: `text_input_view_input_callback()` behandelt `InputKeyBack` explizit als Backspace und
markiert `consumed = true` (`applications/services/gui/modules/text_input.c:542-563`) — die Taste
erreicht den Scene Manager dort nie.

Alle sieben betroffenen `on_event()`-Funktionen mit einem eigenständigen Host-Repro nachgebaut und
gegen ein echtes Zurück-Ereignis (`{type=Back, event=0}`) sowie eine Regressionsmatrix aus allen
zehn bereits gültigen Ereignissen (beide Button-Indizes, alle Submenu-Indizes, alle
`UHFCustomEvent*`-Codes, ein Tick-Ereignis) geprüft — siehe Testhinweis unten.

## Nutzen
- Betrifft die zentralen, am häufigsten benutzten Abläufe der App: "Read Tag" und "Write Tag"
  sind die beiden Hauptfunktionen der gesamten App, und genau deren Detect-/Write-Lade-Bildschirme
  (`read_tag.c`/`write_tag.c`) sowie deren Erfolgsbildschirme (`read_tag_success.c`/
  `write_tag_success.c`) sind betroffen — ein Nutzer, der während "Detecting [UHF] RFID Tag" oder
  auf dem "Read Success"-Bildschirm die naheliegende physische Zurück-Taste statt eines
  D-Pad-Knopfs drückt, verliert kommentarlos die gesamte App-Sitzung, statt zur vorherigen Szene
  zurückzukehren (im Erfolgsfall exakt das, was der bereits vorhandene "Exit"-Knopf tut).
- `uhf_scene_settings.c` ist ebenfalls über das Hauptmenü ("Settings") jederzeit erreichbar und
  bietet gar keinen Bildschirm-Knopf zum Verlassen — aktuell ist die physische Zurück-Taste dort
  der einzige naheliegende Weg zurück, und genau der beendet die App komplett.
- Der Fix ist rein additiv (ein neuer `else if`-Zweig je Szene, keine bestehende Zeile entfernt
  außer der einen fehlerhaften Vergleichszeile in den drei Fällen aus Fund 1) und spiegelt in drei
  von sieben Fällen exakt eine bereits im selben Dateibestand vorhandene, korrekt funktionierende
  Aktion (die eigene "Exit"-/"Back"-Bildschirmtaste derselben Szene bzw. das
  `SaveSuccess`-Fallback-Muster für den Sprung über die transiente `FileSelect`-Szene hinweg) — ich
  erfinde damit kein neues Verhalten, sondern mache ein bereits vorhandenes für die physische Taste
  konsistent nutzbar.
- Keine Sicherheits- oder Speicherzugriffsfrage (reiner Kontrollfluss), aber ein Alltags-UX-Fehler
  mit hoher Trefferwahrscheinlichkeit — die physische Zurück-Taste ist auf dem Flipper die
  naheliegendste Geste zum Verlassen eines Bildschirms.

## Betroffene Dateien (lokal)
In `applications_user/uhf_rfid/scenes/` (App bereits identisch aus
`firmware-mining/uhfrfid-hexstring-oob-read-fix.md` übernehmbar, hier zusätzlich):
- `uhf_scene_read_tag.c` — neuer `else if(event.type == SceneManagerEventTypeBack)`-Zweig
- `uhf_scene_write_tag.c` — dieselbe Ergänzung (eigenständige, strukturell identische Kopie)
- `uhf_scene_read_tag_success.c` — fehlerhafte Vergleichszeile korrigiert
- `uhf_scene_write_tag_success.c` — dieselbe Korrektur (eigenständige, strukturell identische Kopie)
- `uhf_scene_delete.c` — neuer `else if`-Zweig
- `uhf_scene_saved_menu.c` — neuer `else if`-Zweig
- `uhf_scene_settings.c` — `on_event()` von `return false;` auf echte Behandlung umgestellt
- Unverändert, zum Vergleich mitgelesen: `uhf_scene_device_info.c`, `uhf_scene_tag_menu.c`
  (bereits korrekt), `uhf_scene_save_name.c`, `uhf_scene_save_success.c`,
  `uhf_scene_delete_success.c`, `uhf_scene_file_select.c`, `uhf_scene_start.c`,
  `uhf_scene_module_info.c` (kein Fund, siehe Risiko-Abschnitt), `uhf_scene.c`/`.h`,
  `uhf_scene_config.h`

## Risiko
- Niedrig: reine Kontrollfluss-Ergänzung in App-eigenem Code, keine Firmware-Kernänderung, keine
  neuen Abhängigkeiten. Fehlschlag zeigt sich beim Build als Compile-Fehler.
- **API-87-Kompatibilität**: `scene_manager_previous_scene()` und
  `scene_manager_search_and_switch_to_previous_scene()` sind bereits im selben App-Bestand
  mehrfach in Gebrauch (`uhf_scene_delete.c`, `uhf_scene_device_info.c`, `uhf_scene_tag_menu.c`,
  `uhf_scene_save_name.c`, `uhf_scene_save_success.c`) — keine neue API-Fläche.
- **`uhf_scene_module_info.c` bewusst nicht mitgefixt**: dieselbe fehlerhafte Vergleichszeile
  kommt dort ebenfalls vor, ist hier aber folgenlos — `uhf_app_main()` ruft
  `scene_manager_next_scene(scene_manager, UHFSceneModuleInfo)` als *allererste* Szene auf einem
  leeren Szenen-Stack auf (`uhf_app.c:208`); ein `consumed=false` führt dort zum selben Ergebnis
  (App-Ende) wie auf jeder anderen App-Wurzelszene (z. B. `uhf_scene_start.c`, das aus demselben
  Grund ebenfalls keine Zurück-Behandlung hat) — konsistentes, erwartbares Verhalten für die
  Wurzelszene, kein Fund. Die dort zusätzlich vorhandene, ebenfalls wirkungslose
  `uhf_app->worker->state = UHFWorkerStateStop`-Zuweisung habe ich gegen `verify_module_connected()`/
  `setup_and_send_rx()` (`uhf_module.c`) geprüft: keine der drei sequenziellen UART-Anfragen
  (`m100_get_hardware_version/_software_version/_manufacturers`) fragt den Worker-State während
  des Wartens ab — die Zuweisung könnte die Funktion also selbst bei korrektem Vergleich nicht
  vorzeitig abbrechen. Der tatsächliche Stopp erfolgt beim App-Ende ohnehin unbedingt über
  `uhf_free()` → `uhf_worker_stop()` (`uhf_app.c:141`), unabhängig vom Szenen-Code.
- **`uhf_scene_delete_success.c`/`uhf_scene_save_success.c` bewusst nicht mitgefixt**: beide
  verwenden `Popup` mit gesetztem Callback; `popup_view_input_callback()`
  (`applications/services/gui/modules/popup.c:80-90`) löst bei *jeder* kurzen Tastenbetätigung —
  unabhängig von der konkreten Taste — bereits den Callback aus und markiert `consumed = true`,
  bevor der Scene Manager das Ereignis überhaupt sieht. Die physische Zurück-Taste verhält sich
  dort schon heute wie ein vorzeitiges Überspringen der 1500-ms-Anzeige, kein App-Abbruch.
- Der Fix ändert das Verhalten für alle bereits gültigen Ereignisse (Bildschirm-Tasten,
  Submenu-Auswahl, Worker-/Verify-/View-Exit-Callbacks) nicht — mit Host-Repro über eine
  Regressionsmatrix aus allen zehn bislang gültigen Ereigniscodes je Szene bestätigt (siehe
  Testhinweis).

## Port-Skizze
1. App-Ordner wie in `firmware-mining/uhfrfid-hexstring-oob-read-fix.md` beschrieben nach
   `applications_user/uhf_rfid/` übernehmen (bzw. bereits vorhandene lokale Kopie weiterverwenden).
2. In `scenes/uhf_scene_read_tag.c`, vor dem abschließenden `return consumed;` in
   `uhf_scene_read_tag_on_event()`:
   ```c
   } else if(event.type == SceneManagerEventTypeBack) {
       consumed = scene_manager_previous_scene(uhf_app->scene_manager);
   }
   ```
   (aus `if` wird `} else if`, damit die bestehende `UHFCustomEventWorkerExit`-Prüfung unverändert
   bleibt.) Identisch in `scenes/uhf_scene_write_tag.c` einfügen.
3. In `scenes/uhf_scene_read_tag_success.c` die fehlerhafte erste Zeile ersetzen:
   ```c
   // ALT:
   if(event.event == SceneManagerEventTypeBack) {
       uhf_app->worker->state = UHFWorkerStateStop;
   }
   // NEU:
   } else if(event.type == SceneManagerEventTypeBack) {
       consumed = scene_manager_search_and_switch_to_previous_scene(
           uhf_app->scene_manager, UHFSceneStart);
   ```
   als zusätzlicher `else if`-Zweig ans Ende der bestehenden `if(event.type ==
   SceneManagerEventTypeCustom) {...}`-Kette gehängt (spiegelt exakt die eigene
   `GuiButtonTypeLeft`/"Exit"-Behandlung zwei Zeilen darüber). Identisch in
   `scenes/uhf_scene_write_tag_success.c`.
4. In `scenes/uhf_scene_delete.c`, als neuer `else if`-Zweig ans Ende der
   `if(event.type == SceneManagerEventTypeCustom) {...}`-Kette:
   ```c
   } else if(event.type == SceneManagerEventTypeBack) {
       consumed = scene_manager_previous_scene(uhf_app->scene_manager);
   }
   ```
   (spiegelt exakt die eigene `GuiButtonTypeLeft`/"Back"-Behandlung.)
5. In `scenes/uhf_scene_saved_menu.c`, als neuer `else if`-Zweig:
   ```c
   } else if(event.type == SceneManagerEventTypeBack) {
       consumed = scene_manager_search_and_switch_to_previous_scene(
           uhf_app->scene_manager, UHFSceneStart);
   }
   ```
   (springt bewusst wie beim `SaveSuccess`-Fallback über `UHFSceneFileSelect` hinweg direkt auf
   `UHFSceneStart` — ein einfaches `scene_manager_previous_scene()` würde sonst `FileSelect`
   erneut betreten, dessen `on_enter()` unbedingt den Dateibrowser neu öffnet.)
6. In `scenes/uhf_scene_settings.c` die `on_event()`-Funktion ersetzen:
   ```c
   bool uhf_scene_settings_on_event(void* ctx, SceneManagerEvent event) {
       UHFApp* uhf_app = ctx;
       bool consumed = false;
       if(event.type == SceneManagerEventTypeBack) {
           consumed = scene_manager_search_and_switch_to_previous_scene(
               uhf_app->scene_manager, UHFSceneStart);
       }
       return consumed;
   }
   ```
7. `ufbt launch_app APPSRC=applications_user/uhf_rfid` — reine Logikänderung, keine neuen
   Includes/Signaturen, sollte ohne Compile-Fehler durchlaufen.
8. Host-seitiges Repro (`repro.c`, unten) mit
   `gcc -O1 -g -Wall -Wextra -fsanitize=address,undefined` bauen und laufen lassen — bereits
   lokal ausgeführt, siehe Testhinweis.
9. Funktionstest am Gerät (kein YRM100-Modul nötig für Settings/Delete/SavedMenu; für
   Read/Write-Tag-Szenen genügt das Aufrufen der Szene, ein Abbruch ohne angeschlossenes Modul ist
   ausreichend):
   - Hauptmenü → "Settings" → physische Zurück-Taste drücken → muss zum Hauptmenü zurückkehren
     (vorher: App schließt sich komplett).
   - Hauptmenü → "Read Tag" (Detecting-Bildschirm) → physische Zurück-Taste drücken → muss zum
     Hauptmenü zurückkehren, ohne dass die App sich schließt.
   - Nach einem erfolgreichen Read (oder durch Navigation über eine gespeicherte `.uhf`-Datei bis
     zum Read-Success-Bildschirm) → physische Zurück-Taste drücken → muss identisch zum
     "Exit"-Knopf zum Hauptmenü zurückkehren.
   - Hauptmenü → "Saved" → Datei wählen → SavedMenu → physische Zurück-Taste drücken → muss zum
     Hauptmenü zurückkehren (nicht erneut den Dateibrowser öffnen).
   - SavedMenu → "Delete" (Bestätigungsbildschirm) → physische Zurück-Taste drücken → muss wie der
     "Back"-Knopf zum SavedMenu zurückkehren, ohne die Datei zu löschen.
   - Regressionstest: an jeder der sieben Szenen jeweils auch die bereits vorhandenen
     Bildschirm-Tasten (Exit/Back/Delete/Retry/Skip/Continue) sowie die reguläre
     Submenu-Navigation ausprobieren — Verhalten muss identisch zum Vorher-Stand bleiben.

## Testhinweis (lokal nachvollzogen)
Host-kompilierbares Mini-Repro (`repro.c`) in
`firmware-mining/repro-uhfrfid-back-button-exits-app/` — bildet alle sieben betroffenen
`on_event()`-Funktionen (ALT und vorgeschlagenes NEU) 1:1 anhand der gelesenen Quelldateien nach,
inklusive der exakten `SceneManagerEvent`/`GuiButtonType`/`UHFCustomEvent`-Zahlenwerte aus dem
eigenen `mntm-012`-Firmwarebaum. Gebaut/ausgeführt mit
`gcc -O1 -g -Wall -Wextra -fsanitize=address,undefined`:

```
=== Szenario A: echtes physisches Back (type=Back, event=0) ===
read_tag           ALT: consumed=false action=NONE           | NEU: consumed=true  action=PREV_SCENE
write_tag          ALT: consumed=false action=NONE           | NEU: consumed=true  action=PREV_SCENE
read_tag_success   ALT: consumed=false action=NONE           | NEU: consumed=true  action=SEARCH_PREV(Start)
write_tag_success  ALT: consumed=false action=NONE           | NEU: consumed=true  action=SEARCH_PREV(Start)
delete             ALT: consumed=false action=NONE           | NEU: consumed=true  action=PREV_SCENE
saved_menu         ALT: consumed=false action=NONE           | NEU: consumed=true  action=SEARCH_PREV(Start)
settings           ALT: consumed=false action=NONE           | NEU: consumed=true  action=SEARCH_PREV(Start)

=== Szenario B: Regressionscheck ueber alle bereits gueltigen Events ===
-> 7 Szenen x 10 bereits gueltige Ereignisse geprueft: 0 consumed-Abweichungen, 4 reine
Action-Unterschiede (ausschliesslich der bereits im Original auskommentierte, nie ueber einen
echten Knopf erreichbare GuiButtonTypeCenter-Zweig).

=== Szenario C: NEU spiegelt fuer read/write_tag_success exakt die eigene Exit-Taste ===
-> Fuer alle drei Szenen mit eigenem Abbrechen-Button liefert die physische Back-Taste nach dem
Fix bitgenau dieselbe Aktion wie der bereits vorhandene Bildschirm-Button.

ALLE PRUEFUNGEN BESTANDEN (0 Fehlschlaege)
```

Bestätigt: ALT liefert für ein echtes Zurück-Ereignis in allen sieben Szenen `consumed=false`
(→ App-Ende laut `view_dispatcher.c`); NEU liefert `consumed=true` mit einer Aktion, die in den
drei Fällen mit eigenem Bildschirm-Knopf (Delete, Read/Write-Tag-Success) bitgenau dessen bereits
vorhandener Aktion entspricht. Über alle zehn bereits gültigen Ereigniscodes hinweg (Bildschirm-
Tasten, Submenu-Indizes, Custom-Callback-Codes, Tick) ändert sich das extern sichtbare
`consumed`-Ergebnis in keinem einzigen Fall.

## Quelle
- RogueMaster-Firmware, `flipperzero-firmware-wPlugins`, Commit `40ca04c0` (2026-09-11,
  unverändert gegenüber dem in `firmware-mining/uhfrfid-hexstring-oob-read-fix.md` geprüften
  Stand), `applications/external/uhf_rfid/scenes/` (Original-Repo `frux-c/uhf_rfid`, Branch
  `master`, Commit `1992af4e`, `fap_author` "frux-c") — die beiden Dateien, die zwischen
  RogueMaster und Upstream differieren (`uhf_scene_delete_success.c`, `uhf_scene_save_success.c`),
  unterscheiden sich ausschließlich in einem Icon-Namen, keine Logikänderung.
- Eigener Fund: `SceneManagerEventTypeBack` wird in drei Szenen gegen das falsche Struct-Feld
  (`event.event` statt `event.type`) geprüft und in vier weiteren Szenen gar nicht behandelt —
  gegen `applications/services/gui/scene_manager.h`/`scene_manager.c`/`view_dispatcher.c` sowie
  die Input-Callbacks von `widget.c`/`popup.c`/`submenu.c`/`variable_item_list.c`/`text_input.c`
  im eigenen `mntm-012`-Firmwarebaum verifiziert; mit Host-Repro bestätigt (siehe
  `firmware-mining/repro-uhfrfid-back-button-exits-app/repro.c`).
- Restlicher, bereits vollständig geprüfter `uhf_rfid`-Bestand (Core-Dateien, `convertToHexString()`
  -Fund) siehe `firmware-mining/uhfrfid-hexstring-oob-read-fix.md`. Damit ist der komplette
  `uhf_rfid`-App-Quelltext (Core + Scenes) jetzt vollständig geprüft.
