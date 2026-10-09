# g4meover-wifi-relay — Antwort-Pfad der Satelliten prüft überhaupt keinen Counter, jederzeit wiederholbar

## Was
Ich habe den zuletzt als "zurückgestellt" vermerkten `respCounter`-Punkt aus
`app-improvements/lilygo-ukfe-rx/rf-counter-replay-window-reset-on-reboot-fix.md` aufgegriffen
(dort als reiner Verfügbarkeits-Nebenfund — Selbstaussperrung nach Neustart — eingeordnet und
bewusst zurückgestellt). Bevor ich ihn wie vorgemerkt fixe, habe ich geprüft, *wer* `respCounter`
auf der Gegenseite überhaupt auswertet — und dabei festgestellt, dass die ursprüngliche
Einordnung nicht zutrifft: der Fund liegt woanders und ist größer.

`g4meover-wifi-relay` (der ESP32-WROOM-Koordinator, `src/main.cpp`) empfängt die signierten
`ukfe_rf`-Antworten aller Satelliten (`lilygo-ukfe-rx`, `heltec-ukfe-rx`) per ESP-NOW und reicht
sie unverändert an den Flipper-UART weiter:

```cpp
// ESP-NOW-Empfang (Antworten der Satelliten) -> an Flipper-UART weiterreichen
static void on_espnow_recv(const uint8_t* mac, const uint8_t* data, int len) {
    if (len <= 0 || len > (int)UKFE_RF_MAX_FRAME) return;
    UkfeRfMessage msg;
    // Antworten ohne Replay-Check parsen (NULL) — nur Integritaet zaehlt.
    if (ukfe_rf_parse_frame(RELAY_SECRET, data, (size_t)data[0] + 1, &msg, NULL)) {
        s_fromSatellite++;
        Serial2.write(data, len);   // zum Flipper zurueck
        ...
    }
}
```

`ukfe_rf_parse_frame(..., NULL)` deaktiviert die Anti-Replay-Prüfung vollständig (`last_counter`
ist `NULL` → der `if(last_counter) { ... }`-Block in `ukfe_rf.c` wird komplett übersprungen,
siehe `ukfe_rf.h`-Kommentar "`NULL fuer last_counter = ohne Replay-Check`"). Geprüft wird nur
noch Integrität (keyed MAC + CRC16), **nicht** Frische. Das bedeutet: `respCounter` in
`lilygo-ukfe-rx` (bzw. der analoge Counter in `heltec-ukfe-rx`) wird auf Relay-Seite **nie**
ausgewertet — weder vor noch nach einem Neustart. Damit entfällt der ursprünglich vermutete
Selbstaussperrungs-Effekt (ein RAM-Reset auf `0` ändert am Relay-Verhalten gar nichts, da dort
sowieso nicht verglichen wird), **aber** das eigentliche Problem ist gravierender: ein
Angreifer, der per ESP-NOW-Sniffer (unverschlüsselt, `encrypt = false`) irgendwann ein gültig
signiertes Antwort-Frame eines Satelliten mitschneidet, kann dieses Frame **jederzeit, beliebig
oft, ohne jeden Geräte-Neustart** erneut einspielen — der Relay nimmt es jedes Mal an und
reicht es unverändert an den Flipper-UART weiter.

**Warum das (trotz "nur eine Statusanzeige") relevant ist:** Der durchgereichte Inhalt ist kein
Freitext, sondern ein strukturiertes `UkfeRfRespAck`/`RespStatus`/`RespScanHit`-Frame
(`args[0]=orig_cmd, args[1]=result` bei Ack). Ein Operator, der sich während eines Pentests auf
die am Flipper angezeigte Rückmeldung verlässt ("Befehl X erfolgreich ausgeführt", Scan-Treffer,
Satelliten-Status), kann durch einen beliebig oft wiederholbaren, aber inhaltlich veralteten
Status getäuscht werden — z. B. ein längst überholtes `result=0` (Erfolg) für einen Befehl, der
beim jetzigen Einsatz in Wahrheit fehlgeschlagen ist, oder ein alter `RespScanHit` als
scheinbar aktueller Fund. Das untergräbt den Zweck der signierten Rückmeldung (Operator soll
sich auf das verlassen können, was am Flipper ankommt) und ist ein eigenständiger Fund,
unabhängig vom ursprünglich vermuteten `respCounter`-Neustart-Fall.

