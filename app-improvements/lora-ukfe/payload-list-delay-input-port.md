# lora-ukfe — Payload-Liste ("Payloads") bekommt echte Delay-ms-Eingabe statt fest delay_ms=0

## Was
Ich habe den in `trigger-catalog-number-input-port.md` und
`payload-run-catalog-number-input-port.md` jeweils unter "Bewusst nicht
mitgeliefert"/"Offen" vermerkten, identischen Rückstellpunkt aufgegriffen:
`scenes/scene_payload_list.c`s `payload_callback()` ruft beim Anklicken eines
Eintrags in der vom Heltec gemeldeten Payload-Liste (JSON-Kommando
`payload_list`, eigener Menüpunkt "Payloads", **nicht** der Katalog-Pfad aus
`command_catalog.h`) bislang `ukfe_uart_send_trigger(app, id, 0)` auf —
`delay_ms` ist fest auf 0 verdrahtet, das Menü fragt den Wert nie ab. Eine
Payload aus dieser Liste auszuwählen löst sie also immer sofort aus; anders
als beim Katalog-Menüpunkt "Trigger [V3]" (der inzwischen über
`scene_trigger_confirm.c` sowohl ID als auch Delay abfragt) gab es für diesen
zweiten, unabhängigen Pfad zur selben fachlichen Aktion ("Trigger") bislang
keine Möglichkeit, überhaupt einen Delay einzugeben.

Ich habe das jetzt geschlossen: `payload_callback()` merkt sich beim Klick nur
noch die Payload-ID des angeklickten Eintrags, schaltet dann auf die
`NumberInput`-View um und fragt "Delay ms (0-300000)" ab (dieselbe
`TRIGGER_DELAY_MS_MAX`-Konvention und derselbe Wert wie in
`scene_trigger_confirm.c` — als eigenständiges `#define` in dieser Datei
wiederholt, da C-Makros nicht über Übersetzungseinheiten hinweg geteilt
werden; beide Dateien bleiben inhaltlich synchron). Erst der
Ergebnis-Callback der Eingabe sendet `ukfe_uart_send_trigger(app, id,
delay_ms)` mit der tatsächlich eingegebenen Verzögerung und geht danach —
genau wie der bisherige Code direkt nach dem Sofortversand — über
`scene_manager_previous_scene()` zurück ins Menü.

Anders als bei den beiden Katalog-Funden handelt es sich hier **nicht** um
eine neue Scene: die Payload-Liste ist bereits eine eigene Scene mit zwei
fachlich zusammengehörigen Ansichten (Liste der Payloads / Delay-Eingabe für
die gerade ausgewählte), und es gab — anders als bei den Katalog-Einträgen —
keine bereits reservierte, ungenutzte Scene-ID, die sich dafür angeboten
hätte. Die einfachste, in sich geschlossene Lösung ist ein View-Wechsel
innerhalb der bestehenden Scene (`UkfeViewMenu` → `UkfeViewNumberInput` →
zurück), nicht eine weitere Scene mit eigenem Scene-Manager-Zustand für einen
einzigen Zwischenschritt.

## Nutzen
- Macht die Payload-Liste ("Payloads"-Menü) für jeden gewünschten
  Verzögerungswert (0–300000 ms) nutzbar — bisher war ausschließlich
  `delay_ms=0` (Sofortauslösung) über diesen Pfad erreichbar.
- Baut auf der bereits von `trigger_catalog_number_input.patch` eingebundenen
  `NumberInput`-Infrastruktur auf (`app->number_input`,
  `UkfeViewNumberInput` in `lora_ukfe.h`/`.c`) — keine erneute
  View-/Modul-Registrierung nötig, nur ein zusätzlicher View-Wechsel innerhalb
  einer bereits bestehenden Scene.
