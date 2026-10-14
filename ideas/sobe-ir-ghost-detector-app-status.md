# Sobe (sacriphanius) — IR-Ghost-Detector (versteckte Kameras/Nachtsicht)

## Quelle

`gitlab.com/sacriphanius/sobe`, MIT-Lizenz (`LICENSE`-Datei im Repo verifiziert). Frisch, blobfrei
geklont (`GIT_LFS_SKIP_SMUDGE=1 git clone --depth 10`), alle acht Quelldateien (`sobe.c`,
`ir_worker.c/.h`, `analysis.c/.h`, `ui.c/.h`, `sobe_app_i.h`) vollständig gelesen (521 Zeilen,
100 %). Sehr junges, kleines Projekt: `application.fam` nennt `fap_version="1.2.0"`, Repo hat laut
Metadaten nur zwei Commits und einen Tag, letzte Aktivität 26.07.2026.

Engerer Alternativkandidat zu **Nyx** (`at0m-b0mb`, bereits in
`ideas/nyx-ir-hidden-camera-detector-app-status.md` katalogisiert) — beide Apps lösen dieselbe
Aufgabe (passive IR-Erkennung versteckter Nachtsichtkameras), hier aber mit einem deutlich
kleineren, eigenständigen Quellbestand und zusätzlich einer aktiven "Spy-Finder"-Sweep-Funktion,
die Nyx nicht hat.

## Was

Zwei Betriebsmodi über den Flipper-IR-Empfänger (intern oder ein per GPIO-Pin `A7`
auto-erkanntes externes IR-Modul, inkl. automatischer 5-V-OTG-Versorgung für Letzteres):

- **Passiv** (Start-Modus): hört nur auf vorhandene IR-Signale (gepulste Nachtsicht-Illuminatoren)
  und bewertet die Pulsrate/-dauer-Verteilung als "Probability"-Prozentwert.
- **Aktiv (Sweep)**: sendet alle ~65 ms einen kurzen 38-kHz-IR-Blitz (10 ms) und lauscht danach auf
  Reflexionen (klassisches "Optic Spy Finder"-Prinzip: ein Kamera-Objektiv reflektiert den
  eigenen IR-Blitz zurück).
- Live-Anzeige (Ambient/Reflect/Probability-Balken) plus LED/Vibration-Alarm ab zwei Schwellen
  (40 %/75 %).

## Nutzen

Ergänzt Nyx um eine zweite, unabhängige Implementierung derselben Gegenüberwachungs-Kategorie
(IR-Kanal, siehe `ideas/pentest-app-katalog.md`, Abschnitt "Counter-Surveillance / TSCM") — anders
als bei **Badge Audit vs. Access Audit** (siehe `ideas/badge-audit-app-status.md`) hier aber kein
reiner Funktionsunterschnitt: Sobes aktiver Sweep-Modus deckt einen Erkennungsfall ab, den Nyx
bewusst nicht abdeckt (Nyx ist laut eigener Doku listen-only, erkennt nur aktiv blitzende
IR-Illuminatoren, keine passiven/reflektierenden Linsen ohne eigene IR-Quelle). Beide Apps sind
daher komplementär, kein Verwerfungs-Fall wie Badge Audit. Kleiner, leicht überschaubarer
Quellbestand (521 Zeilen) macht eine vollständige Prüfung in einer Sitzung möglich.

## Aufwand

Gering. Acht kompakte Quelldateien, reine externe `.fap`, keine Firmware-Änderung. Optional ein
externes IR-Modul am Erweiterungsport für den "EXT"-Modus (sonst automatisch interner IR-Sender/
-Empfänger).

## API-87-Status

✅ Kompatibel, geprüft. Frischer, blobloser Sparse-Checkout von `Next-Flip/Momentum-Firmware`
(Tag `mntm-012`, Commit `b359f6f`) gegen den eigenen Header-/Implementierungs-Stand abgeglichen:

