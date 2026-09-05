# RollForge — RollJam behauptet "B saved", bevor Signal B je gespeichert wurde

## Was
Ich habe `module_rolljam.c` (RollJam-Modul in RollForge) gegen die eigene
Statuslogik geprüft, nachdem der letzte Fund im selben Modul der
Doppel-Stop-Crash im Jammer-Modul war (anderes Modul derselben App). Fund:
Der Übergang von Phase 2 RX (`RJ_P2_RX`) zu Replay (`RJ_REPLAY`) setzt sofort
die Statuszeile `"Replaying A... (B saved)"` — obwohl an dieser Stelle noch
**kein** `rollforge_save_sig()`-Aufruf für Signal B stattgefunden hat:

```c
case RJ_P2_RX:
    if(m->sig_b.ready && m->sig_b.count >= RF_MIN_EDGES) {
        rj_cap_stop(app);
        // Immediately start replay
        m->tx_pos = 0; s_rep = app;
        rollforge_rf_start_tx(app, rj_replay_cb, NULL);
        app->rf_op = RF_REPLAYING;
        m->phase = RJ_REPLAY;
        snprintf(m->status, sizeof(m->status), "Replaying A... (B saved)");
        ...
```

Per `grep -n rollforge_save_sig module_rolljam.c` gibt es im gesamten Modul
genau zwei Aufrufstellen, beide in `rj_input()` und beide an einen manuellen
Tastendruck gebunden:

```c
} else if(ev->key == InputKeyDown) {
    if(m->phase == RJ_CAPTURED && m->sig_a.count > 0) {
        rollforge_save_sig((const RfSig*)&m->sig_a, app->frequency, "rj_A");
        ...
    } else if(m->phase == RJ_DONE && m->sig_b.count > 0) {
        rollforge_save_sig((const RfSig*)&m->sig_b, app->frequency, "rj_B");
```

Signal B wird also nur dann tatsächlich auf die SD-Karte geschrieben, wenn
ich nach Abschluss des Replays (`RJ_DONE`) explizit `[v]` (Down) drücke. Der
Übergang nach `RJ_REPLAY` behauptet mit `"(B saved)"` aber bereits vorher das
Gegenteil. Zwischen diesem Zeitpunkt und `RJ_DONE` liegt der komplette
Replay-Vorgang (Übertragung von Signal A, Dauer abhängig von dessen
Flankenzahl) — währenddessen ist `[Back]` jederzeit bedienbar:
`rollforge_input_cb()` setzt `app->abort = true` für jeden `InputKeyBack`
unabhängig von Modul/Phase, und die Haupt-Loop ruft darauf in jedem
Nicht-Menü-Zustand `mod_leave()` → `rj_exit()` → `rj_rf_clean()` auf, die
ausschließlich RF sauber stoppt (`s_jam = false; s_cap = NULL; s_rep = NULL;
rollforge_rf_idle(app);`) — **kein** Aufruf von `rollforge_save_sig()`
irgendwo in diesem Pfad. Drücke ich also `[Back]`, während `"Replaying A...
(B saved)"` angezeigt wird (das plausibelste Bedienmuster, sobald ich glaube,
B sei bereits gesichert und A allein noch übertragen wird), geht Signal B
ersatzlos verloren — es existiert ausschließlich in `m->sig_b` im RAM, und
`rj_enter()` beim nächsten Start des Moduls setzt nur `phase`/`status`
zurück, ohne `sig_a`/`sig_b` zu löschen, aber der nächste vollständige
Durchlauf überschreibt `sig_b` beim Erreichen von `RJ_P2_JAM`
(`rj_phase_start()`: `sig->count = 0; sig->ready = false;`) ohnehin, bevor
ich je wieder Gelegenheit zum Speichern hätte.

Das trifft den eigentlichen Zweck des Moduls direkt: Bei einem klassischen
RollJam-Ablauf ist B (der zweite, während des Jammens abgefangene Code) der
Wert, den ich für einen **späteren** Zugriff benötige (der nächste vom
Empfänger erwartete gültige Code, da A ja gerade repliziert/"verbraucht"
wird) — genau der Wert, den die Statuszeile fälschlich schon als gesichert
ausweist.

