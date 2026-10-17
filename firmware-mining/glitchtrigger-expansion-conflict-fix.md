# Glitch Trigger — externe App von at0m-b0mb (Fault-Injection-Trigger-Generator)

## Was

Neue Kategorie "Fault Injection / Glitching" aufgegriffen (siehe
`ideas/glitchtrigger-fault-injection-app-status.md` für die vollständige Werkzeugbeschreibung)
und dabei im einzigen UART-nutzenden Pfad der App denselben Ressourcenkonflikt gefunden, den ich
bereits bei UART Sniff/RS485 Modbus/GPIO Logic Analyzer/SWD Probe/Argus/CAN Commander dokumentiert
habe: **Glitch Trigger** (`at0m-b0mb/GlitchTrigger-FlipperZero`, App-ID `glitch_trigger`) ruft
nirgends `expansion_disable()`/`expansion_enable()` auf, obwohl `helpers/glitch_serial.c` für die
"UART"-Feedback-Quelle der Sweep-Kampagne `furi_hal_serial_control_acquire(FuriHalSerialIdUsart)`
nutzt — exakt das UART, das der Flipper-eigene Expansion-Systemdienst standardmäßig überwacht.

## Nutzen für mich

Direkt verwertbare, kleine Code-Korrektur in einem neu katalogisierten Werkzeug — schließt den
Fund in der Kategorie ab, bevor ich das Gerät lokal baue. Zusätzlich ein eigenständiger,
unabhängiger Zweitfund (stille Versionsnummer-Regression im selben Release-Commit, siehe unten)
im selben Patch mitbehoben, da beide Änderungen dieselbe Datei (`glitch_trigger_i.h`) betreffen
und beide additiv/risikofrei sind.

## API-87-Kompatibilität (mntm-012)

Gegen eine frische, unabhängige Zweitkopie von `G4MEOVER18/G4MEOVER-FW` (Tag `mntm-012`, Commit
`b359f6f`) verifiziert — vollständige Liste in
`ideas/glitchtrigger-fault-injection-app-status.md`. Zusätzlich hier relevant:
`applications/services/expansion/expansion.h` dokumentiert die geforderte Aufruf-Reihenfolge
selbst normativ im Doc-Kommentar von `expansion_disable()`:

> *"Applications requiring serial port access MUST call this function BEFORE calling
> furi_hal_serial_control_acquire(). Similarly, an expansion_enable() call MUST be made right
> AFTER a call to furi_hal_serial_control_release() to ensure that the user settings are
> properly restored."*

Glitch Trigger verletzt diese Regel vollständig (kein einziger der beiden Aufrufe vorhanden) —
nicht nur eine falsche Reihenfolge wie beim bereits dokumentierten RS485-Modbus-Fund, sondern
komplettes Fehlen wie beim CAN-Commander-Fund.

## Fund: Kein `expansion_disable()`/`expansion_enable()` — UART-Auto-Hit kann vom Stock-Expansion-Service dauerhaft blockiert werden

`glitch_app_alloc()`/`glitch_app_free()` (`glitch_trigger.c`) öffnen nirgends `RECORD_EXPANSION`
und rufen nirgends `expansion_disable()`/`_enable()` auf (per `grep -rn "expansion"` im gesamten
Quellbestand vor dem Fix bestätigt — null Treffer). Sechs andere, bereits in diesem Katalog
dokumentierte Erweiterungsport-/UART-Apps (UART Sniff, RS485 Modbus, GPIO Logic Analyzer,
SWD Probe, Argus, CAN Commander) tun das (bei den ersten dreien korrekt, bei RS485 Modbus mit
einer bereits dokumentierten Reihenfolge-Verwechslung, bei CAN Commander ebenfalls komplett
fehlend) — Glitch Trigger reiht sich bei "komplett fehlend" ein.

Das ist kein kosmetisches Versäumnis, sondern führt nachweislich zu einem Ausfall der
UART-Erfolgsstring-Erkennung, wenn der Stock-"Expansion"-Dienst in seiner Standardeinstellung
läuft (gegen die eigene `G4MEOVER-FW`-Zweitkopie nachvollzogen, Datei-/Funktionsnamen unten
identisch zum bereits bei CAN Commander dokumentierten Mechanismus):

1. `applications/services/expansion/expansion.c::expansion_on_system_start()` ruft beim
   Systemstart unbedingt `expansion_enable()` auf (sofern eine SD-Karte gemountet ist).
   `expansion_settings_load()` fällt beim Fehlen einer eigenen Einstellungsdatei auf
   `memset(settings, 0, ...)` zurück — `uart_index = 0` entspricht `FuriHalSerialIdUsart`, also
   genau dem UART, das `glitch_serial.c` selbst für die Feedback-Quelle "UART" nutzt
   (`furi_hal_serial_control_acquire(FuriHalSerialIdUsart)`).
