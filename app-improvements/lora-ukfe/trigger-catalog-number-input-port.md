# lora-ukfe — Katalog-Menüpunkt "Trigger [V3]" bekommt echte Payload-ID/Delay-Eingabe statt fest arg=0

## Was
Ich habe den bereits mehrfach als "Offen" vermerkten Rückstellpunkt aus
`catalog-trigger-arg-format-mismatch-fix.md` aufgegriffen: der Katalog-Menüpunkt
"Trigger [V3]" (`command_catalog.h`, `FN_PAYLOAD[0]` — `{UkfeRfCmdTrigger, 0, true}`)
lässt sich im Katalog-Menü (`scenes/scene_menu.c`) bislang nur mit `arg = 0` auslösen —
die Payload-ID und der Verzögerungswert sind im `UkfeFn`-Eintrag fest verdrahtet, das
Menü selbst fragt beide nie ab. Ich habe zusätzlich beim erneuten Durchgehen von
`scenes/scenes.h` festgestellt, dass genau dafür bereits eine Scene-ID reserviert
war: `UkfeSceneTriggerConfirm` existiert im `UkfeScene`-Enum, aber `scene_handlers.c`
verdrahtet sie bislang nur als Alias auf `scene_status_on_enter/_on_event/_on_exit` —
kein eigenständiger Handler, keine eigene Datei, funktional ein Duplikat der
Status-Ansicht statt einer eigenen Eingabe-Scene. Der reservierte Platz war also schon
vorgesehen, nur nie gefüllt.

Ich habe diesen Platz jetzt gefüllt: eine neue Scene `scene_trigger_confirm.c` fragt
beim Auslösen von "Trigger [V3]" zuerst die Payload-ID (0–255), danach den
Verzögerungswert in ms ab (Flipper-Standardmodul `gui/modules/number_input.h` — ein
Integer-Tastenfeld mit Min/Max-Validierung, das schon in mehreren OFW-/Momentum-Apps
produktiv läuft, hier aber in dieser App noch nirgends eingebunden war), und sendet
danach direkt über `ukfe_rf_make_trigger()` + `ukfe_uart_send_rf()` einen korrekten
5-Byte-`UkfeRfCmdTrigger`-Frame (`id` + `delay_ms` LE) — denselben Builder, den der
bereits dokumentierte Wire-Format-Fix für `send_fn()`s generischen Pfad vorschlägt. Da
der Katalog-Eintrag "Trigger [V3]" jetzt vor `send_fn()` abgefangen und direkt in die
neue Scene umgeleitet wird, läuft er nie mehr über den generischen 1-Byte-Pfad —
**der dort vorgeschlagene `send_fn()`-Sonderfall wird für diesen Eintrag durch dieses
Feature überflüssig** (er bliebe nur relevant, falls künftig ein *zweiter*
Katalog-Eintrag mit `cmd == UkfeRfCmdTrigger` hinzukäme).

## Nutzen
- Macht "Trigger [V3]" über das Katalog-Menü für jede Payload-ID (0–255) und einen
  frei wählbaren Verzögerungswert nutzbar — bisher war praktisch nur Payload-ID 0,
  sofort (delay_ms=0) erreichbar, alles andere erforderte den Umweg über
  `scene_payload_list.c` (die allerdings, siehe unten, einen anderen Transport
  verwendet und `delay_ms` ohnehin nie abfragt).
- Behebt nebenbei den bereits dokumentierten Wire-Format-Fehler (`arg_len=1` statt der
  vom Protokoll geforderten 5 Byte) für genau diesen Menüpunkt, ohne den in
  `catalog-trigger-arg-format-mismatch-fix.md` vorgeschlagenen Sonderfall in
  `send_fn()` überhaupt zu benötigen — eine Quelle der Korrektheit statt zwei.
- Füllt eine bereits reservierte, aber leere Scene-ID (`UkfeSceneTriggerConfirm`)
  mit tatsächlicher Funktionalität, statt eine weitere Scene-ID anzulegen.
- Nutzt ausschließlich bereits vorhandene, produktiv genutzte Bausteine:
  `gui/modules/number_input.h` (Standard-Flipper-Modul), `ukfe_rf_make_trigger()`
  (bereits vorhanden, bislang im gesamten Baum nirgends aufgerufen — siehe
  `catalog-trigger-arg-format-mismatch-fix.md`), `ukfe_uart_send_rf()` (bereits von
  `send_fn()` für jeden anderen Katalog-Eintrag genutzt). Keine neue Transportart,
  kein neues Wire-Format.
