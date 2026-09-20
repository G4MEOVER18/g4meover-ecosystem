# lora-ukfe — Katalog-Menüpunkt "Payload-Run [V3]" bekommt echte Index-Eingabe statt fest arg=0

## Was
Ich habe den Geschwister-Rückstellpunkt aus `catalog-trigger-arg-format-mismatch-fix.md`
aufgegriffen, der beim vorherigen Fund ("Trigger [V3]") bewusst zurückgestellt wurde: der
Katalog-Eintrag `FN_PAYLOAD[1]` = `{"Payload-Run [V3]", UkfeRfCmdPayloadRun, 0, true}`
(`command_catalog.h`) hat dasselbe fest verdrahtete `arg = 0` wie "Trigger [V3]" es hatte —
über das Katalog-Menü lässt sich damit praktisch nur Payload-Index 0 auslösen, jeder andere
Index ist unerreichbar. Anders als beim Trigger-Fund ist das hier **kein Protokollfehler**:
`rf/ukfe_rf.h:27` dokumentiert `UkfeRfCmdPayloadRun = 0x05, // args: uint8 idx` — genau 1
Arg-Byte, exakt das, was der generische Pfad in `scene_menu.c`s `send_fn()` für jeden
`has_arg`-Eintrag ohnehin baut. Reine Bedienungslücke: die Zahl 0 ist die einzige, die das
Menü je anbietet.

Ich habe dafür dieselbe Lösung wie beim Trigger-Fund portiert, nur um einen Schritt kürzer:
eine neue Scene `scene_payload_run_confirm.c` fragt beim Auslösen von "Payload-Run [V3]"
einen einzelnen Payload-Index (0–255) über das in der vorherigen Sitzung bereits eingebundene
Flipper-Standardmodul `gui/modules/number_input.h` ab und sendet danach über denselben
generischen 1-Byte-Pfad, den `send_fn()` für jeden anderen `has_arg`-Eintrag nutzt (`cmd`,
`arg_len=1`, `args[0]=idx`) — keine neue Kodierlogik, nur der Wert kommt jetzt aus der
Eingabe statt aus dem festen Katalog-`arg`.

## Nutzen
- Macht "Payload-Run [V3]" über das Katalog-Menü für jeden Payload-Index (0–255) nutzbar —
  bisher war ausschließlich Index 0 erreichbar.
- Baut auf der in der vorherigen Sitzung bereits eingebundenen `NumberInput`-Infrastruktur
  auf (`app->number_input`, `UkfeViewNumberInput` in `lora_ukfe.h`/`.c`) — keine erneute
  View-/Modul-Registrierung nötig, nur eine weitere Scene, die dieselbe View-Instanz nutzt.
- Rührt am bestehenden Wire-Format nichts an: `send_fn()`s generischer 1-Byte-Pfad war für
  diesen Eintrag bereits korrekt (anders als beim `UkfeRfCmdTrigger`-Fund) — die neue Scene
  baut den `UkfeRfMessage` nach demselben Schema selbst, statt eine parallele Bau-Funktion
  einzuführen.
- Additiv und eng begrenzt: ein gezielter `else if`-Zweig in `menu_callback()` vor dem
  unveränderten generischen Pfad, ausschließlich für `cmd == UkfeRfCmdPayloadRun`. Alle
  anderen Katalog-Einträge (inklusive des bereits eigenständig behandelten "Trigger [V3]")
  bleiben exakt auf ihrem bisherigen Pfad.

## Betroffene Dateien
- `scenes/scene_payload_run_confirm.c` — **neu**. Einschritt-Zahleneingabe (Payload-Index
  0–255 → Senden). Back wird nicht konsumiert, da es (anders als beim zweistufigen
  Trigger-Ablauf) keinen Zwischenzustand gibt, zu dem zurückgegangen werden könnte —
  Standardnavigation zurück zum Katalog-Menü greift direkt.