## Nutzen
- Schließt ein reales Datenverlust-Fenster: der plausibelste Bedienschritt
  ("B ist ja schon gespeichert, ich breche ab") führt aktuell zum
  unwiderruflichen Verlust genau des Codes, um den es beim RollJam-Angriff
  geht.
- Die Statuszeile wird wahrheitsgemäß: `"(B saved)"` erscheint nur noch, wenn
  der Schreibvorgang tatsächlich stattgefunden und laut Rückgabewert von
  `rollforge_save_sig()` auch geklappt hat; schlägt er fehl (SD-Karte fehlt/
  ist voll), zeigt die Zeile das ehrlich an, statt Erfolg vorzutäuschen —
  aktuell würde ALT in diesem Fall exakt denselben (falschen) Text zeigen,
  da der Rückgabewert nirgends geprüft wird.
- Der bereits vorhandene manuelle Save-Weg über `[v]` in `RJ_DONE` bleibt
  unverändert erhalten (harmlose erneute Speicherung unter neuem
  Zeitstempel, falls der automatische Save fehlgeschlagen war oder ich die
  Datei zusätzlich unter einem neuen Namen behalten will).
- Reine Ergänzung eines bereits vorhandenen Funktionsaufrufs an einer neuen
  Stelle im selben Modul — keine Änderung an `rollforge_storage.c`,
  `rollforge_rf.c` oder den anderen RollForge-Modulen.

## Betroffene Dateien
- `module_rolljam.c` — `rj_tick()`, Zweig `RJ_P2_RX` (Übergang nach
  `RJ_REPLAY`): ein zusätzlicher `rollforge_save_sig()`-Aufruf für `sig_b`,
  vor dem `snprintf()` der Statuszeile.

## Port-Skizze / Patch

### `module_rolljam.c` — `rj_tick()`, Zweig `RJ_P2_RX`
```c
case RJ_P2_RX:
    if(m->sig_b.ready && m->sig_b.count >= RF_MIN_EDGES) {
        rj_cap_stop(app);
        // Signal B jetzt sichern -- ab hier kann [Back] jederzeit abbrechen,
        // und der bisherige manuelle Save-Weg (RJ_DONE, [v]) wird erst nach
        // dem kompletten Replay erreicht.
        bool b_saved = rollforge_save_sig((const RfSig*)&m->sig_b, app->frequency, "rj_B");
        // Immediately start replay
        m->tx_pos = 0; s_rep = app;
        rollforge_rf_start_tx(app, rj_replay_cb, NULL);
        app->rf_op = RF_REPLAYING;
        m->phase = RJ_REPLAY;
        snprintf(m->status, sizeof(m->status),
            b_saved ? "Replaying A... (B saved)" : "Replaying A... (B SAVE FAILED!)");
        ...
```
Einzige Änderung: der neue `rollforge_save_sig()`-Aufruf plus die
Bedingung um den bisherigen `snprintf()`. `rj_input()`s bestehender
`InputKeyDown`-Zweig für `RJ_DONE` bleibt unverändert bestehen (weiterhin
manuell erneut speicherbar, z. B. nach einem fehlgeschlagenen Auto-Save).

## Testhinweis (ohne Hardware)
Reine Kontrollfluss-/Zustandslogik, host-nachstellbar ohne echten
CC1101-Treiber oder echte Storage-API.
`repro-rj-b-premature-saved-claim/repro.c` bildet die relevanten
Übergänge (`RJ_P2_RX` → `RJ_REPLAY` sowie den `[Back]`-Abbruchpfad über
`rj_exit()`/`rj_rf_clean()`) nach, mit `rollforge_save_sig()` als einfachem
Flag-Mock (`disk_has_B`), und prüft vier Fälle:

