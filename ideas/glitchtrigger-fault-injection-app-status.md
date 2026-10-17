# Glitch Trigger (at0m-b0mb) — neue Kategorie "Fault Injection / Glitching"

Gezielt nach einer bislang komplett fehlenden Kategorie gesucht: alle bisherigen
Hardware-Debug-/Bus-Funde (SWD Probe, GPIO Logic Analyzer, i2c Tools, UART Sniff, RS485 Modbus,
CAN-Bus-Cluster, SPI Flash Dump) lesen oder sprechen einen Ziel-Bus passiv/aktiv an — keiner
davon deckt die Spannungs-Glitching-/Fault-Injection-Seite ab (gezielte Störung der
Versorgungsspannung eines Ziel-Chips, um Secure-Boot-/PIN-Retry-/Readout-Protection-Logik zu
überlisten). Dieselbe `at0m-b0mb`-Werkstatt, aus der bereits Nyx/Specter/GhostTag/Argus (TSCM-
Cluster) stammen, pflegt dafür ein eigenständiges Werkzeug.

## Quelle

- Repo: `at0m-b0mb/GlitchTrigger-FlipperZero` (direktes GitHub-Repo, MIT-Lizenz), App-ID
  `glitch_trigger`, `fap_category="GPIO"`.
- Geprüfter Stand: Tag `v1.3`, aktueller HEAD-Commit `c40b64b` (zwei Commits nach dem Tag
  betreffen nur `README.md`/`docs/` — keine Quelltextänderung).
- 36 Quelldateien (`.c`/`.h`), 3656 Zeilen gesamt — vollständig gelesen (App-Lebenszyklus,
  Puls-Engine, UART-Erfolgsstring-Watcher, SD-Profile/CSV-Log, Fault-Map-Grid, alle elf
  Szenen, alle vier Custom-Views).

## Was

Kein eigener Glitcher — ein präziser, konfigurierbarer GPIO-Impulsgenerator für die
**Trigger-Seite** einer Spannungsglitching-Kampagne gegen eigene Entwicklungsboards. Das Modell
ist ein "Schuss" (`trigger → delay → puls(e)`): ein Flipper-GPIO-Pin steuert ein externes
MOSFET-/Gate-Driver-Crowbar an, das tatsächlich die Zielversorgung kurzschließt/einbricht — der
Flipper selbst liefert nur das zeitlich präzise Steuersignal dafür (3,3-V-Push-Pull, kann keine
Zielschiene selbst schalten, README macht das explizit und wiederholt klar).

- **Timing:** Cortex-M4-DWT-Zykluszähler (`DWT->CYCCNT`) in einem `FURI_CRITICAL_ENTER()`/
  `_EXIT()`-Fenster (Interrupts maskiert) für kanten-genaue Pulse (~15,6 ns Auflösung bei
  64 MHz); lange Arm-Delays laufen größtenteils mit Interrupts an (gekapptes 1-ms-Fenster für
  den kritischen Rest), damit BLE/USB/System-Tick nicht einfrieren.
- **Drei Trigger-Modi:** Manuell (OK-Taste), Extern (GPIO-Interrupt auf eine Flanke am
  Trigger-Eingang, niedrigste Latenz), Repeat (freilaufend im Intervall).
- **Sweep (1D/2D):** Pulsbreite allein oder volles Delay×Breite-Gitter, linear oder zufällig
  durchlaufen, mit Dwell (N Schüsse pro Punkt) und Live-Fortschrittsbalken.
- **Auto-Hit:** nach jedem Schuss automatische Treffer-Erkennung über eine Ziel-Feedback-Quelle
  — entweder ein GPIO-Pegel oder ein per UART (Pins 13 TX/14 RX, einstellbare Baudrate)
  beobachteter Erfolgs-Teilstring (gleitendes Fenster über einen ISR-gespeisten Ringpuffer,
  korrekt implementiert — siehe Quelltext-Review).
- **Fault Map:** jeder Treffer landet auf einer Delay×Breite-Heatmap (scrollbarer Cursor,
  CSV-Export); zusätzlich fortlaufendes CSV-Hit-Log und persistente, benannte Profile auf der
  SD-Karte.
- **On-Device-Wiring-Screen** mit rotierenden Sicherheitshinweisen statt README am Bankarbeits-
  platz.

## Nutzen für mich