- `scenes/scenes.h` — neuer Scene-ID-Wert `UkfeScenePayloadRunConfirm` (im Enum nach
  `UkfeSceneTriggerConfirm` eingefügt) plus die drei Funktionsdeklarationen.
- `scenes/scene_handlers.c` — die drei `UkfeScenePayloadRunConfirm`-Einträge in den
  Handler-Tabellen ergänzt.
- `scenes/scene_menu.c` — `menu_callback()`: ein weiterer, gezielter `else if`-Zweig
  (`cmd == UkfeRfCmdPayloadRun && has_arg`) vor dem bestehenden generischen Pfad, analog zum
  bereits vorhandenen `UkfeRfCmdTrigger`-Zweig.

Keine Änderung an `command_catalog.h` (Katalog-Eintrag bleibt unverändert), `rf/ukfe_rf.c`/
`.h` (nutzt nur das bereits korrekte 1-Byte-Format), `lora_ukfe.h`/`.c` (die `NumberInput`-
Infrastruktur existiert bereits durch den vorherigen Trigger-Fix) oder einer anderen Scene.

## Port-Skizze / Patch
**Setzt auf dem bereits vorgeschlagenen
`trigger-catalog-number-input-port/trigger_catalog_number_input.patch` auf** (derselben
Sitzungsreihe, noch nicht upstream gemerged) — dieser Patch hier ist gegen eine Basis
erstellt, auf der jener bereits angewendet wurde, da er dessen `NumberInput`-Infrastruktur
(`app->number_input`, `UkfeViewNumberInput`) wiederverwendet. Lokale Anwendungsreihenfolge:
zuerst `trigger_catalog_number_input.patch`, danach
`payload-run-catalog-number-input-port/payload_run_catalog_number_input.patch`. Beide
zusammen `git apply --check` gegen eine frische Kopie der unveränderten
`G4MEOVER18/lora-ukfe`-Basis (Commit `0dba410a`) verifiziert, real angewendet und
byte-für-byte mit der Arbeitskopie abgeglichen — identisch.

Kernstück, `scenes/scene_payload_run_confirm.c`:
```c
static void payload_run_idx_cb(void* ctx, int32_t number) {
    LораUkfeApp* app = ctx;
    UkfeRfMessage m;
    m.cmd = UkfeRfCmdPayloadRun;
    m.arg_len = 1;
    m.args[0] = (uint8_t)number;
    ukfe_uart_send_rf(app, &m);
    scene_manager_next_scene(app->scene_manager, UkfeSceneLog);
}

void scene_payload_run_confirm_on_enter(void* ctx) {
    LораUkfeApp* app = ctx;
    number_input_set_header_text(app->number_input, "Payload-Idx (0-255)");
    number_input_set_result_callback(app->number_input, payload_run_idx_cb, app, 0, 0, 255);
    view_dispatcher_switch_to_view(app->view_dispatcher, UkfeViewNumberInput);
}

bool scene_payload_run_confirm_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;   // kein Zwischenzustand -> Standardnavigation greift
}
```

`scenes/scene_menu.c` — `menu_callback()`, zweiter gezielter Sonderfall neben dem
bestehenden `UkfeRfCmdTrigger`-Zweig:
```c
const UkfeFn* fn = &CATALOG[s_cat].fns[index];
if(fn->cmd == UkfeRfCmdTrigger && fn->has_arg) {
    scene_manager_next_scene(app->scene_manager, UkfeSceneTriggerConfirm);
} else if(fn->cmd == UkfeRfCmdPayloadRun && fn->has_arg) {
    scene_manager_next_scene(app->scene_manager, UkfeScenePayloadRunConfirm);
} else {
    send_fn(app, fn);
    scene_manager_next_scene(app->scene_manager, UkfeSceneLog);
}
```