**Warum bisher `NULL` (bewusste Design-Entscheidung, kein Versehen):** Ein einzelnes,
gemeinsames `last_counter`-Fenster über *alle* Satelliten hinweg hätte exakt das bereits in
`app-improvements/ukfe-esp-now-mesh/espnow-origin-tag-counter-window-split-fix.md` dokumentierte
Kollisionsproblem: jeder Satellit führt seinen eigenen, unabhängigen `respCounter`/`enowCounter`
ab eigenem Startwert. Ein hoher Counter-Stand von Satellit A würde dann jede spätere (aber
eigenständig niedriger zählende) Antwort von Satellit B als "nicht neu genug" verwerfen. Der
`NULL`-Pfad umgeht dieses Kollisionsproblem korrekt — löst aber damit das Replay-Problem gar
nicht, sondern deaktiviert die Prüfung komplett.

## Nutzen
- Schließt eine bisher komplett ungeprüfte Replay-Lücke im Antwort-Pfad (nicht nur ein
  Neustart-Zeitfenster wie bei den bereits behobenen Befehls-Richtungen) — passt in den in
  CONSTRAINTS Punkt 5 verlangten defensiven Rahmen (signierte Rückmeldungen sollen sich auf
  Frische verlassen lassen, nicht nur auf Integrität).
- Löst das Kollisionsproblem korrekt, das `NULL` ursprünglich motiviert hat: ein eigenes
  Zähler-Fenster je Absender-MAC (`sat_counter_slot()`) statt einem gemeinsamen Fenster oder
  gar keinem — jeder Satellit bleibt unabhängig, keine gegenseitige Blockade.
- Rein additiv und ohne NVS/`Preferences`: anders als die bisherigen Neustart-Funde ist hier
  kein Neustart-Szenario zu lösen (ein Relay-Neustart leert die Tabelle, der erste Kontakt
  jedes Satelliten startet dann einfach wieder bei `0` und wird von dessen tatsächlichem, stets
  höherem Counter sofort überholt — kein Lockout, siehe Testhinweis).
- Deckt automatisch alle aktuellen *und* künftigen Satelliten ab, ohne dass pro Gerät ein
  eigener Fix nötig wäre — die Änderung sitzt zentral am einzigen Empfangspunkt aller
  Antworten.
- `respCounter` (`lilygo-ukfe-rx`) und der analoge Antwort-Counter in `heltec-ukfe-rx` selbst
  bleiben unverändert — der Fix sitzt ausschließlich auf der empfangenden (Relay-)Seite, keine
  Satelliten-Firmware muss angepasst werden.

## Betroffene Dateien
- `src/main.cpp` — neue feste Tabelle `s_satCounters[8]` (MAC → eigener `last_counter`),
  neue freie Funktion `sat_counter_slot()`, `on_espnow_recv()` übergibt deren Rückgabewert statt
  `NULL`, neuer Statistik-Zähler `s_satRejected` (plus `cli_status()`-Zeile).

## Patch
Unified-Diff gegen die unveränderte `G4MEOVER18/g4meover-wifi-relay`-Basis (`main`, Commit
`9b4a9e1`; `git apply --check` gegen eine frische, unabhängige Zweitkopie verifiziert, real
angewendet und byte-für-byte gegen die beabsichtigte Endfassung abgeglichen — identisch) in
[`sat-resp-ack-replay-missing-counter-check-fix/main_cpp.patch`](sat-resp-ack-replay-missing-counter-check-fix/main_cpp.patch).