Eigenständige, bislang fehlende Kategorie — kein Ersatz für, sondern Ergänzung zu den
bestehenden Bus-/Debug-Funden: SWD Probe und GPIO Logic Analyzer liefern die Diagnose
("was läuft auf dem Ziel-Board"), Glitch Trigger liefert die Angriffsseite ("Zielzustand durch
eine präzise getimte Störung kippen") — klassischer Baustein für autorisierte Secure-Boot-/
Readout-Protection-Studien an eigener Hardware. Benötigt zusätzlich eine externe
MOSFET-/Gate-Driver-Crowbar-Stufe (keine komplexe Eigenentwicklung, Standard-Schaltung laut
README) — reiner Software-Fund ohne eigene Hardware-Verifikation bis zur Beschaffung/zum Aufbau
dieser Crowbar-Stufe.

## Aufwand

Klein bis mittel. Die App selbst ist ein Standard-FAP ohne Fremdhardware-Treiber (reine
GPIO/DWT/UART-Nutzung). Für den eigentlichen Glitch-Versuch zusätzlich: ein Logic-Level-N-FET
(oder dedizierte Glitcher-Platine), kurze Leitungen, gemeinsame Masse mit dem Zielboard — siehe
Wiring-Screen/README.

## API-87-Status (mntm-012)

✅ Kompatibel, geprüft. Gegen eine frische, unabhängige Zweitkopie von `G4MEOVER18/G4MEOVER-FW`
(Tag `mntm-012`, Commit `b359f6f`) verifiziert:

- `furi_hal_gpio_init/_init_simple/_write/_read/_add_int_callback/_remove_int_callback/
  _enable_int_callback`, `GpioModeOutputPushPull/_Analog/_InterruptRise/_InterruptFall`,
  `GpioPullNo/_Up/_Down` — byteidentisch vorhanden. Alle sechs in `glitch_gpio[]`
  referenzierten Erweiterungsport-Pins (`gpio_ext_pa7/_pa6/_pa4/_pb3/_pb2/_pc3`) byteidentisch
  vorhanden (bewusste Auswahl der App: vermeidet die SPI-/I²C-/USART-Standardpins).
- `FURI_CRITICAL_ENTER()`/`_EXIT()`, `DWT->CYCCNT`/`CoreDebug->DEMCR` (STM32WB55, Cortex-M4) —
  unverändert vorhanden, identische Hardware wie bei allen bisher geprüften GPIO-/Bus-Apps.
- `furi_hal_serial_control_acquire/_release/_is_busy()`, `furi_hal_serial_init/_deinit/
  _async_rx_start/_stop/_async_rx_available/_async_rx()`, `FuriHalSerialIdUsart`,
  `FuriHalSerialRxEventData` — byteidentisch vorhanden (dieselbe bereits bei UART Sniff/
  RS485 Modbus/CAN Commander verifizierte moderne API, kein `cli_session_open/close`-Kompat-
  Pfad nötig).
- `applications/services/expansion/expansion.h`: `RECORD_EXPANSION`, `Expansion`-Opaque-Typ,
  `expansion_enable()`/`expansion_disable()` — byteidentisch vorhanden. Die Header-Doku dieser
  Datei benennt die Reihenfolge sogar explizit und normativ: *"Applications requiring serial
  port access MUST call this function BEFORE calling furi_hal_serial_control_acquire(). […] an
  expansion_enable() call MUST be made right AFTER a call to
  furi_hal_serial_control_release()"* — genau die Regel, gegen die Glitch Trigger verstößt (s.
  Fund unten).
- Standard-GUI-/`ViewDispatcher`-/`SceneManager`-/Storage-API — byteidentisch, keine Abweichung.

Kein Port-Hindernis: reines externes FAP (`applications/external/glitch_trigger/`), keine
Firmware-Core-Änderung nötig.

## Quelltext-Review (Speichersicherheit)

Bis auf den unten dokumentierten Fund durchgehend sehr defensiv geschrieben — vergleichbar mit
den bereits ohne Speicherfehler-Fund geprüften `at0m-b0mb`-Werkzeugen (Nyx/Specter/GhostTag):

- **Puls-Engine** (`helpers/glitch_engine.c`): `glitch_delay_cycles()` ist wrap-sicher
  (`(uint32_t)(DWT->CYCCNT - start) < cycles`, funktioniert auch über einen `CYCCNT`-Überlauf
  hinweg). Pin-Index wird in `glitch_pin_for()` gegen `glitch_pins_len` geklemmt, nie ungeprüft
  als Array-Index verwendet.
- **UART-Erfolgsstring-Watcher** (`helpers/glitch_serial.c`): klassisches gleitendes Fenster
  über `needle`/`win` (beide `char[GLITCH_SUCCESS_MAX]`), `strncpy()` mit expliziter
  Nullterminierung beim Setzen der Nadel, `memmove()`-Schiebefenster korrekt um `nlen - 1`
  bytes — kein Off-by-one, kein Fall, in dem `wlen`/`nlen` den Puffer überschreiten können.
  Empfang läuft komplett über eine ISR → `FuriStreamBuffer` → Poll-auf-UI-Thread-Kette, keine
  gemeinsam beschriebene Struktur ohne Entkopplung.