2. `expansion_control_handler_enable()` registriert dafür
   `furi_hal_serial_control_set_expansion_callback()`, das laut
   `furi_hal_serial_control.c::furi_hal_serial_control_enable_expansion_irq()` einen
   GPIO-Interrupt auf **fallende Flanke** des RX-Pins scharf schaltet — reagiert bereits auf das
   erste Startbit eines beliebigen Bytes auf der Leitung, nicht erst auf ein vollständiges
   Handshake-Paket.
3. Startet der Nutzer eine Sweep-Kampagne mit Feedback-Quelle "UART" (Pins 13 TX/14 RX zum
   Zielboard verdrahtet) und liegt am Zielboard-UART bereits vor oder während des Starts
   irgendein Pegelwechsel an (Zielboard läuft bereits, eigenes Boot-Logging, Bus-Rauschen) —
   realistisch bei jedem Zielboard, das schon vor dem Flipper-Sweep-Start läuft oder
   unabhängig bootet —, kann diese Flanke die Expansion-Erkennung auslösen, bevor
   `glitch_serial_start()` selbst `furi_hal_serial_control_acquire()` aufruft.
4. Löst die Flanke aus, wechselt der Expansion-Dienst nach `ExpansionStateRunning` und ruft
   `expansion_worker_alloc()`/`_start()` auf — `expansion_worker.c` ruft darin selbst
   `furi_hal_serial_control_acquire(instance->serial_id)` auf und hält das Handle, während es
   auf ein eigenes Handshake wartet.
5. Ruft `glitch_serial_start()` danach selbst
   `furi_hal_serial_control_acquire(FuriHalSerialIdUsart)` auf, liefert die Funktion laut
   eigener Doku bei bereits vergebenem Handle `NULL` zurück — `glitch_serial_start()` gibt
   `false` zurück (siehe `glitch_serial.c:62-64`), die Sweep-Szene setzt `s_uart = false` und
   fährt fort, als wäre nichts geschehen (siehe compoundierender Befund unten).
6. Der Expansion-Worker gibt das Handle irgendwann wieder frei (eigenes Handshake-Timeout), aber
   zu diesem Zeitpunkt hat die Sweep-Kampagne bereits ohne Feedback-Kanal begonnen; ein manueller
   Neustart der Sweep-Szene (Rechts-Taste, `glitch_scene_sweep_on_event()`/`SweepEvtRestart`)
   ruft `glitch_serial_start()` erneut auf und kann danach zufällig funktionieren (wenn der
   Expansion-Dienst inzwischen im Leerlauf ist) oder erneut fehlschlagen.

Betroffen ist jede Sweep-Kampagne mit Feedback-Quelle "UART" auf einem Flipper, auf dem die
Expansion-Modul-Erkennung aktiv ist — das ist, da `expansion_enable()` beim Systemstart unbedingt
aufgerufen wird und die Standardeinstellung exakt das von Glitch Trigger genutzte UART überwacht,
der Auslieferungszustand selbst, nicht ein exotischer Sonderfall.

### Compoundierender Befund (dokumentiert, bewusst nicht gepatcht)

Selbst wenn `glitch_serial_start()` fehlschlägt, zeigt der Sweep-Screen weiterhin das "A"
(Auto-Hit)-Abzeichen an — `sweep_view_set_auto()` wird mit `app->params.auto_hit` aufgerufen
(der Nutzerabsicht), nie mit dem tatsächlichen `s_uart`-Verbindungsstatus
(`scenes/glitch_scene_sweep.c:170-173` vs. `glitch_sweep_push()`). Eine unbeaufsichtigte
Kampagne — genau das vom README als Hauptnutzen beworbene "hands-off"-Szenario — kann dadurch
beliebig lange mit Hit-Rate 0 % laufen, ohne dass der Nutzer erkennen kann, ob das Zielboard
tatsächlich immun ist oder der Feedback-Kanal nie verbunden hat. Bewusst **ohne** Fix in dieser
Sitzung, da eine saubere Lösung einen neuen UI-Zustand im Sweep-View erfordert (mehr als die hier
gewählte additive, risikoarme Änderung) — als eigener Rückstellpunkt dokumentiert, analog zum bei
Sentry Safe Opener notierten, ebenfalls nicht gepatchten Mutex-Doppel-Release-Befund.

## Unabhängiger Zweitfund: stille Versionsnummer-Regression (`GLITCH_VERSION` vs. `fap_version`)