Einziger inhaltlicher Eingriff: Ursprung und Prüfung des Anti-Replay-Fensters für
ESP-NOW-Antworten in `main.cpp`. `ukfe_rf.c`/`.h` (Frame-Format, MAC, CRC, die
`counter <= last_counter`-Prüfung selbst) bleiben unverändert — der Fix nutzt ausschließlich die
bereits vorhandene `last_counter`-Parameterschnittstelle von `ukfe_rf_parse_frame()`.

## Testhinweis (ohne Hardware)
[`repro-sat-resp-ack-replay-missing-counter-check/repro.c`](repro-sat-resp-ack-replay-missing-counter-check/repro.c)
nutzt die unveränderten Original-Dateien `ukfe_rf.c`/`ukfe_rf.h` (1:1 aus `g4meover-wifi-relay`
übernommen, laut eigenem Dateikommentar "plattformunabhaengig, ohne HW-Abhaengigkeit") plus eine
1:1-Kopie der neuen `sat_counter_slot()`-Logik aus dem Patch — keine ESP32/Arduino-Abhängigkeit
nötig:

```
$ gcc -std=c11 -O0 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -o repro repro.c ukfe_rf.c
$ ./repro
Szenario 1 (Ist-Zustand, on_espnow_recv mit NULL):
  5/5 reguläre ACKs akzeptiert (sollte 5 sein)
  BUG:  mitgeschnittenes ACK (Counter 3, orig_cmd=WifiDeauth) 10x erneut eingespielt -> 10/10 ANGENOMMEN (...)

Szenario 2 (Fix, eigenes Fenster je Absender-MAC):
  FIX:  5/5 reguläre ACKs von Satellit A akzeptiert (sollte 5 sein)
  FIX:  dasselbe mitgeschnittene ACK (Counter 3) erneut eingespielt -> abgelehnt (...)
  FIX:  Satellit B, erstes ACK (eigener Counter 1, trotz Satellit-A-Stand 5) -> angenommen (...)
  FIX:  Satellit A, neuer legitimer Befehl (Counter 6) -> angenommen (...)

Szenario 3 (Fix, Tabelle voll -- Fallback-Pfad):
  8/8 Slots fuer 8 unterschiedliche Absender belegt (sollte alle sein)
  (SAT_COUNTER_TABLE_SIZE+1)-ter unbekannter Absender -> Slot == NULL (...)
  Integritaet im Fallback-Pfad weiterhin geprueft (gueltiges Frame) -> angenommen (...)

Alle Szenarien bestaetigt (exit code 0)
```

Identisch unter `-O1` verifiziert (byte-für-byte gleicher Output), in beiden Fällen keine
ASan-/UBSan-Meldungen und keine Compiler-Warnungen. Das Repro baut mit der echten
`ukfe_rf_build_frame()`/`ukfe_rf_parse_frame()`-Funktion reguläre ACK-Frames, schneidet eines
davon ("vom Angreifer per ESP-NOW-Sniffer mitgeschnitten") mit und zeigt sowohl den Ist-Zustand
(beliebig oft wiederholbar, unabhängig vom späteren Counter-Stand) als auch die Fix-Logik
(Replay abgelehnt, zwei unabhängig zählende Satelliten blockieren sich nicht gegenseitig, neue
legitime Befehle werden weiterhin angenommen, der dokumentierte Fallback bei voller Tabelle
bleibt sicher).

Der eigentliche Patch (`main.cpp`) wurde nicht gegen echte Hardware kompiliert (benötigt
PlatformIO + ESP32-Toolchain), ist aber als rein additive Änderung an bereits bestehendem Code
gehalten (eine neue feste Tabelle, eine neue freie Funktion, ein geänderter Funktionsaufruf-
Parameter, ein neuer Statistik-Zähler) und ändert keine bestehende Kontrollstruktur. Lokaler
Funktionstest: PlatformIO-Build, Relay mit mindestens einem Satelliten (`lilygo-ukfe-rx` oder
`heltec-ukfe-rx`) betreiben, am Flipper einen Befehl auslösen und die ACK-Anzeige prüfen; mit
einem zweiten ESP32 im Promiscuous-Modus ein zuvor mitgeschnittenes Satelliten-Antwort-Frame per
ESP-NOW an den Relay erneut einspielen — vor dem Fix erscheint die (veraltete) Rückmeldung am
Flipper erneut, nach dem Fix bleibt sie aus (`resp rejected`-Zähler in `status` steigt).
Zusätzlich: zwei Satelliten gleichzeitig betreiben (unterschiedliche MACs) und gegenprüfen, dass
beide weiterhin unabhängig voneinander ACKs durchreichen; Relay-Neustart mitten in einer
laufenden Sitzung (erster Kontakt jedes Satelliten nach dem Neustart muss sofort angenommen
werden, kein Lockout).