## Testhinweis (ohne Hardware)
Reine Logik/State, vollständig ohne Flipper/CC1101/Heltec nachstellbar —
`repro-payload-run-catalog-number-input/repro.c` kompiliert dafür **die echte,
unveränderte** `scenes/scene_payload_run_confirm.c` dieser Sitzung direkt mit (keine
Transkription), zusammen mit dem ebenfalls unveränderten `rf/ukfe_rf.c/.h`. Nur
`lora_ukfe.h`/`scenes.h` sind schlanke Mock-Header (1:1 nach demselben Muster wie in
`repro-trigger-catalog-number-input/`), die ausschließlich die Typen/Funktionssignaturen
bereitstellen, die die echte Datei referenziert. Die GUI-Tastaturmechanik von
`number_input.c` selbst wird nicht nachgebaut, sondern als bereits produktiv verifiziert
vorausgesetzt (ruft ihren Callback laut eigener Implementierung nur mit einem Wert innerhalb
`[min,max]` auf) — `simulate_enter()` bildet genau das nach.

Vier Szenarien:
1. **Normalablauf**: `on_enter` zeigt "Payload-Idx (0-255)" (Min/Max 0/255, Vorbelegung 0),
   noch nichts gesendet; Eingabe 17 sendet — geprüft wird sowohl der aufgezeichnete
   `UkfeRfMessage` (`cmd=UkfeRfCmdPayloadRun`, `arg_len=1`, `args[0]=17`) als auch der volle
   Wire-Rundlauf über den echten `ukfe_rf_build_frame()`/`_parse_frame()` (Frame exakt
   `UKFE_RF_HDR_OVERHEAD+1` Byte, geparst `idx=17`), sowie die anschließende Navigation zu
   `UkfeSceneLog`.
2. **Back unkonsumiert**: `on_event` mit einem Back-Event liefert `consumed=false`, keine
   Rekonfiguration, nichts gesendet — Beleg, dass die Standardnavigation der Scene (zurück
   zum Katalog-Menü) hier absichtlich nicht abgefangen wird (anders als der Delay-Schritt
   beim Trigger-Fund, da es hier nur einen einzigen Eingabeschritt gibt).
3. **Randfall-Rundlauf**: sechs Indizes (`0, 1, 42, 128, 254, 255`) je durch den vollen
   `on_enter`→Eingabe→Senden-Ablauf, jeweils gegen den echten
   `ukfe_rf_build_frame()`/`_parse_frame()`-Rundlauf verifiziert.
4. **Wiederholter Scene-Aufruf**: nach einem gesendeten Index 99 erneut `on_enter` aufgerufen
   — Callback/Header/Vorbelegung werden komplett neu gesetzt (wieder bei 0, kein
   Überbleibsel aus dem vorherigen Durchlauf), ein danach eingegebener Index 200 wird
   unabhängig vom vorherigen korrekt gesendet.

```
$ gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined -I. \
      -o repro repro.c scenes/scene_payload_run_confirm.c rf/ukfe_rf.c
$ ./repro
OK:   Normalablauf Index=17 -> korrekter 1-Byte-PayloadRun-Frame, danach UkfeSceneLog.
OK:   Back bleibt unkonsumiert (consumed=false) -- Standardnavigation der Scene zurueck zum Katalog-Menue greift.
OK:   6 Payload-Indizes (inkl. Randfaellen 0 und 255) verlustfrei durch den vollen Ablauf gesendet.
OK:   Wiederholter Scene-Aufruf startet wieder bei Index 0, unabhaengig vom zuvor gesendeten Index.
Alle Szenarien bestaetigt (exit code 0)
```
Keine ASan-/UBSan-Meldungen, 4/4 Szenarien bestanden.

Funktionaler Test danach lokal auf dem Flipper: Kategorie "Payloads" → "Payload-Run [V3]"
öffnen, einen Payload-Index eingeben (Abgleich, welcher Index zu welcher Payload gehört,
über "Payloads" im Hauptmenü/`scene_payload_list.c`, die die vom Hub gemeldete Liste mit
IDs/Namen anzeigt), mit OK bestätigen; auf der Empfängerseite (Hub/Satellit) prüfen, dass
ein 1-Byte-`UkfeRfCmdPayloadRun`-Frame mit dem gewählten Index ankommt und die erwartete
Payload ausgeführt wird (nicht mehr zwangsläufig Index 0).