- Rührt am Transport nichts an: `ukfe_uart_send_trigger()` (`uart_comm.c`) ist
  unverändert und baut das JSON für jeden `delay_ms`-Wert bereits korrekt
  (`delay_ms`-Feld nur bei `delay_ms>0`, sonst reines `{"cmd":"trigger","id":N}`
  — siehe `trigger-catalog-number-input-port.md`). Dieser Fix ändert
  ausschließlich, **woher** der `delay_ms`-Wert kommt (Nutzereingabe statt
  fest 0), nicht wie er kodiert wird.
- Additiv und eng auf `payload_callback()` begrenzt: `build_list()`, die
  `0x12`-Refresh- und `0x11`-Ack-Behandlung in `scene_payload_list_on_event()`
  sowie `on_enter`/`on_exit` bleiben unverändert. Jeder andere Menüpunkt bzw.
  jede andere Scene ändert ihr Verhalten nicht.
- Schließt denselben "Offen"-Punkt, der in zwei vorherigen Sitzungen jeweils
  bewusst zurückgestellt wurde ("eine unabhängige Erweiterung eines anderen,
  bereits funktionierenden Menüpfads") — jetzt nachgezogen, nachdem die dafür
  nötige `NumberInput`-Infrastruktur bereits vorhanden ist.

## Betroffene Dateien
- `scenes/scene_payload_list.c` — einzige geänderte Datei. `payload_callback()`
  sendet nicht mehr selbst, sondern merkt sich die Payload-ID
  (`s_selected_id`, nach demselben Muster wie `s_id`/`s_step` in
  `scene_trigger_confirm.c` bzw. `s_cat` in `scene_menu.c` — Datei-statische
  Variable, unkritisch im Single-Threaded-GUI-Modell mit je einer aktiven
  Scene) und schaltet auf `UkfeViewNumberInput` um; neue Funktion
  `payload_delay_cb()` sendet nach Bestätigung der Eingabe und navigiert
  danach zurück. `build_list()`, `scene_payload_list_on_enter/_on_event/_on_exit()`
  bleiben Zeile für Zeile unverändert.

Keine Änderung an `lora_ukfe.h`/`.c` (die `NumberInput`-Infrastruktur
existiert bereits durch `trigger_catalog_number_input.patch`),
`scenes/scenes.h`/`scene_handlers.c` (keine neue Scene-ID, kein neuer
Handler-Eintrag nötig), `scenes/scene_menu.c` (unabhängiger Katalog-Pfad,
unberührt), `uart_comm.c` (`ukfe_uart_send_trigger()` war bereits korrekt) oder
`command_catalog.h`.

## Port-Skizze / Patch
**Setzt auf dem bereits vorgeschlagenen
`trigger-catalog-number-input-port/trigger_catalog_number_input.patch` auf**
(frühere Sitzung, noch nicht upstream gemerged) — dieser Patch hier ist gegen
eine Basis erstellt, auf der jener bereits angewendet wurde, da er dessen
`NumberInput`-Infrastruktur (`app->number_input`, `UkfeViewNumberInput`)
wiederverwendet. **Er setzt nicht** auf
`payload-run-catalog-number-input-port/payload_run_catalog_number_input.patch`
auf (unabhängiger Katalog-Pfad, keine gemeinsame Datei) — beide Patches lassen
sich unabhängig voneinander auf `trigger_catalog_number_input.patch` aufsetzen
und miteinander kombinieren, berühren aber unterschiedliche Dateien
(`scenes/scene_payload_run_confirm.c` + `scene_menu.c`/`scene_handlers.c`/
`scenes.h` dort, ausschließlich `scenes/scene_payload_list.c` hier).

Lokale Anwendungsreihenfolge: zuerst `trigger_catalog_number_input.patch`,
danach `payload-list-delay-input-port/payload_list_delay_input.patch`. Beide
zusammen `git apply --check` gegen eine frische Kopie der unveränderten
`G4MEOVER18/lora-ukfe`-Basis (Commit `0dba410a`) verifiziert, real angewendet
und byte-für-byte mit der Arbeitskopie abgeglichen — identisch.

Vollständiger Unified-Diff (gegen den Stand nach
`trigger_catalog_number_input.patch`) in
`payload-list-delay-input-port/payload_list_delay_input.patch`.

Kernstück, `scenes/scene_payload_list.c`:
```c
#define TRIGGER_DELAY_MS_MAX 300000   // dieselbe Konvention/derselbe Wert wie
                                       // in scene_trigger_confirm.c

static uint8_t s_selected_id;

static void payload_delay_cb(void* ctx, int32_t number) {
    LораUkfeApp* app = ctx;
    ukfe_uart_send_trigger(app, s_selected_id, (uint32_t)number);

    // Kurzes visuelles Feedback → dann zurück ins Menü
    scene_manager_previous_scene(app->scene_manager);
}

static void payload_callback(void* ctx, uint32_t index) {
    LораUkfeApp* app = ctx;
    if(index >= app->payload_count) return;

    s_selected_id = app->payloads[index].id;

    number_input_set_header_text(app->number_input, "Delay ms (0-300000)");
    number_input_set_result_callback(
        app->number_input, payload_delay_cb, app, 0, 0, TRIGGER_DELAY_MS_MAX);
    view_dispatcher_switch_to_view(app->view_dispatcher, UkfeViewNumberInput);
}
```
`build_list()`, `scene_payload_list_on_enter()`, `scene_payload_list_on_event()`
und `scene_payload_list_on_exit()` bleiben dabei unverändert (siehe Patch für
den vollständigen Kontext-Diff).

**Entscheidung zu Back während der Delay-Eingabe:** `scene_payload_list_on_event()`
behandelt `SceneManagerEventTypeBack` bereits vor diesem Fix unkonditional —
unabhängig davon, welche View gerade sichtbar ist, ruft es
`scene_manager_previous_scene()` auf und liefert `consumed=true` (verlässt
also die gesamte Scene). Ich habe das bewusst **nicht** geändert: Back während
der Delay-Eingabe verlässt damit ebenfalls direkt die Payload-Liste (zurück
zum vorherigen Menü) und verwirft die begonnene Eingabe, statt nur einen
Schritt zur Liste zurückzugehen. Das ist dieselbe "einfachste Lösung für den
eigenen Einschritt-Fall", die `scene_payload_run_confirm.c` bereits gewählt
hat (Back nicht speziell behandeln, Standardverhalten durchlaufen lassen) —
hier fällt sie mir sogar kostenlos zu, da das bestehende, unveränderte
`on_event()` bereits exakt dieses Verhalten liefert und keine Fallunterscheidung
nach aktueller View braucht. Eine Alternative (Back während der Delay-Eingabe
geht nur zurück zur Liste, analog zum zweistufigen ID→Delay-Schritt in
`scene_trigger_confirm.c`) wäre möglich, hätte aber eine zusätzliche
View-Tracking-Variable und eine Fallunterscheidung in `on_event()` nötig
gemacht — für einen reinen Bedienungskomfort-Punkt, der hier bewusst
zurückgestellt wird (siehe "Offen").

`number_input_set_header_text()`/`_set_result_callback()` sind in der echten
`number_input.c` so gebaut, dass ein erneuter Aufruf das View-Model komplett
neu befüllt (Header, Textpuffer, Callback, Min/Max) und einen Redraw auslöst
— jeder erneute Klick auf einen Payload-Eintrag (nach Back oder nach einem
abgeschlossenen Versand und erneutem Öffnen der Liste) setzt die Eingabe also
korrekt frisch auf, ohne Reststand vom vorherigen Durchlauf.

## Testhinweis (ohne Hardware)
Reine Logik/State, vollständig ohne Flipper/CC1101/Heltec/UART nachstellbar —
`repro-payload-list-delay-input/repro.c` kompiliert dafür **die echte,
unveränderte** `scenes/scene_payload_list.c` dieser Sitzung direkt mit (keine
Transkription). Nur `lora_ukfe.h`/`scenes/scenes.h` sind schlanke Mock-Header
(nach demselben Muster wie in `repro-trigger-catalog-number-input/` und
`repro-payload-run-catalog-number-input/`), die ausschließlich die
Typen/Funktionssignaturen bereitstellen, die die echte Datei referenziert
(`NumberInput`, `Submenu`, `SceneManager`, `ViewDispatcher`,
`LораUkfeApp`-Felder) — die echten Header hängen an `<furi.h>`/`<gui/...>`,
die auf dem Host nicht existieren. `ukfe_uart_send_trigger()` selbst
(`uart_comm.c`, unverändert) hängt an `furi_hal_serial_tx()`/
`ukfe_log_append()` und wird daher, genau wie `ukfe_uart_send_rf()` in den
beiden vorherigen lora-ukfe-Repros dieses Verzeichnisses, als aufzeichnender
Stub nachgebildet statt mitkompiliert — ihr (bereits an anderer Stelle
dokumentiertes und durch diesen Fix unangetastetes) JSON-Format wird hier
nicht erneut verifiziert, geprüft wird ausschließlich, dass
`scene_payload_list.c` sie mit der richtigen Payload-ID und dem richtigen
`delay_ms` aufruft. Die GUI-Mechanik von `number_input.c`/`submenu.c` selbst
(Tastatur- bzw. Listen-Navigation) wird nicht nachgebaut, sondern als bereits
produktiv verifiziert vorausgesetzt — `simulate_select()`/
`simulate_delay_enter()` bilden genau das nach (Aufruf des zuletzt
registrierten Item- bzw. Ergebnis-Callbacks mit einem gültigen Wert).

Acht Szenarien:
1. **Normalablauf**: Liste mit drei Einträgen aufgebaut (`id`≠Index bewusst
   gewählt, um zu belegen, dass die `id` und nicht der Index verwendet wird);
   Klick auf Eintrag 1 (`id=42`) schaltet auf "Delay ms (0-300000)" um
   (Min/Max 0/300000), noch nichts gesendet, noch kein `previous_scene()`;
   Eingabe 2500 sendet `ukfe_uart_send_trigger(id=42, delay_ms=2500)` und erst
   danach `previous_scene()`.
2. **delay_ms=0-Randfall**: wird weiterhin gesendet (nicht übersprungen), jetzt
   über die explizite Eingabe statt stillschweigend fest verdrahtet.
3. **Obergrenze delay_ms=300000**: wird unverändert durchgereicht.
4. **index >= payload_count**: `payload_callback()` kehrt unverändert sofort
   zurück — kein View-Wechsel, keine Delay-Eingabe, kein Sendeversuch
   (Regressions-Check auf das bereits vor diesem Fix bestehende
   Bounds-Check-Verhalten, z. B. bei einem veralteten Callback-Aufruf kurz vor
   einem die Liste verkleinernden `0x12`-Refresh).
5. **0x12-Refresh/0x11-Ack unberührt**: ein Payload wird ausgewählt (Delay-
   Eingabe läuft), währenddessen trifft ein `0x12`-Event mit einer geänderten
   Liste ein — `build_list()` läuft unverändert (neue Liste übernommen), die
   laufende Delay-Eingabe für den *zuvor* ausgewählten Eintrag bleibt
   unbeeinflusst und sendet beim Bestätigen weiterhin dessen ursprüngliche
   `id`; `0x11` bleibt ebenfalls konsumiert.
6. **Back während der Delay-Eingabe**: verlässt die Scene (bestehendes,
   unverändertes `on_event()`-Verhalten, siehe Entscheidung oben) und verwirft
   die begonnene Eingabe, ohne zu senden.
7. **on_exit() mit aktiver NumberInput-View**: `submenu_reset()` läuft
   unverändert, auch wenn die Scene mit unbestätigter Delay-Eingabe verlassen
   wird — kein Hängenbleiben/undefinierter Zustand.
8. **Randfall-Rundlauf**: sechs ID/Delay-Kombinationen (`{0,0}`, `{255,0}`,
   `{0,300000}`, `{255,300000}`, `{1,1}`, `{200,299999}`) je durch den vollen
   `on_enter`→Auswahl→Delay→Senden-Ablauf.

```
$ gcc -std=c11 -O0 -Wall -Wextra -fsanitize=address,undefined -I. \
      -o repro repro.c scenes/scene_payload_list.c
$ ./repro
OK:   Normalablauf: Payload id=42 ausgewaehlt, Delay=2500ms eingegeben -> ukfe_uart_send_trigger(id=42, delay_ms=2500), danach erst previous_scene().
OK:   delay_ms=0 wird weiterhin gesendet (wie zuvor bei Sofortversand), jetzt ueber die explizite Delay-Eingabe statt stillschweigend fest verdrahtet.
OK:   Obergrenze delay_ms=300000 (TRIGGER_DELAY_MS_MAX) wird unveraendert durchgereicht.
OK:   index >= payload_count: payload_callback() kehrt unveraendert sofort zurueck, keine Delay-Eingabe, kein Sendeversuch.
OK:   0x12-Refresh (build_list()) und 0x11-trigger_ack bleiben unveraendert funktionsfaehig, auch waehrend einer laufenden Delay-Eingabe fuer einen zuvor ausgewaehlten Eintrag.
OK:   Back waehrend der Delay-Eingabe verlaesst die Scene (bestehendes, unveraendertes on_event()-Verhalten) und verwirft die begonnene Eingabe, ohne zu senden.
OK:   on_exit() raeumt (submenu_reset()) unveraendert auf, auch wenn die Scene mit aktiver NumberInput-View (unbestaetigte Delay-Eingabe) verlassen wird.
OK:   6 ID/Delay-Kombinationen (inkl. Randfaellen 0/255 und 0/300000-Obergrenze) verlustfrei durch den vollen Ablauf gesendet.
Alle Szenarien bestaetigt (exit code 0)
```
Identisch bei `-O1` (zweiter Lauf, dieselben acht "OK"-Zeilen, kein
abweichendes Verhalten). Keine ASan-/UBSan-Meldungen, 8/8 Szenarien bestanden,
keine Compiler-Warnungen bei `-Wall -Wextra`.

Funktionaler Test danach lokal auf dem Flipper: "Payloads" öffnen (Liste wird
vom Heltec geladen), einen Eintrag anklicken, auf der Delay-Eingabe einen Wert
eintippen (z. B. 3000) und mit OK bestätigen; auf der Empfängerseite prüfen,
dass das JSON-Kommando mit `"delay_ms":3000` ankommt und die Payload nach der
gewählten Verzögerung ausgeführt wird (nicht mehr sofort). Zusätzlich: Back
während der Delay-Eingabe drücken, prüfen, dass die Liste verlassen wird und
nichts ausgelöst wurde (siehe "Entscheidung" oben); danach "Payloads" erneut
öffnen und einen Eintrag mit `delay_ms=0` bestätigen, prüfen, dass weiterhin
sofort ausgelöst wird.

## Risiko
- Niedrig: ausschließlich additiv in einer einzigen Datei
  (`scenes/scene_payload_list.c`), kein neuer Scene-Eintrag, kein neuer
  Handler, kein geändertes Wire-/JSON-Format. `build_list()` und die
  `0x11`-/`0x12`-Event-Behandlung sind byte-identisch zum bisherigen Stand.
- Die Datei-statische Variable `s_selected_id` folgt demselben Muster wie
  `s_id`/`s_step` in `scene_trigger_confirm.c` bzw. `s_cat` in
  `scene_menu.c` — kein neues Nebenläufigkeits-/Zustandsrisiko gegenüber dem
  Rest der App (Single-Threaded-GUI, eine Scene aktiv zur Zeit).
- `TRIGGER_DELAY_MS_MAX = 300000` ist als eigenständiges `#define` in dieser
  Datei wiederholt (C-Makros werden nicht über Übersetzungseinheiten hinweg
  geteilt) — identischer Name und Wert wie in `scene_trigger_confirm.c`,
  damit beide Eingaben für Nutzer dieselbe Obergrenze zeigen. Ein künftiger
  Bedarf für eine andere Grenze müsste an beiden Stellen angepasst werden;
  das in `lora_ukfe.h` als gemeinsame Konstante zu verschieben, wäre eine
  rein kosmetische Aufräumarbeit ohne Verhaltensänderung und nicht Teil
  dieses Fixes.
- Abhängigkeit: setzt voraus, dass `trigger_catalog_number_input.patch`
  bereits angewendet wurde (liefert `app->number_input`/
  `UkfeViewNumberInput`). Ohne diesen fehlt die Voraussetzung — beide Patches
  gehören lokal zusammen angewendet. Unabhängig davon, ob
  `payload_run_catalog_number_input.patch` ebenfalls angewendet ist oder
  nicht (keine gemeinsame Datei, keine Wechselwirkung).
- Bewusst nicht mitbehoben (siehe "Offen"): Back während der Delay-Eingabe
  verlässt die gesamte Payload-Liste statt nur zur Liste zurückzugehen.

## Offen (bewusst nicht mitbehoben)
- Back während der Delay-Eingabe verwirft nicht nur die Eingabe, sondern
  verlässt die gesamte Payload-Liste-Scene (siehe "Entscheidung" oben) — ein
  Zurück-zur-Liste-Schritt analog zum zweistufigen `scene_trigger_confirm.c`
  wäre möglich, aber eine zusätzliche, über den reinen Bedienungskomfort
  hinausgehende Erweiterung, die hier bewusst zurückgestellt wird.
- Beim erneuten Öffnen von "Payloads" nach einem `0x12`-Refresh, der einen
  bereits angeklickten Eintrag aus der Liste entfernt (ID wechselt den
  Besitzer/verschwindet beim Hub), bleibt die laufende Delay-Eingabe für die
  *alte* ID bestehen und sendet beim Bestätigen weiterhin dorthin (siehe
  Szenario 5) — fachlich dieselbe Grenze, die auch das bisherige
  Sofort-Senden schon hatte (die App hat ohnehin keine Möglichkeit, hier eine
  stattgefundene Hub-seitige ID-Neuvergabe zu erkennen); keine Verschlechterung
  gegenüber dem Vorzustand, daher nicht Teil dieses Fixes.
- `command_catalog.h`/der Katalog-Menü-Pfad (`scene_menu.c`,
  `scene_trigger_confirm.c`, `scene_payload_run_confirm.c`) bleiben
  unverändert — wie in beiden vorherigen Sitzungen dokumentiert, bleiben
  Katalog-Pfad und JSON-Direktmodus-Pfad zwei unabhängige, nebeneinander
  bestehende Transportarten für dieselbe fachliche Aktion.

## Quelle
- `G4MEOVER18/lora-ukfe` (main, `0dba410a`): `scenes/scene_payload_list.c`
  (`payload_callback()`, `build_list()`, Event-Handling), `uart_comm.c`
  (`ukfe_uart_send_trigger()`, unverändert), `lora_ukfe.h`
  (`UkfePayloadEntry`/`app->payloads[]`/`app->payload_count` als Kontext).
- `app-improvements/lora-ukfe/trigger-catalog-number-input-port.md` (dieses
  Repo, vorherige Sitzung) — Basis-Infrastruktur (`NumberInput`-View),
  identisches Lösungsmuster (Header/Min/Max/Ergebnis-Callback) und der
  ursprüngliche Fundort dieses "Bewusst nicht mitgeliefert"-Punkts.
- `app-improvements/lora-ukfe/payload-run-catalog-number-input-port.md`
  (dieses Repo, vorherige Sitzung) — wiederholt denselben Rückstellpunkt für
  den Katalog-Pfad und grenzt ihn explizit vom hier bearbeiteten
  JSON-Direktmodus-Pfad ab.