- IR-HAL (`targets/furi_hal_include/furi_hal_infrared.h`,
  `targets/f7/furi_hal/furi_hal_infrared.c`): alle sieben verwendeten Funktionen
  (`furi_hal_infrared_async_rx_start/_stop`, `_async_rx_set_capture_isr_callback`,
  `_async_tx_set_data_isr_callback`, `_async_tx_start/_stop`, `_set_tx_output`) und der
  `FuriHalInfraredTxPin`-Enum (`Internal`/`ExtPA7`) byteidentisch vorhanden. Zustandsmaschine
  des HAL (`furi_hal_infrared_state`, intern `InfraredStateIdle`/`AsyncRx`/`AsyncTx`/...)
  anhand des Quelltexts nachvollzogen: `async_rx_stop()` und `async_tx_stop()` sind beide
  synchron (Letzteres über `async_tx_wait_termination()`, blockierend bis zur Semaphore) und
  garantieren `InfraredStateIdle` vor der Rückkehr — Sobes Aktiv-Sweep-Schleife
  (`ir_worker.c::sobe_worker_thread()`, RX-Stop → TX-Start/-Stop → RX-Start im ~65-ms-Takt)
  verletzt damit keine der `furi_check()`-Zustandsvorbedingungen, kein Crash-Risiko hier.
- Power-HAL (`targets/furi_hal_include/furi_hal_power.h`,
  `targets/f7/furi_hal/furi_hal_power.c`): `furi_hal_power_enable_otg()`/`_disable_otg()`
  byteidentisch vorhanden.
- GPIO (`gpio_ext_pa7` in `targets/f7/furi_hal/furi_hal_resources.c`),
  Benachrichtigungs-Sequenzen (`sequence_blink_red_100`/`_blink_blue_100`/`_single_vibro` in
  `applications/services/notification/notification_messages.h`) und
  `elements_progress_bar()` (`applications/services/gui/elements.h`) — alle unverändert mit
  identischer Signatur vorhanden.
- `application.fam`: `requires=[]` (keine deklarierte Abhängigkeit über `gui`/`notification`
  hinaus, die per `furi_record_open()` direkt angefordert werden), `fap_category="Tools"`.

## Fund: externe 5-V-OTG-Versorgung bleibt nach App-Ende dauerhaft an

Der GPIO-Autoerkennungsblock in `sobe.c::sobe_app()` reimplementiert von Hand exakt das, was die
Firmware bereits als fertige API bereitstellt (`furi_hal_infrared_detect_tx_output()` in
`targets/f7/furi_hal/furi_hal_infrared.c`, Zeile 710 ff. — identischer Algorithmus: Pull-Up setzen,
kurz warten, Pegel lesen, auf `GpioModeAnalog` zurücksetzen). Reine Code-Duplikation ohne
Funktionsunterschied (Sobes Variante wartet 10 ms statt der offiziellen 1 ms, sonst gleich) — kein
eigener Fund an dieser Stelle.

Der eigentliche Fund liegt eine Funktion weiter, in `ir_worker.c::sobe_worker_set_tx_output()`:

```c
void sobe_worker_set_tx_output(SobeWorker *worker, bool use_external) {
  UNUSED(worker);
  if (use_external) {
    furi_hal_power_enable_otg();
    furi_hal_infrared_set_tx_output(FuriHalInfraredTxPinExtPA7);
  } else {
    furi_hal_power_disable_otg();
    furi_hal_infrared_set_tx_output(FuriHalInfraredTxPinInternal);
  }
}
```

Diese Funktion ist die **einzige** Stelle im gesamten 521-Zeilen-Quellbestand, die
`furi_hal_power_enable_otg()`/`_disable_otg()` aufruft (per `grep` über alle `.c`/`.h`-Dateien
bestätigt) — und sie wird genau **einmal** aufgerufen, direkt nach dem Start in `sobe_app()`:

```c
app->use_external_ir = external_present;
sobe_worker_set_tx_output(worker, app->use_external_ir);   // einziger Aufruf im ganzen Quelltext
```

Wird beim Start ein externes IR-Modul erkannt, schaltet Sobe damit die 5-V-OTG-Boost-Versorgung
des Flipper ein — und schaltet sie **nirgends** wieder ab, auch nicht in der Aufräumsequenz von
`sobe_app()` beim Beenden (`sobe_analysis_stop()`/`sobe_worker_stop()`/`_free()`, dann
`gui_remove_view_port()`/`view_port_free()`/`sobe_app_free()` — keiner dieser Aufrufe berührt
OTG oder den TX-Pin erneut).

Zum Vergleich der offizielle Referenzpfad (`applications/main/infrared/infrared_app.c`, dieselbe
Aufgabe: externes IR-Modul erkennen, OTG dafür einschalten): dort steht direkt nach
`view_dispatcher_run()` (d. h. im Moment des App-Endes, vor `infrared_free()`):

```c
infrared_set_tx_pin(infrared, FuriHalInfraredTxPinInternal);
infrared_enable_otg(infrared, false);
infrared_free(infrared);
```