- Bewusst **nicht** angefasst: `scene_payload_list.c` sendet weiterhin über
  `ukfe_uart_send_trigger()` — einen eigenständigen, JSON-basierten Direktmodus-Pfad
  (`ukfe_uart_send_cmd()`, reiner Text über dieselbe UART, kein `ukfe_rf`-Binärframe).
  Das ist ein zweiter, unabhängiger Transport für dieselbe fachliche Aktion
  ("Trigger"), keine Redundanz zu diesem Fix — beide Pfade bleiben nebeneinander
  bestehen, wie sie es schon vor dieser Änderung taten.

## Betroffene Dateien
- `scenes/scene_trigger_confirm.c` — **neu**. Zwei-Schritt-Zustandsautomat
  (Payload-ID → Delay-ms → Senden), Back während der Delay-Eingabe geht einen Schritt
  zurück zur ID-Eingabe (mit der zuletzt eingegebenen ID vorbelegt) statt die ganze
  Scene zu verlassen; Back während der ID-Eingabe bleibt unkonsumiert, damit die
  normale Scene-Manager-Navigation zurück zum Katalog-Menü greift.
- `scenes/scenes.h` — Funktionsdeklarationen für die neue Scene ergänzt (die
  Scene-ID `UkfeSceneTriggerConfirm` existierte, wie oben beschrieben, bereits).
- `scenes/scene_handlers.c` — die drei `UkfeSceneTriggerConfirm`-Einträge von
  `scene_status_on_enter/_on_event/_on_exit` (Alias/Platzhalter) auf die neuen
  `scene_trigger_confirm_on_enter/_on_event/_on_exit` umgehängt.
- `scenes/scene_menu.c` — `menu_callback()`: Katalog-Einträge mit
  `cmd == UkfeRfCmdTrigger && has_arg` werden jetzt vor `send_fn()` abgefangen und
  navigieren stattdessen zu `UkfeSceneTriggerConfirm`. Alle anderen Katalog-Einträge
  (0-Byte- und 1-Byte-Args) bleiben exakt auf dem bisherigen, unveränderten Pfad.
- `lora_ukfe.h` — `#include <gui/modules/number_input.h>` ergänzt, neues Feld
  `NumberInput* number_input` im App-State, neuer View-Enum-Wert
  `UkfeViewNumberInput`.
- `lora_ukfe.c` — `number_input_alloc()`/`_add_view()` in `app_alloc()`,
  `_remove_view()`/`_free()` in `app_free()` — exakt dasselbe Muster wie die drei
  bereits vorhandenen Views (`submenu`, `text_box`, `var_list`).

## Port-Skizze / Patch
Vollständiger Unified-Diff (gegen die unveränderte `G4MEOVER18/lora-ukfe`-Basis,
Commit `0dba410a`, `git apply --check` vorab gegen eine frische Kopie verifiziert,
real angewendet und byte-für-byte mit der Arbeitskopie abgeglichen — identisch) in
`trigger-catalog-number-input-port/trigger_catalog_number_input.patch`.

Kernstück, `scenes/scene_trigger_confirm.c`:
```c
#define TRIGGER_DELAY_MS_MAX 300000   // 5 Min. UI-Obergrenze (Protokoll traegt volle uint32)

typedef enum { TriggerStepId = 0, TriggerStepDelay } TriggerStep;
static TriggerStep s_step;
static uint8_t     s_id;

static void trigger_delay_cb(void* ctx, int32_t number) {
    LораUkfeApp* app = ctx;
    UkfeRfMessage m;
    ukfe_rf_make_trigger(&m, s_id, (uint32_t)number);
    ukfe_uart_send_rf(app, &m);
    scene_manager_next_scene(app->scene_manager, UkfeSceneLog);
}

static void show_delay_step(LораUkfeApp* app, int32_t preset) {
    s_step = TriggerStepDelay;
    number_input_set_header_text(app->number_input, "Delay ms (0-300000)");
    number_input_set_result_callback(
        app->number_input, trigger_delay_cb, app, preset, 0, TRIGGER_DELAY_MS_MAX);
}

static void trigger_id_cb(void* ctx, int32_t number) {
    LораUkfeApp* app = ctx;
    s_id = (uint8_t)number;
    show_delay_step(app, 0);
}

static void show_id_step(LораUkfeApp* app, int32_t preset) {
    s_step = TriggerStepId;
    number_input_set_header_text(app->number_input, "Payload-ID (0-255)");
    number_input_set_result_callback(app->number_input, trigger_id_cb, app, preset, 0, 255);
}

void scene_trigger_confirm_on_enter(void* ctx) {
    LораUkfeApp* app = ctx;
    s_id = 0;
    show_id_step(app, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, UkfeViewNumberInput);
}

bool scene_trigger_confirm_on_event(void* ctx, SceneManagerEvent event) {
    LораUkfeApp* app = ctx;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeBack) {
        if(s_step == TriggerStepDelay) {
            show_id_step(app, s_id);   // zurueck zur ID-Eingabe, Wert bleibt vorbelegt
            consumed = true;
        }
        // Back im ID-Schritt: nicht konsumiert -> normale Scene-Navigation
    }
    return consumed;
}
```

