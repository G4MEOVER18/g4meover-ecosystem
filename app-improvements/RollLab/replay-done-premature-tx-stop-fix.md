# RollLab — `rlab_replay_done()` beendet die TX, bevor die letzte Flanke physisch gesendet ist

## Was
`rolllab_rf.c`s Fertig-Erkennung fuer jede Signal-Wiederholung (Replay Attack, Rollback
Probe, Sync Window Probe — alle drei Modi laufen ueber dieselben `rlab_replay_start()`/
`rlab_replay_done()`/`rlab_replay_stop()`):

```c
bool rlab_replay_done(const RollLabApp* app) {
    return (app->tx_pos >= app->ref_sig.count) ||
           subghz_devices_is_async_complete_tx(app->device);
}
```

verknuepft zwei Bedingungen mit ODER, die **nicht** gleichzeitig wahr werden:

- `app->tx_pos >= app->ref_sig.count` — "der TX-Callback (`rlab_replay_tx_cb()`) hat alle
  gespeicherten Flanken bereits einmal an den Treiber uebergeben".
- `subghz_devices_is_async_complete_tx(app->device)` — das Treiber-eigene Signal "die
  physische Ausgabe ist wirklich fertig".

Das sind zwei verschiedene Zeitpunkte. `subghz_devices_start_async_tx()` arbeitet nach
demselben Double-Buffering-Vertrag wie jeder Timer-getriebene Bit-Bang-Sender: der
TX-Callback wird fuer die **naechste** Dauer bereits aufgerufen, waehrend die **aktuelle**
Dauer noch physisch laeuft (der Timer braucht die naechste Periodenlaenge schon geladen,
bevor die laufende abgelaufen ist — sonst entstuende eine Luecke im Signal). Dadurch liegt
die Zahl der Callback-Aufrufe (= `tx_pos`) **immer mindestens eine volle Flanke vor** der
Zahl der tatsaechlich physisch abgeschlossenen Flanken. Sobald `rlab_replay_tx_cb()` die
letzte gespeicherte Flanke abruft, wird `tx_pos >= ref_sig.count` schon wahr — die
**allerletzte Flanke selbst beginnt gerade erst**, physisch ausgegeben zu werden. Die
`||`-Verknuepfung lässt `rlab_replay_done()` genau in diesem Moment `true` liefern, die
Aufrufer-Schleife (`RLAB_REPLAYING`/`RLAB_PROBING` in `rolllab_app.c`) bricht ab und ruft
`rlab_replay_stop()` → `subghz_devices_stop_async_tx()` — die letzte Flanke wird mitten in
ihrer eigentlich vorgesehenen Dauer abgeschnitten, bevor sie fertig ist.

Dass `subghz_devices_is_async_complete_tx()` dafuer existiert, genau diesen Unterschied
abzubilden (physische Fertigstellung statt Daten-Erschoepfung), zeigt der offizielle
Referenzgebrauch: `DarkFlippers/unleashed-firmware`,
`applications/system/secplus_pin/secplus_pin_radio.c`, `secplus_pin_radio_tx()`:

```c
while(!subghz_devices_is_async_complete_tx(device) && (waited_ms < SECPLUS_PIN_TX_TIMEOUT_MS)) { ... }
```

— der offizielle Code wartet ausschliesslich auf `is_async_complete_tx()` (plus
Sicherheits-Timeout gegen einen haengenden Treiber), **ohne** einen eigenen "alle Daten
abgerufen"-Zaehler zusaetzlich heranzuziehen. Genau dieser bei mir zusaetzliche,
ODER-verknuepfte `tx_pos`-Check ist der hier dokumentierte Fund.

## Nutzen
- Betrifft **alle drei** RF-Sendepfade der App gleichermassen (Replay Attack, Rollback
  Probe, Sync Window Probe nutzen denselben `rlab_replay_*()`-Code) — nicht nur einen
  Modus.
- Besonders kontraproduktiv fuer den eigentlichen Zweck des Tools: Rollback Probe und Sync
  Window Probe sollen zeigen, ob ein Empfaenger einen **gueltigen, aber alten** Code noch
  akzeptiert. Wird die letzte Flanke des wiederholten Codes abgeschnitten, kann ein
  Empfaenger den Code schon allein wegen des unvollstaendigen Signals ablehnen — das Tool
  würde dann faelschlich "nicht verwundbar" anzeigen, obwohl die eigentliche
  Rolling-Code-Pruefung nie erreicht wurde. Der Fix macht das Messergebnis erst
  aussagekraeftig.
- Minimal-invasiv: entfernt nur die zusaetzliche, fruehzeitig wahre Teilbedingung; die
  bestehende Abbruchmoeglichkeit per Zuruecktaste (`app->abort`, in der Aufrufer-Schleife
  bereits vorhanden) bleibt als Notausstieg unveraendert erhalten, falls der Treiber einmal
  nicht fertig meldet.

## Betroffene Dateien (lokal, in meiner RollLab-App)
- `rolllab_rf.c` — `rlab_replay_done()`: die `tx_pos >= ref_sig.count`-Teilbedingung
  entfernt, es bleibt nur `subghz_devices_is_async_complete_tx(app->device)`.