```
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c
$ ./repro
ALT  (SD ok, sofortiger Abbruch): status="Replaying A... (B saved)", disk_has_B=0 (Status behauptete "B saved")
NEU  (SD ok, sofortiger Abbruch): status="Replaying A... (B saved)", disk_has_B=1
NEU  (SD ok, kein Abbruch, RJ_DONE erreicht): disk_has_B=1
NEU  (SD fehlt): status="Replaying A... (B SAVE FAILED!)", disk_has_B=0

Alle Faelle wie erwartet.
```
ALT zeigt exakt die im Fund beschriebene Lücke: der Statustext behauptet
`"(B saved)"`, aber nach einem sofortigen Abbruch ist `disk_has_B` weiterhin
`0` — B war nie auf der "SD-Karte". NEU speichert vor dem Statustext und vor
jeder Abbruchmöglichkeit (`disk_has_B=1` bereits vor dem simulierten
`[Back]`), der Regressionsfall (kein Abbruch, normaler Ablauf bis `RJ_DONE`)
bleibt unverändert erfolgreich, und ein simulierter SD-Fehler wird als
`"SAVE FAILED"` gemeldet statt verschwiegen.

Funktionaler Test danach lokal auf dem Flipper: RollForge > RollJam, Phase 1
und 2 wie gewohnt durchlaufen (zwei simulierte Fob-Tastendrücke), sobald
`"Replaying A... (B saved)"` erscheint sofort `[Back]` drücken, danach
`/ext/subghz/rollforge/` auf der SD-Karte prüfen — vor dem Fix fehlt die
`rj_B_*.sub`-Datei vollständig, nach dem Fix liegt sie bereits vor, obwohl
das Replay abgebrochen wurde. Zusätzlich Normalablauf ohne Abbruch prüfen
(bis `RJ_DONE`, dann `[v]`): weiterhin genau wie vorher eine (zweite,
neu zeitgestempelte) `rj_B_*.sub`-Datei.

## Risiko
- Niedrig: ein zusätzlicher, bereits an anderer Stelle im selben Modul
  vorhandener Funktionsaufruf (`rollforge_save_sig()`) an einer neuen
  Stelle, keine neue Abhängigkeit, keine Änderung der Dateibenennung
  (weiterhin `"rj_B"`-Hint plus Zeitstempel wie beim bisherigen manuellen
  Save).
- Der Schreibvorgang verschiebt sich um einen Tick nach vorn (vor Start des
  Replays statt danach) — `storage_file_open/write/close` auf der SD-Karte
  braucht messbar Zeit; dadurch beginnt die eigentliche Signalübertragung
  (`rollforge_rf_start_tx()`) minimal später als vorher. Da RollJam ohnehin
  bereits zwei Jam-/RX-Fenster (180 ms/80 ms) pro Phase durchläuft und der
  Replay selbst nicht zeitkritisch gegenüber einem festen externen Fenster
  ist (der Empfänger wartet auf die nächste beliebige gültige Übertragung,
  kein Hardware-Timeout im Modul selbst), ist diese Verzögerung praktisch
  unkritisch.
- Schreibt jetzt bei **jedem** abgeschlossenen Phase-2-Durchlauf automatisch
  eine Datei, auch wenn ich anschließend nie bis `RJ_DONE` komme oder das
  Replay danach ohnehin normal beende und zusätzlich manuell erneut
  speichere — führt im Erfolgsfall zu bis zu einer zusätzlichen Datei pro
  Durchlauf gegenüber ALT (harmloser Mehrverbrauch an SD-Speicher, kein
  Datenverlust; das bisherige Verhalten war das eigentliche Risiko, nicht
  dieses).
- Deckt nur den beschriebenen `RJ_P2_RX`-Übergang ab; Signal A folgt keinem
  vergleichbaren Muster (dessen Statuszeile bei `RJ_CAPTURED`
  — `"A: %lu edges -- [OK] Ph2"` — behauptet an keiner Stelle, A sei bereits
  gespeichert, das bleibt wie bisher rein manuell über `[v]`).

## Quelle
- `G4MEOVER18/RollForge` (main): `module_rolljam.c` (`rj_tick`, `rj_input`,
  `rj_exit`, `rj_rf_clean`), `rollforge_app.c`
  (`rollforge_input_cb`, `mod_leave`), `rollforge_storage.c`
  (`rollforge_save_sig`).