`scenes/scene_menu.c` — `menu_callback()`, gezielter Sonderfall vor `send_fn()`:
```c
const UkfeFn* fn = &CATALOG[s_cat].fns[index];
if(fn->cmd == UkfeRfCmdTrigger && fn->has_arg) {
    scene_manager_next_scene(app->scene_manager, UkfeSceneTriggerConfirm);
} else {
    send_fn(app, fn);
    scene_manager_next_scene(app->scene_manager, UkfeSceneLog);
}
```

`number_input_set_result_callback()`/`_set_header_text()` sind in der echten
`number_input.c` so gebaut, dass ein erneuter Aufruf das View-Model komplett neu
befüllt (Header, Textpuffer, Callback, Min/Max) und einen Redraw auslöst — die Scene
kann also beide Eingabeschritte über dieselbe `UkfeViewNumberInput`-View-Instanz
abwickeln, ohne zwischendurch die View zu wechseln (`view_dispatcher_switch_to_view()`
läuft nur einmal, beim Scene-Eintritt).

## Testhinweis (ohne Hardware)
Der Zustandsautomat selbst (Schrittfolge ID→Delay→Senden, Back-Korrektur, welcher
`UkfeRfMessage`-Frame am Ende tatsächlich gebaut wird) ist reine Logik/State und
lässt sich vollständig ohne Flipper/CC1101/Heltec nachstellen —
`repro-trigger-catalog-number-input/repro.c` kompiliert dafür **die echte,
unveränderte** `scenes/scene_trigger_confirm.c` dieser Sitzung direkt mit (keine
Transkription), zusammen mit dem ebenfalls unveränderten `rf/ukfe_rf.c/.h`. Nur
`lora_ukfe.h`/`scenes.h` sind schlanke Mock-Header, die ausschließlich die
Typen/Funktionssignaturen bereitstellen, die die echte Datei referenziert
(`NumberInput`, `SceneManager`, `ViewDispatcher`, `LораUkfeApp`-Felder) — die echten
Header hängen an `<furi.h>`/`<gui/...>`, die auf dem Host nicht existieren. Die
GUI-Tastaturmechanik von `number_input.c` selbst wird nicht nachgebaut, sondern als
bereits produktiv verifiziert vorausgesetzt (sie ruft ihren Callback laut eigener
Implementierung nur mit einem Wert innerhalb `[min,max]` auf); der Repro-Helfer
`simulate_enter()` bildet genau das nach.

Vier Szenarien:
1. **Normalablauf**: `on_enter` zeigt zuerst "Payload-ID (0-255)" (Min/Max 0/255);
   Eingabe 42 schaltet auf "Delay ms (0-300000)" um (Min/Max 0/300000), noch nichts
   gesendet; Eingabe 1500 sendet — geprüft wird sowohl der aufgezeichnete
   `UkfeRfMessage` (`cmd=UkfeRfCmdTrigger`, `arg_len=5`, `args[0]=42`) als auch der
   volle Wire-Rundlauf über den echten `ukfe_rf_build_frame()`/`_parse_frame()`
   (Frame exakt `UKFE_RF_HDR_OVERHEAD+5` Byte, geparst `id=42`/`delay_ms=1500`),
   sowie die anschließende Navigation zu `UkfeSceneLog`.
2. **Back-Korrektur**: ID=10 eingegeben (jetzt im Delay-Schritt) → Back → zurück zur
   ID-Eingabe mit Vorbelegung 10, noch nichts gesendet → ID auf 20 überschrieben →
   Delay=500 → geprüft wird, dass der tatsächlich gesendete Frame `id=20` trägt
   (nicht die verworfene 10).
3. **Back im ID-Schritt**: `on_event` liefert `consumed=false`, keine Rekonfiguration
   ausgelöst — Beleg, dass die Standardnavigation der Scene (zurück zum
   Katalog-Menü) hier absichtlich nicht abgefangen wird.
4. **Randfall-Rundlauf**: sechs ID/Delay-Kombinationen (`{0,0}`, `{255,0}`,
   `{0,300000}`, `{255,300000}`, `{1,1}`, `{200,299999}`) je durch den vollen
   `on_enter`→ID→Delay→Senden-Ablauf, jeweils gegen den echten
   `ukfe_rf_build_frame()`/`_parse_frame()`-Rundlauf verifiziert.