## Risiko
- Niedrig: rein additiv in `src/main.cpp` (eine neue feste Tabelle mit fixer Größe
  `SAT_COUNTER_TABLE_SIZE = 8`, eine neue freie Funktion, ein geänderter Parameter im
  bestehenden `ukfe_rf_parse_frame()`-Aufruf, ein neuer Statistik-Zähler). Wire-Format,
  MAC/CRC-Berechnung und die Anti-Replay-Regel selbst (`ukfe_rf.c`) sind unverändert.
- Feste Tabellengröße statt dynamischer Allokation (passend zum übrigen, allokationsfreien Stil
  der Datei). `8` deckt die aktuell bekannten zwei Satelliten (`lilygo-ukfe-rx`,
  `heltec-ukfe-rx`) mit großem Spielraum für künftige Geräte ab. Bei mehr als acht
  unterschiedlichen Absender-MACs (derzeit praktisch ausgeschlossen) fällt `sat_counter_slot()`
  für jede weitere, neue MAC auf `NULL` zurück — exakt das bisherige, dokumentierte Verhalten
  (ungeprüft statt grundlos abgewiesen oder Pufferüberlauf), kein neues Fehlverhalten.
- Kein NVS/`Preferences`-Bezug nötig und damit kein Neustart-Lockout-Risiko wie bei den anderen
  bereits behobenen Richtungen: die Tabelle startet nach jedem Relay-Neustart leer, der erste
  Kontakt jedes Satelliten wird dann wie ein neuer Absender behandelt (Startwert `0`) und von
  dessen tatsächlichem, stets höherem Counter sofort überholt.
- Verändert nicht `relay_frame()`/die Flipper→Satelliten-Richtung — ausschließlich der
  ESP-NOW-Empfang der Satelliten-*Antworten* ist betroffen.
- Löst **nicht** das ursprünglich für `respCounter` vermutete Neustart-Szenario, weil dieses
  Szenario — wie oben gezeigt — in Wirklichkeit nie bestanden hat (der Relay hat `respCounter`
  nie ausgewertet). Der `respCounter`-Backlog-Punkt in `lilygo-ukfe-rx` kann damit als
  gegenstandslos geschlossen werden, siehe `BACKLOG.md`.

## Quelle
- `G4MEOVER18/g4meover-wifi-relay` (main, Commit `9b4a9e1`): `src/main.cpp`
  (`on_espnow_recv()`, `RELAY_SECRET`, `cli_status()`), `src/ukfe_rf.c`/`.h`
  (`ukfe_rf_parse_frame`, Kommentar "`NULL fuer last_counter = ohne Replay-Check`").
- `G4MEOVER18/lilygo-ukfe-rx` (main, Commit `aa8a285`): `src/main.cpp` (`respCounter`,
  `send_espnow_ack()`) — Ursprung des hier richtiggestellten Backlog-Punkts.
- `G4MEOVER18/g4meover-ecosystem`:
  `app-improvements/lilygo-ukfe-rx/rf-counter-replay-window-reset-on-reboot-fix.md` (dort die
  ursprüngliche, hier korrigierte Einordnung als reiner Verfügbarkeits-Nebenfund),
  `app-improvements/ukfe-esp-now-mesh/espnow-origin-tag-counter-window-split-fix.md` (Ursprung
  der hier wiederverwendeten Idee getrennter Zählerräume je Quelle, dort per Tag statt MAC).
