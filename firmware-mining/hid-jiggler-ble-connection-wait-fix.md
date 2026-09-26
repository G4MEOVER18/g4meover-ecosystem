# HID Mouse Jiggler (beide Varianten): Sendet weiter, ohne auf die BLE-Verbindung zu warten

## Was
Ich habe den letzten verbliebenen Teilfund aus der `8fdb70dd3`-Serie (PR #1112, "HID: improve
Mouse Jiggler timers and Bluetooth controls", `unlshd`-`dev`) aufgegriffen: "Jiggler wartet über
BLE nicht auf die Verbindung vor dem Senden". Alle fünf vorherigen Teilfunde aus demselben Commit
(Stop-Timer-Fix, Exit-Reset-Fix, Stealth-Exit-Neubewaffnung-Race, Stealth-Bewegung kann 0 sein,
BLE-Callback-Registrierungsreihenfolge) sind bereits in früheren Sitzungen bearbeitet — dieser
war zuletzt offen. Betrifft beide Jiggler-Varianten:
`applications/system/hid_app/views/hid_mouse_jiggler.c` (nicht-stealth) und
`hid_mouse_jiggler_stealth.c` (stealth), jeweils den Timer-Callback und den Draw-Callback, sowie
neu `applications/system/hid_app/hid.h` für eine kleine gemeinsame Hilfsmakro-Definition.

## Fund: Timer-Callback prüft `model->connected` nicht, obwohl das Feld bereits existiert und aktuell gehalten wird
Beide Modelle tragen bereits ein `connected`-Feld:
```c
typedef struct {
    bool connected;
    bool running;
    int interval_idx;
    uint8_t counter;
} HidMouseJigglerModel;
```
gepflegt von `hid_ble_app()`s `bt_hid_connection_status_changed_callback()` in `hid.c` über
`hid_mouse_jiggler_set_connected_status()` bzw. `hid_mouse_jiggler_stealth_set_connected_status()`
— diese Infrastruktur ist bei mir (`mntm-012`) bereits vollständig vorhanden und verdrahtet, kein
eigener Fund hier. Beide Draw-Callbacks lesen `model->connected` auch schon, aber ausschließlich,
um zwischen `I_Ble_connected_15x15`/`I_Ble_disconnected_15x15` zu wählen:
```c
#ifdef HID_TRANSPORT_BLE
    if(model->connected) {
        canvas_draw_icon(canvas, 0, 0, &I_Ble_connected_15x15);
    } else {
        canvas_draw_icon(canvas, 0, 0, &I_Ble_disconnected_15x15);
    }
#endif
```
Die beiden Timer-Callbacks lesen `model->connected` dagegen an keiner Stelle — weder über BLE noch
über USB, wo das Feld ohnehin nicht existiert:
```c
// hid_mouse_jiggler.c, Timer-Callback (nicht-stealth)
if(model->running) {
    model->counter++;
    hid_hal_mouse_move(hid_mouse_jiggler->hid, ..., 0);
}
```
```c
// hid_mouse_jiggler_stealth.c, Timer-Callback (stealth)
if(model->running) {
    // Intervall/Bewegung berechnen ...
    hid_hal_mouse_move(hid_mouse_jiggler->hid, move_x, move_y);
    furi_timer_stop(hid_mouse_jiggler->timer);
    furi_timer_start(hid_mouse_jiggler->timer, randomIntervalMinutes * 60000);
}
```
**Praktische Auswirkung (nur über BLE relevant, USB hat immer eine physische Verbindung):** Ist
der Jiggler gestartet, während die BLE-Verbindung noch nicht steht — Gerät gerade erst beworben,
Host außer Reichweite, Host schläft/startet neu, Pairing noch offen — ruft der Timer-Callback
`hid_hal_mouse_move()` trotzdem in jedem Takt auf. `hid_hal_mouse_move()` schreibt lokal in die
HID-Characteristic; ohne abonnierten/verbundenen Host gibt es dabei keinen Transportfehler, der
Aufruf verläuft einfach ins Leere. Der Jiggler zeigt zwar bereits das (bestehende) BLE-Symbol,
aber keinen eigenen Hinweis auf den laufenden-aber-wirkungslosen Zustand — der alleinige Zweck
der App (Inaktivität am Host verhindern) ist in dieser Zeit ohne jede Rückmeldung nicht erfüllt.

## Host-Repro
Eigenständiges Host-Repro (keine Flipper-/Hardware-Abhängigkeiten). Modelliert die
Sende-Entscheidung beider Timer-Callbacks (ALT: nur `model->running`; NEU: `model->running &&
hid_model_connected(model)`) als reine Funktion von `(build_is_ble, running, model_connected)` und
prüft erschöpfend alle `2×2×2 = 8` Kombinationen, für beide Jiggler-Varianten (da beide exakt
dieselbe Gate-Form haben — im Repro selbst nachgeprüft, nicht nur angenommen):
```
gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
./repro
```
Tatsächliche Ausgabe (lokal ausgeführt, identisch unter `-O0` und `-O1`):
```
[group1] 16/16 rows checked (2 variants x 2x2x2 exhaustive: build x running x connected)
[group2] ALT sends into a disconnected BLE link: 2/2 running-BLE-disconnected rows (bug reproduced on both variants)
[group3] NEU sends into a disconnected BLE link: 0/2 running-BLE-disconnected rows (must be 0 on both variants)
[group4] NEU differs from ALT on a USB build: 0/8 rows (must be 0 -- USB has no link to wait for)
ALL CHECKS PASSED
```
Gruppe 2 bestätigt den Fund: ALT sendet in beiden Varianten in jeder der zwei
`running && BLE && !connected`-Zeilen (der volle relevante Zustandsraum für diesen Fund, nicht
nur eine Stichprobe). Gruppe 3 bestätigt, dass NEU das in keiner dieser Zeilen mehr tut. Gruppe 4
bestätigt, dass sich NEU auf einem USB-Build in keiner der acht Zeilen von ALT unterscheidet —
der Fix ist eine rein BLE-bezogene Verhaltensänderung, USB bleibt byte-für-byte gleich. Unter
`gcc -std=c11 -O0/-O1 -Wall -Wextra -fsanitize=address,undefined`: keine ASan-/UBSan-Meldungen,
keine Compiler-Warnungen.

## Betroffene Dateien
- `applications/system/hid_app/hid.h` — neues Makro `hid_model_connected(model)` direkt nach den
  bestehenden `bt_hid_*`-Deklarationen. Reine Ergänzung, keine bestehende Deklaration verändert.
- `applications/system/hid_app/views/hid_mouse_jiggler.c` —
  `hid_mouse_jiggler_timer_callback()` (Sende-Bedingung erweitert) und
  `hid_mouse_jiggler_draw_callback()` (Text bei laufend-aber-getrennt). Unabhängig von den beiden
  bereits gelieferten Fixes derselben Datei (Stop-Timer-Fix, Exit-Reset-Fix — andere
  Funktionen/Zeilen, kein Überlapp); Patch hier gegen die unveränderte `mntm-012`-Basis.
- `applications/system/hid_app/views/hid_mouse_jiggler_stealth.c` —
  `hid_mouse_jiggler_stealth_timer_callback()` (Sende-Bedingung erweitert) und
  `hid_mouse_jiggler_stealth_draw_callback()` (Text bei laufend-aber-getrennt). Setzt die beiden
  bereits vorgeschlagenen `hid-jiggler-stealth-move-bounds-fix` und
  `hid-jiggler-stealth-zero-movement-fix` voraus (siehe "Port-Skizze / Fix" unten zur
  Anwendungsreihenfolge); unabhängig vom ebenfalls bereits gelieferten
  `hid-jiggler-stealth-exit-rearm-race-fix` (andere Funktion, kein Überlapp).

## Risiko
Gering. Keine geänderte Funktionssignatur außer dem rein additiven Makro in `hid.h`; auf
USB-Builds (kein `HID_TRANSPORT_BLE`) löst das Makro zu `(true)` auf, die Sende-Bedingung ist dort
also exakt `model->running` wie vorher — durch das Host-Repro (Gruppe 4) explizit auf
Byte-Gleichheit mit ALT geprüft, nicht nur behauptet. Auf BLE-Builds wird die Bewegung bei
laufend-aber-getrennt jetzt übersprungen, statt wirkungslos gesendet zu werden — kein neuer
Fehlerpfad, keine Änderung an Locking/Timer-Handling. Bei der stealth-Variante bleibt der Timer
weiterhin `FuriTimerTypePeriodic`; wird der Tick bei getrennter Verbindung übersprungen, feuert
der Timer einfach mit seinem zuletzt gesetzten Intervall weiter und prüft `connected` beim
nächsten Tick erneut — kein zusätzlicher Rearm-Aufruf nötig, keine Interaktion mit dem bereits
gelieferten Exit-Neubewaffnung-Race-Fix (der eine andere Funktion, `..._exit_callback()`,
betrifft). Nicht 1:1 der volle Unleashed-Commit `8fdb70dd3`: bewusst nicht mitübernommen sind
dessen größere Restrukturierungen (Wechsel der stealth-Variante auf `FuriTimerTypeOnce` mit
Selbst-Rearm unter Lock, Verschieben von `hid_hal_mouse_move()`/`rand()` aus dem Model-Lock heraus
bei beiden Varianten, Umbau der nicht-stealth Timer-Logik auf Bool-Flags außerhalb des Locks) —
das sind eigenständige, unabhängig zu bewertende Änderungen ohne Bezug zu diesem konkreten Fund.

## Port-Skizze / Fix
`hid.h`, direkt nach den bestehenden `bt_hid_remove_pairing`/`bt_hid_save_cfg`-Deklarationen:
```c
// Only BLE has a link to wait for, and a view model field cannot be read behind an #ifdef
// inside a with_view_model() argument list.
#ifdef HID_TRANSPORT_BLE
#define hid_model_connected(model) ((model)->connected)
#else
#define hid_model_connected(model) (true)
#endif
```
1:1 Unleasheds Makro übernommen (identischer Zweck: BLE-Ifdef nicht direkt im
`with_view_model()`-Argument verwenden können, da dessen Makro-Expansion das Modell nur innerhalb
des generierten Blocks kennt).

`hid_mouse_jiggler.c`, Timer-Callback — einzige geänderte Zeile ist die Bedingung selbst:
```c
if(model->running && hid_model_connected(model)) {
    model->counter++;
    hid_hal_mouse_move(...);
}
```
Draw-Callback — bestehende `"Press Start\nto jiggle"`-Zeile durch eine BLE-bedingte Auswahl
ersetzt (identisches Muster zu der schon vorhandenen Icon-Auswahl direkt darüber):
```c
#ifdef HID_TRANSPORT_BLE
    if(model->running && !model->connected) {
        elements_multiline_text(canvas, AlignLeft, 40, "Waiting for\nConnection...");
    } else {
        elements_multiline_text(canvas, AlignLeft, 40, "Press Start\nto jiggle");
    }
#else
    elements_multiline_text(canvas, AlignLeft, 40, "Press Start\nto jiggle");
#endif
```
Unified-Diffs (`git apply --check` vorab je gegen eine frische `mntm-012`-Kopie verifiziert, real
angewendet und Byte-für-Byte gegen die beabsichtigte Endfassung abgeglichen) in
`firmware-mining/hid-jiggler-ble-connection-wait-fix/hid_h.patch` und
`firmware-mining/hid-jiggler-ble-connection-wait-fix/hid_mouse_jiggler_c.patch`.

`hid_mouse_jiggler_stealth.c` — dieselbe Bedingungserweiterung und derselbe Text-Ersatz, an den
analogen Stellen. Anwendungsreihenfolge für die stealth-Datei: zuerst
`hid-jiggler-stealth-move-bounds-fix`, danach `hid-jiggler-stealth-zero-movement-fix`, danach
dieser Patch hier — der Diff ist gegen den Zustand nach den ersten beiden erstellt (nicht gegen
die unveränderte `mntm-012`-Basis), da alle drei denselben Codeblock (Bewegungserzeugung im
Timer-Callback) berühren. `git apply --check -p1` gegen eine frische Kopie mit bereits
angewendeten Vorgänger-Patches vorab verifiziert, real angewendet und Byte-für-Byte gegen die
beabsichtigte Endfassung abgeglichen (volle Kette
Pristine→Bounds-Fix→Zero-Movement-Fix→dieser Fix geprüft) in
`firmware-mining/hid-jiggler-ble-connection-wait-fix/hid_mouse_jiggler_stealth_c.patch`.

## Testhinweis (lokal nachzuvollziehen)
`firmware-mining/repro-hid-jiggler-ble-connection-wait-fix/repro.c`, host-kompilierbar, keine
Flipper-/Hardware-Abhängigkeiten (Befehl und tatsächliche Ausgabe siehe Host-Repro oben).

Für den lokalen Funktionstest an echter Hardware: `hid.h`- und `hid_mouse_jiggler.c`-Patch können
unabhängig angewendet werden; für `hid_mouse_jiggler_stealth.c` zuerst
`hid-jiggler-stealth-move-bounds-fix` und `hid-jiggler-stealth-zero-movement-fix` anwenden (siehe
oben), danach `fbt` neu bauen und flashen. Regressionsprüfung über USB-HID (kein BLE beteiligt):
beide Jiggler-Varianten wie bisher starten, keine Verhaltensänderung erwartet. Funktionsprüfung
über BLE: Host-Pairing trennen bzw. Host außer Reichweite bringen, Jiggler starten — Anzeige
wechselt auf "Waiting for Connection...", keine Mausbewegung am (getrennten) Host beobachtbar;
nach Wiederverbinden (BLE-Symbol wechselt auf "connected") setzt die Bewegung automatisch wieder
ein, ohne den Jiggler neu starten zu müssen.

## Quelle
- `github.com/DarkFlippers/unleashed-firmware`, Commit `8fdb70dd3` (PR #1112, "HID: improve Mouse
  Jiggler timers and Bluetooth controls"), `unlshd`-`dev` — nur die Teilverbesserung "the jiggler
  waits for the link instead of sending into nothing" 1:1 übernommen (Sende-Bedingung und
  Warte-Anzeige), nicht die begleitenden größeren Restrukturierungen (siehe "Risiko" oben).
- Zum Vergleich herangezogen: `github.com/Next-Flip/Momentum-Firmware`, Tag `mntm-012`
  (`applications/system/hid_app/hid.h`, `views/hid_mouse_jiggler.c`,
  `views/hid_mouse_jiggler_stealth.c`) — bestätigt, dass `model->connected` und die
  `_set_connected_status()`-Verdrahtung bereits vollständig vorhanden sind und nur die
  Sende-/Anzeige-Prüfung selbst fehlt.
- Voraussetzung für die stealth-Datei: `firmware-mining/hid-jiggler-stealth-move-bounds-fix.md`
  und `firmware-mining/hid-jiggler-stealth-zero-movement-fix.md` (müssen zuerst angewendet
  werden).
- Damit ist die gesamte aus `8fdb70dd3` (PR #1112) zurückgestellte Fundliste abgearbeitet — kein
  weiterer Teilfund aus diesem Commit offen.