- **Fault-Map/CSV-Export** (`helpers/glitch_map.c`): Bit-Packing-Index (`row * cols + col`)
  durchgehend gegen `cols`/`rows` geprüft, bevor auf `m->bits[]` zugegriffen wird;
  `glitch_map_export()`s Zeilenpuffer (`char line[GLITCH_MAP_W * 2 + 2]`) ist exakt für die
  maximale Spaltenzahl dimensioniert und die Schreibschleife bricht zusätzlich über
  `pos + 2 < sizeof(line)` ab.
- **Profile/Settings-Persistenz** (`helpers/glitch_storage.c`): Magic/Version/Size-Dreifach-
  Prüfung vor jeder Übernahme eines geladenen Blobs (verwirft stillschweigend statt mit
  falschen Werten weiterzuarbeiten, falls sich `GlitchParams` künftig ändert, ohne die
  `_VER`-Konstante zu bumpen); Profilname-Dateipfad läuft über `snprintf()` in einen
  ausreichend großen `char[128]`.
- **Text-Eingabe-Szenen** (Profile-Name, Erfolgs-String): beide übergeben
  `text_input_set_result_callback()` korrekt die tatsächliche Zielpuffergröße
  (`GLITCH_PROFILE_NAME_MAX`/`GLITCH_SUCCESS_MAX`), keine abweichende Konstante wie beim
  NFC-Tools-vCard-Fund.

## Fund: fehlendes `expansion_disable()`/`expansion_enable()` — UART-Auto-Hit kann vom
Stock-Expansion-Dienst dauerhaft blockiert werden; zusätzlich stille Versionsnummer-Regression

Zwei unabhängige, aber zusammen dokumentierte Funde derselben Sitzung — Details, Befund-Kette
und additiver Fix in `firmware-mining/glitchtrigger-expansion-conflict-fix.md`:

1. **Funktionsfund:** `glitch_app_alloc()`/`glitch_app_free()` öffnen nirgends
   `RECORD_EXPANSION` und rufen nirgends `expansion_disable()`/`_enable()` auf (per
   `grep -rn "expansion"` im gesamten Quellbestand vor dem Fix bestätigt: null Treffer) —
   exakt dieselbe Fehlerklasse, die ich bereits bei UART Sniff/RS485 Modbus/GPIO Logic
   Analyzer/SWD Probe/Argus/CAN Commander dokumentiert habe. `helpers/glitch_serial.c` holt
   sich für die UART-Auto-Hit-Funktion (Sweep-Szene, Feedback-Quelle "UART")
   `FuriHalSerialIdUsart` — exakt das UART, das der Flipper-eigene Expansion-Systemdienst in
   seiner Standardeinstellung ab Systemstart überwacht. Reagiert der Dienst zuerst auf eine
   Flanke am RX-Pin (z. B. Rauschen/erste Bytes vom Zielboard), hält er das Handle, und
   `furi_hal_serial_control_acquire()` in `glitch_serial_start()` schlägt fehl.
2. **Compoundierender Befund (nicht gepatcht, nur dokumentiert):** `glitch_serial_start()` gibt
   in diesem Fall `false` zurück, aber `glitch_scene_sweep_on_enter()` wertet den Rückgabewert
   nur in die lokale `s_uart`-Variable, ohne die UI zu informieren — das "A"-Auto-Hit-Abzeichen
   auf dem Sweep-Screen (`sweep_view_set_auto()`) spiegelt weiterhin nur
   `app->params.auto_hit` (die Nutzerabsicht), nicht den tatsächlichen Verbindungsstatus. Eine
   unbeaufsichtigte Kampagne (genau das vom README beworbene "hands-off"-Szenario) kann dadurch
   stundenlang mit Hit-Rate 0 % laufen, während der Nutzer eine immune Zielhardware vermutet —
   tatsächlich hat der Feedback-Kanal nie verbunden. Bewusst **ohne** Fix in dieser Sitzung
   (würde eine neue UI-Statusanzeige erfordern, siehe Offen unten) — nur der Vollständigkeit
   halber dokumentiert, analog zum bei Sentry Safe Opener notierten, nicht gepatchten
   Mutex-Doppel-Release.