— die Firmware selbst macht die OTG-Abschaltung beim Beenden also explizit zur Pflicht für jede
App, die extern aktiviert hat. Sobe hat dieses symmetrische Gegenstück schlicht nicht.

**Konsequenz:** Wird Sobe mit angeschlossenem externem IR-Modul gestartet und dann regulär über
"Zurück" beendet, bleibt der 5-V-Boost-Wandler (bq25896) bis zum nächsten Geräte-Neustart oder
einer manuellen Abschaltung (z. B. über eine andere App, die ihrerseits OTG korrekt verwaltet, oder
einen eigenen Power-Cycle) aktiv — ein bekannter, in der Community dokumentierter
Akku-Verschleiß-Faktor, weshalb die offizielle Infrarot-App genau diese Abschaltung beim Beenden
erzwingt. Reiner Robustheitsfund (eigener Akkuverbrauch), kein Ziel-Sicherheitsrisiko.

## Fix-Skizze (additiv)

In `sobe.c::sobe_app()`, direkt nach `sobe_worker_stop(worker);` und vor `sobe_worker_free(worker);`
die bereits vorhandene Funktion ein zweites Mal mit `false` aufrufen, wenn extern aktiviert war:

```c
  sobe_analysis_stop(analysis);
  sobe_worker_stop(worker);

  if (app->use_external_ir) {
    sobe_worker_set_tx_output(worker, false);
  }

  sobe_analysis_free(analysis);
  sobe_worker_free(worker);
```

Rein additiv (vier neue Zeilen, keine Signaturänderung, keine neue Funktion), spiegelt exakt das
offizielle `infrared_set_tx_pin(internal) + infrared_enable_otg(false)`-Muster. Patch in
`ideas/sobe-otg-left-enabled-on-exit-fix/sobe_c.patch`, gegen zwei frische, unabhängige
Zweitkopien mit `git apply --check` verifiziert, real angewendet, beide Endstände byte-für-byte
identisch zum erwarteten Ergebnis.

## Testhinweis (lokal nachvollzogen)

Host-kompilierbares Kontrollfluss-Repro (`repro.c`) in
`ideas/repro-sobe-otg-left-enabled-on-exit/` — bildet `sobe_worker_set_tx_output()` sowie den
Start-/Exit-Ablauf von `sobe_app()` 1:1 nach, mit einem Mock für `furi_hal_power_enable_otg()`/
`_disable_otg()` (ein gemeinsames globales Bool, das den einen realen Hardware-Zustand des
bq25896-Boost-Wandlers abbildet). Gebaut/ausgeführt mit
`gcc -std=c11 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-sanitize-recover=undefined`
bei `-O0` und `-O1`:

- Zwei Regressionsfälle (kein externes Modul erkannt): ALT und NEU lassen OTG in beiden Fällen
  korrekt aus, identisches Verhalten.
- Fund: externes Modul erkannt, ALT-Exit-Sequenz — OTG bleibt nach dem simulierten App-Ende
  eingeschaltet.
- Fix: externes Modul erkannt, NEU-Exit-Sequenz — OTG ist nach dem simulierten App-Ende wieder
  ausgeschaltet.

Ergebnis beider Durchläufe (`-O0`/`-O1`): **ALLE CHECKS BESTANDEN**, keine ASan-/UBSan-Meldungen,
keine Compiler-Warnungen.

Lokal zu tun: Patch gegen die eigene `sobe.c` anwenden, mit `ufbt launch_app`/`fbt` bauen.
Funktionstest: eigenes externes IR-Modul am Erweiterungsport anschließen, Sobe starten (Anzeige
muss "EXT" zeigen), App über "Zurück" beenden, danach über `Settings → Power → USB-Power-Output`
(oder per `power` CLI) prüfen, dass die 5-V-Ausgabe aus ist — vor dem Fix bleibt sie an, nach dem
Fix ist sie nach dem Beenden aus. Ohne externes Modul (interner IR) ist das Verhalten vor/nach dem
Fix unverändert (OTG wird in diesem Fall nie eingeschaltet).

## Sicherheitsrahmen

Rein passiv/listen-only im Passiv-Modus; der Aktiv-Sweep-Modus sendet ausschließlich kurze,
unmodulierte 38-kHz-IR-Pulse (kein Datenprotokoll, keine Fremdgeräte-Beeinflussung) — passt in den
defensiven Rahmen aus `CONSTRAINTS.md` Punkt 5. Der gefundene Fund ist ein reines
Akku-/Ressourcenverwaltungsproblem der eigenen App, kein Ziel-Sicherheitsrisiko.