## Risiko
- Sehr niedrig: additiv in `scene_menu.c` (ein weiterer gezielter `else if`-Zweig neben dem
  bereits vorhandenen `UkfeRfCmdTrigger`-Zweig, vor dem für alle übrigen Katalog-Einträge
  unveränderten generischen Pfad), neue eigenständige Scene-Datei mit neuer, bisher
  ungenutzter Scene-ID. Kein bestehender Menüpunkt ändert sein Verhalten.
- Kein neues Wire-Format, keine neue Kodierlogik: baut den `UkfeRfMessage` nach exakt
  demselben Schema, das `send_fn()` für diesen Eintrag ohnehin schon anwendet (`arg_len=1`)
  — nur der Wert kommt jetzt aus der Eingabe.
- Wiederverwendet ausschließlich bereits vorhandene, in der vorherigen Sitzung eingebundene
  Infrastruktur (`NumberInput`-View) — kein zusätzlicher Speicher-, Storage- oder
  Netzwerkzugriff, keine neue Zustandsvariable in `LораUkfeApp`.
- Einziger Eingabeschritt, kein Zwischenzustand wie beim zweistufigen Trigger-Ablauf — kein
  zusätzliches Back-Navigations-Risiko.
- Abhängigkeit: setzt voraus, dass `trigger_catalog_number_input.patch` bereits angewendet
  wurde (liefert `app->number_input`/`UkfeViewNumberInput`). Ohne diesen fehlt die
  Voraussetzung — beide Patches gehören lokal zusammen angewendet.

## Offen (bewusst nicht mitbehoben)
- Die Eingabe bleibt ein reiner Zahlenwert (0–255), keine Namensauswahl aus der vom Hub
  gemeldeten Payload-Liste (`app->payloads[]`, von `scene_payload_list.c` verwaltet). Eine
  Namens-Auswahl wäre eine größere, eigenständige UI-Erweiterung (Submenü mit bedingter
  Zahlen-Eingabe als Fallback, falls die Liste noch nicht vom Hub geladen wurde) und nicht
  Teil dieses auf die reine Bedienungslücke begrenzten Fixes — zumal unklar ist, ob das
  `idx`-Feld des `UkfeRfCmdPayloadRun`-Protokolls (`rf/ukfe_rf.h:27`) dieselbe Zählung wie
  das `id`-Feld der JSON-Payload-Liste verwendet oder eine eigene, hub-seitige Indexierung;
  das wäre nur mit Einblick in die Heltec-/Hub-Firmware sicher zu klären, nicht rein aus
  diesem Flipper-Quelltext.
- `command_catalog.h` selbst bleibt reine, unveränderte Daten — der Fund liegt
  ausschließlich in ihrem Konsumenten (`scene_menu.c`).

## Quelle
- `G4MEOVER18/lora-ukfe` (main, `0dba410a`): `scenes/scene_menu.c` (`send_fn()`,
  `menu_callback()`), `scenes/scenes.h` (`UkfeScene`-Enum), `scenes/scene_handlers.c`
  (Handler-Tabellen), `rf/ukfe_rf.h` (`UkfeRfCmdPayloadRun`-Kommentar), `command_catalog.h`
  (`FN_PAYLOAD`-Tabelle), `lora_ukfe.h` (`UkfePayloadEntry`/`app->payloads[]` als Kontext
  für den "Offen"-Punkt).
- `app-improvements/lora-ukfe/trigger-catalog-number-input-port.md` (dieses Repo, vorherige
  Sitzung) — Basis-Infrastruktur (`NumberInput`-View) und identisches Lösungsmuster für den
  Geschwister-Katalog-Eintrag.
- `app-improvements/lora-ukfe/catalog-trigger-arg-format-mismatch-fix.md` (dieses Repo) —
  ursprüngliche Notiz des hier aufgegriffenen "Offen"-Punkts.