3. **Unabhängiger, unzusammenhängender Zweitfund:** `GLITCH_VERSION` (`glitch_trigger_i.h`)
   stand beim Release-Commit `7c03f73` ("Glitch Trigger 1.3 — UART success-string auto-hit
   …") weiterhin auf `"1.2"`, während `application.fam`s `fap_version` korrekt auf `"1.3"`
   angehoben wurde — derselbe Bug-Typ wie beim PINGEQUA-RF-Lab-Fund
   (`ideas/pingequa-rf-lab-2in1-app-status.md`), hier per `git log -p` auf den exakten
   Release-Commit zurückverfolgt statt nur vermutet. Der About-Screen zeigte seit diesem
   Commit "v1.2" an, obwohl die App bereits die v1.3-Funktionen (UART-Auto-Hit, Hit-Rate)
   enthielt.

## Fix

Additiv (ein neues Feld, ein neuer Include, zwei neue Aufrufe, ein Einzeiler-Versionsbump),
Details und Herleitung in `firmware-mining/glitchtrigger-expansion-conflict-fix.md`, Patch in
`firmware-mining/glitchtrigger-expansion-conflict-fix/glitchtrigger_expansion_fix.patch` —
gegen zwei frische, unabhängige Zweitkopien von `at0m-b0mb/GlitchTrigger-FlipperZero`
(Commit `c40b64b`) mit `git apply --check` verifiziert, real angewendet, Ergebnis per `diff -r`
byte-für-byte abgeglichen — identisch.

## Testhinweis (lokal)

1. `glitch_trigger` (mit Patch) gegen die eigene `mntm-012`-Basis mit `ufbt`/`fbt` bauen,
   `applications/external/glitch_trigger/` wie gewohnt einbinden.
2. Vor dem Patch: `Settings → System → Expansion` aktiviert lassen (Auslieferungszustand),
   Sweep mit Feedback-Quelle "UART" starten, während am Zielboard-UART (Pins 13/14) bereits vor
   dem Start Pegelwechsel anliegen (z. B. Zielboard läuft schon) — reproduzierbar (abhängig vom
   exakten Timing, aber über mehrere Neustarts beobachtbar)
   `furi_hal_serial_control_acquire()` liefert `NULL`, `glitch_serial_start()` gibt `false`
   zurück, Auto-Hit feuert nie, UI zeigt unverändert das "A"-Abzeichen.
3. Nach dem Patch: identischer Ablauf, App ruft `expansion_disable()` bereits beim App-Start
   auf — der Expansion-Dienst kann das Handle währenddessen nicht mehr an sich reißen, die
   UART-Erfolgsstring-Erkennung muss zuverlässig funktionieren.
4. Regressionstest: App normal beenden, danach ein offizielles Erweiterungsmodul (z. B. WiFi
   Dev Board, falls vorhanden) verwenden — muss nach dem `expansion_enable()` in
   `glitch_app_free()` unverändert funktionieren.
5. About-Screen nach dem Patch zeigt korrekt "v1.3".

## Sicherheitsrahmen

Reiner Verbindungs-/Informationsfund (kein Speicherfehler, keine Remote-Angriffsfläche) in
einem Werkzeug, das laut eigener Doku ausschließlich auf eigenen/autorisierten
Entwicklungsboards zum Einsatz kommt (CONSTRAINTS Punkt 5). Keine Kartendaten oder echten
Zugangsdaten beteiligt. Das Glitching selbst erfordert ohnehin externe Hardware (MOSFET-
Crowbar) — diese App liefert nur den Trigger-Impuls, kein eigenständiges Angriffswerkzeug ohne
diese Zusatzhardware.

## Offen

- Lokale Bauprobe sowie Beschaffung/Aufbau der MOSFET-/Gate-Driver-Crowbar-Stufe für den
  eigentlichen Hardware-Test (siehe Wiring-Screen/README) weiterhin offen.
- Der unter Fund Punkt 2 dokumentierte UI-Befund (kein sichtbarer Unterschied zwischen
  "Auto-Hit angefordert" und "Auto-Hit-UART tatsächlich verbunden") ist bewusst nicht gepatcht
  — ein sauberer additiver Fix würde einen neuen, von `s_uart` gespeisten Badge-/Status-Zustand
  im Sweep-View erfordern (z. B. ein drittes Chip-Symbol oder ein verändertes "A"-Glyph bei
  nicht verbundenem UART-Feedback); als eigenständiger Rückstellpunkt für eine künftige Sitzung
  vorgemerkt, um diese Sitzung auf den einen vollständig verifizierten Fund zu begrenzen.
- Katalog-Zeile unter neuer Kategorie "Fault Injection / Glitching" in
  `ideas/pentest-app-katalog.md` ergänzt.