Per `git log --oneline -p -- glitch_trigger_i.h`/`application.fam` auf den exakten Commit
zurückverfolgt: der Release-Commit `7c03f73` ("Glitch Trigger 1.3 — UART success-string auto-hit
+ hit-rate stats") hebt `application.fam`s `fap_version` korrekt von `"1.2"` auf `"1.3"` an, bumpt
aber `glitch_trigger_i.h`s `#define GLITCH_VERSION "1.2"` **nicht** mit — bei allen drei
vorherigen Releases (`1.0`→`1.1`→`1.2`) wurden beide Stellen im selben Commit synchron
angehoben, hier zum ersten Mal übersehen. Der About-Screen (`scenes/glitch_scene_about.c`, letzte
Zeile `"v" GLITCH_VERSION "  -  MIT License"`) zeigt seit diesem Commit "v1.2" an, obwohl die App
bereits alle v1.3-Funktionen (UART-Auto-Hit, Hit-Rate-Anzeige, `success_str`/`uart_baud`/
`fb_source` in der Persistenz) enthält — derselbe Bug-Typ wie beim bereits katalogisierten
PINGEQUA-RF-Lab-Fund (`ideas/pingequa-rf-lab-2in1-app-status.md`), hier aber auf den exakten
Release-Commit zurückverfolgt statt nur gegen das ausgelieferte Binary bestätigt.

## Betroffene Dateien

- `glitch_trigger_i.h` — neuer Include (`<expansion/expansion.h>`), neues Feld
  (`Expansion* expansion;` in `struct GlitchApp`/`GlitchApp`-Typedef), Versionsbump
  (`GLITCH_VERSION "1.2"` → `"1.3"`).
- `glitch_trigger.c` — `glitch_app_alloc()`/`glitch_app_free()` (neue
  `expansion_disable()`/`expansion_enable()`-Aufrufe um den gesamten App-Lebenszyklus).

## Fix

Additiv, nach demselben bei UART Sniff/GPIO Logic Analyzer/SWD Probe/CAN Commander bereits
etablierten Muster ("`expansion_disable()` VOR der ersten
`furi_hal_serial_control_acquire()`-Nutzung beim Start, `expansion_enable()` NACH der letzten
`furi_hal_serial_control_release()`-Nutzung beim Stop"):

- `glitch_trigger_i.h`: `#include <expansion/expansion.h>` neben den übrigen Service-Includes;
  Feld `Expansion* expansion;` direkt neben `NotificationApp* notifications;` in `GlitchApp`;
  `GLITCH_VERSION` auf `"1.3"` angehoben (passend zu `application.fam`).
- `glitch_trigger.c`, `glitch_app_alloc()`: direkt nach dem Öffnen von `RECORD_NOTIFICATION` und
  klar vor `glitch_engine_alloc()`/`glitch_serial_alloc()` sowie jeder scene-getriebenen
  Verwendung —
  `app->expansion = furi_record_open(RECORD_EXPANSION); expansion_disable(app->expansion);`.
  Bewusst für die **gesamte** App-Laufzeit deaktiviert (nicht nur während eines laufenden
  UART-Sweeps): die App belegt den Erweiterungsport ohnehin durchgehend für ihre eigenen
  Glitch-Out-/Trigger-In-/Feedback-Pins (`glitch_engine_configure()`), ein gleichzeitig
  angeschlossenes offizielles Erweiterungsmodul ist damit ohnehin kein unterstützter
  Anwendungsfall — exakt dieselbe Abwägung, die bereits beim CAN-Commander-Fix dokumentiert und
  akzeptiert wurde.
- `glitch_trigger.c`, `glitch_app_free()`: direkt nach `glitch_serial_free(app->serial)` und
  `glitch_engine_free(app->engine)` (beide geben das jeweilige Handle endgültig frei) —
  `expansion_enable(app->expansion); furi_record_close(RECORD_EXPANSION);`. Reihenfolge bewusst
  *nach* dem vollständigen Teardown, um die bei RS485 Modbus dokumentierte vertauschte
  Reihenfolge nicht zu wiederholen.

Patch (`firmware-mining/glitchtrigger-expansion-conflict-fix/glitchtrigger_expansion_fix.patch`)
gegen zwei frische, unabhängige Zweitkopien von `at0m-b0mb/GlitchTrigger-FlipperZero` (Commit
`c40b64b`) mit `git apply --check` verifiziert, real angewendet, Ergebnis per `diff -r`
byte-für-byte abgeglichen — identisch.

## Host-Repro

Nicht sinnvoll als eigenständiges Host-Programm nachstellbar — wie beim CAN-Commander-Fund ist
dies kein Speicherfehler/keine Logikabweichung in reinem C-Code, sondern ein Ressourcenkonflikt
zwischen zwei Firmware-Diensten (`Expansion`-Systemdienst vs. App) um ein einziges
Hardware-UART-Handle, nur über den tatsächlichen `furi_hal_serial_control`-/
`expansion`-Quelltext der Firmware selbst nachvollziehbar (siehe Fund-Abschnitt, Schritte 1–6,
jeder Schritt mit exakter Datei-/Funktionsreferenz gegen `G4MEOVER-FW`/`mntm-012`). Der
Versionsnummer-Fix ist über `git log -p` auf den exakten Release-Commit zurückverfolgt (siehe
oben) statt per Host-Repro — ein reiner String-Literal-Vergleich, kein Kontrollfluss.

## Testhinweis (lokal)

1. `glitch_trigger` (mit Patch) gegen die eigene `mntm-012`-Basis mit `ufbt`/`fbt` bauen,
   `applications/external/glitch_trigger/` wie gewohnt einbinden.
2. Vor dem Patch: `Settings → System → Expansion` auf dem Gerät **aktiviert** lassen
   (Flipper-Auslieferungszustand). Ein beliebiges UART-Gerät an Pins 13/14 anschließen, das
   bereits vor dem Flipper-Sweep-Start Pegel wechselt (z. B. ein zuvor gestartetes Zielboard
   mit eigenem Boot-Logging). Sweep mit Feedback-Quelle "UART" starten — reproduzierbar (nicht
   bei jedem Versuch, da vom exakten Timing abhängig, aber über mehrere Neustarts beobachtbar):
   Auto-Hit feuert nie, obwohl das Zielboard tatsächlich den Erfolgs-String ausgibt.
3. Nach dem Patch: identischer Ablauf, App ruft `expansion_disable()` bereits beim App-Start
   auf — der Expansion-Dienst kann das Handle währenddessen nicht mehr an sich reißen, die
   UART-Erfolgsstring-Erkennung muss zuverlässig funktionieren.
4. Regressionstest: App normal beenden (zurück ins Flipper-Hauptmenü), danach ein offizielles
   Erweiterungsmodul (z. B. WiFi Dev Board, falls vorhanden) verwenden — muss nach dem
   `expansion_enable()` in `glitch_app_free()` unverändert funktionieren (kein dauerhaftes
   Abschalten der Expansion-Erkennung über das Ende der `glitch_trigger`-Sitzung hinaus).
5. About-Screen (Hauptmenü → About) zeigt nach dem Patch korrekt "v1.3" statt "v1.2".

## Risiko

Niedrig. Rein additiv (ein neues Feld, ein neuer Include, zwei neue Aufrufe, ein
Einzeiler-Versionsbump), ändert nichts an der bestehenden Puls-/Sweep-/Fault-Map-Logik. Einziger
Nebeneffekt (beabsichtigt, identisch zum bereits akzeptierten CAN-Commander-Fix): während
`glitch_trigger` läuft, kann der Flipper währenddessen kein offizielles Erweiterungsmodul
gleichzeitig über dieselben Pins erkennen — erwartet, da die App den Erweiterungsport ohnehin
exklusiv für die eigene Glitch-Verdrahtung nutzt.

## Sicherheitsrahmen

Reiner Verbindungs-/Informationsfund (kein Speicherfehler, keine Remote-Angriffsfläche) in einem
Werkzeug, das laut eigener Doku ausschließlich auf eigenen/autorisierten Entwicklungsboards zum
Einsatz kommt (CONSTRAINTS Punkt 5). Keine Kartendaten oder echten Zugangsdaten beteiligt.

## Offen

- Lokale Bauprobe sowie Beschaffung/Aufbau der MOSFET-/Gate-Driver-Crowbar-Stufe für den
  eigentlichen Hardware-Test weiterhin offen (siehe `ideas/glitchtrigger-fault-injection-app-
  status.md`, Abschnitt Testhinweis/Wiring).
- Der compoundierende UI-Befund (kein sichtbarer Unterschied zwischen "Auto-Hit angefordert" und
  "Auto-Hit-UART tatsächlich verbunden") bleibt als eigener Rückstellpunkt offen.
- Übrige Quelldateien (Puls-Engine, Fault-Map, Storage, alle Szenen/Views) vollständig
  gelesen und bis auf die beiden hier behandelten Funde ohne weiteren Speicherfehler/Logikfund —
  Details in `ideas/glitchtrigger-fault-injection-app-status.md`.

## Quelle

- `at0m-b0mb/GlitchTrigger-FlipperZero` (direktes GitHub-Repo, Tag `v1.3`, Commit `c40b64b`).
- Referenz-Fix-Muster: `firmware-mining/can-commander-expansion-conflict-fix.md`,
  `ideas/uart-sniff-api-status.md`, `ideas/rs485-modbus-app-status.md` (bereits dokumentierte
  `expansion_disable()`/`_enable()`-Nutzung derselben Firmware-API).