Keine Aenderung an `rlab_replay_tx_cb()`, `rlab_replay_start()`/`rlab_replay_stop()` oder an
`rolllab_app.c` noetig — beide Aufrufer-Schleifen (`RLAB_REPLAYING`, `RLAB_PROBING`) rufen
`rlab_replay_done()` bereits unveraendert als einzige Abbruchbedingung neben `app->abort`
auf.

## Patch
Siehe
`app-improvements/RollLab/replay-done-premature-tx-stop-fix/rolllab_rf_c.patch`
(Unified-Diff gegen die unveraenderte `G4MEOVER18/RollLab`-Basis, `main`, Commit
`4ea80481`, `git apply --check` vorab gegen eine frische Kopie verifiziert, real angewendet
und per `md5sum` byte-identisch mit der beabsichtigten Endfassung sowohl ueber `patch -p1`
als auch ueber `git apply` abgeglichen).

## Testhinweis (ohne Hardware)
Eigenstaendiges, hostseitiges Reproduktionsprogramm in
`app-improvements/RollLab/repro-replay-done-premature-tx-stop/repro.c`, das die beiden
betroffenen Funktionen **woertlich** aus `rolllab_rf.c` uebernimmt
(`rlab_replay_tx_cb()`, `rlab_replay_done()` als ALT/NEU-Variante) und sie gegen einen
Mock-Treiber laufen laesst, der den dokumentierten Double-Buffering-Vertrag von
`subghz_devices_start_async_tx()`/`subghz_devices_is_async_complete_tx()` nachbildet:
jeder Timer-"Tick" schliesst die aktuell physisch laufende Dauer ab und ruft **danach**
per Callback die naechste vorab ab — exakt die Ein-Flanken-Verzoegerung, die den Fund
auslöst. Getestet fuer `N ∈ {1, 2, 16, 1024}` gespeicherte Flanken:

```sh
gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -O0 -o repro_O0 repro.c && ./repro_O0
gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -O1 -o repro_O1 repro.c && ./repro_O1
```

Bei mir lokal reproduziert, `-O0` und `-O1`, fuer jede getestete Groesse `N`:
- **ALT**: Schleife endet, nachdem der Treiber erst `N-1` von `N` Flanken physisch
  abgeschlossen hat — die letzte Flanke wird abgeschnitten (bei `N=1`, dem Extremfall, wird
  die einzige Flanke zu 100 % verworfen: die Schleife bricht ab, bevor der Treiber auch nur
  einen einzigen Tick ausfuehren konnte).
- **NEU**: Schleife endet erst, nachdem der Treiber alle `N` Flanken physisch vollstaendig
  abgeschlossen hat.

`ALL CHECKS PASSED`, keine Compiler-Warnungen, keine ASan-/UBSan-Befunde (erwartungsgemaess,
da kein Speicherfehler vorliegt — reiner Zustands-/Logikfehler im Abbruchkriterium).

Funktionaler Test nach dem Patch, lokal auf dem Flipper (mit CC1101-Empfaenger oder
Oszilloskop am Zielpin):
1. `ufbt` bauen, App starten, "Replay Attack" waehlen, ein Testsignal (z. B. ein eigener
   433,92-MHz-Taster) aufnehmen.
2. "Replay" ausloesen, die gesendete Flankenfolge mit der aufgenommenen vergleichen —
   insbesondere die letzte Flanke muss jetzt ihre volle, aufgezeichnete Dauer erreichen
   (vorher: deutlich verkuerzt oder ganz fehlend, je nach `N mod` Zeitverhalten).
3. Regressionspruefung: Abbruch per Zuruecktaste waehrend einer laufenden Wiedergabe bricht
   weiterhin sofort ab (unveraendert, `app->abort` greift unabhaengig von
   `rlab_replay_done()`).

## Risiko
- Niedrig: Aenderung entfernt ausschliesslich eine zusaetzliche, zu frueh wahre
  Teilbedingung aus einer bestehenden `||`-Verknuepfung — keine neue Variable, kein
  geaenderter Datentyp, keine andere Aufrufstelle betroffen.
- Kein Risiko eines haengenden Loops: `subghz_devices_is_async_complete_tx()` wird vom
  Treiber garantiert gesetzt, sobald `rlab_replay_tx_cb()` einmal `level_duration_reset()`
  zurueckgibt (das passiert bereits beim aktuellen Code zuverlaessig bei `tx_pos >= count`);
  zusaetzlich bleibt der manuelle Abbruch per Zuruecktaste (`app->abort`) als Notausstieg
  unveraendert bestehen.
- Kein Zusammenhang mit dem bereits dokumentierten `capture-timeout-not-wired`-Fund (betrifft
  die RX-Capture-Seite, `RLAB_CAP_TIMEOUT_MS`) oder der `otg-power-false-ownership`-/
  `analyze-silence-terminator-stats-skew`-Fixe (Power- bzw. Analyse-Pfad) — dieser Fund
  betrifft ausschliesslich die TX-Fertig-Erkennung.

## Quelle
- `G4MEOVER18/RollLab` (main, Commit `4ea80481`): `rolllab_rf.c` (`rlab_replay_tx_cb()`,
  `rlab_replay_done()`).
- `DarkFlippers/unleashed-firmware` (dev): `applications/system/secplus_pin/
  secplus_pin_radio.c` (`secplus_pin_radio_tx()`) als Referenz fuer den vorgesehenen
  Gebrauch von `subghz_devices_is_async_complete_tx()`.