```
$ gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined -I. \
      -o repro repro.c scenes/scene_trigger_confirm.c rf/ukfe_rf.c
$ ./repro
OK:   Normalablauf ID=42/Delay=1500ms -> korrekter 5-Byte-Trigger-Frame, danach UkfeSceneLog.
OK:   Back im Delay-Schritt geht zurueck zur ID-Eingabe (Vorbelegung=10), Korrektur auf 20 setzt sich im gesendeten Frame durch (nicht die 10).
OK:   Back im ID-Schritt bleibt unkonsumiert (consumed=false) -- Standardnavigation der Scene zurueck zum Katalog-Menue greift.
OK:   6 ID/Delay-Kombinationen (inkl. Randfaellen 0/255 und 0/300000-Obergrenze) verlustfrei durch den vollen Ablauf gesendet.
Alle Szenarien bestaetigt (exit code 0)
```
Keine ASan-/UBSan-Meldungen, 4/4 Szenarien bestanden.

Funktionaler Test danach lokal auf dem Flipper: Kategorie "Payloads" → "Trigger [V3]"
öffnen, eine Payload-ID eingeben (z. B. eine, die der Heltec-Agent tatsächlich kennt —
Abgleich über "Payloads" im Hauptmenü/`scene_payload_list.c`), danach einen
Verzögerungswert, mit OK bestätigen; auf der Empfängerseite (Hub/Satellit) prüfen,
dass ein vollständiger 5-Byte-`UkfeRfCmdTrigger`-Frame ankommt und die gewählte
Payload nach der gewählten Verzögerung ausgeführt wird. Zusätzlich: während der
Delay-Eingabe Back drücken, prüfen, dass die zuvor eingegebene ID vorbelegt
angezeigt wird und sich überschreiben lässt, bevor erneut bestätigt wird.

## Risiko
- Niedrig: additiv in `scene_menu.c` (ein gezielter `if`-Sonderfall vor dem
  unveränderten generischen Pfad für alle anderen Katalog-Einträge), neue eigene
  Scene-Datei, eine bereits reservierte, bislang ungenutzte Scene-ID wird gefüllt statt
  eine neue anzulegen. Kein bestehender Menüpunkt außer "Trigger [V3]" ändert sein
  Verhalten.
- `NumberInput` ist ein produktiv genutztes Flipper-Standardmodul (`gui/modules/`),
  keine Eigenentwicklung — die neue Abhängigkeit fügt nur `view_dispatcher_add_view`/
  `_remove_view` für eine zusätzliche, bereits im SDK vorhandene View hinzu.
- Die statischen Datei-Variablen `s_step`/`s_id` folgen demselben Muster wie das
  bereits vorhandene `static int s_cat` in `scene_menu.c` — kein neues
  Nebenläufigkeits-/Zustandsrisiko gegenüber dem Rest der App (Single-Threaded-GUI,
  eine Scene aktiv zur Zeit).
- `TRIGGER_DELAY_MS_MAX = 300000` ist eine reine UI-Obergrenze (deckt jeden
  realistischen Trigger-Verzug ab); das Protokoll selbst trägt weiterhin ein volles
  `uint32_t` für `delay_ms` — ein künftiger Bedarf für längere Verzögerungen ist ein
  Ein-Zeilen-Änderung an dieser Konstante, kein Protokoll- oder Strukturwechsel.
- Bewusst nicht mitgeliefert: `scene_payload_list.c` bekommt keine eigene
  Delay-ms-Eingabe (sendet weiterhin `delay_ms=0` über den separaten JSON-Pfad) — das
  wäre eine unabhängige Erweiterung eines anderen, bereits funktionierenden Menüpfads
  und nicht Teil dieses auf den Katalog-Eintrag begrenzten Fixes.

## Quelle
- `G4MEOVER18/lora-ukfe` (main, `0dba410a`): `scenes/scene_menu.c` (`send_fn()`,
  `menu_callback()`), `scenes/scenes.h` (`UkfeSceneTriggerConfirm`-Enum-Wert, bislang
  ohne eigene Handler), `scenes/scene_handlers.c` (bisheriger Alias auf
  `scene_status_*`), `scenes/scene_payload_list.c` (`ukfe_uart_send_trigger()` als
  Vergleich für den separaten JSON-Pfad), `lora_ukfe.h`/`lora_ukfe.c` (App-State,
  View-Verwaltung), `rf/ukfe_rf.h`/`.c` (`UkfeRfCmdTrigger`-Kommentar,
  `ukfe_rf_make_trigger()`), `command_catalog.h` (`FN_PAYLOAD`-Tabelle).
- `app-improvements/lora-ukfe/catalog-trigger-arg-format-mismatch-fix.md` (dieses
  Repo) — der dort dokumentierte Wire-Format-Fund und sein "Offen"-Abschnitt sind der
  unmittelbare Ausgangspunkt dieses Features.
