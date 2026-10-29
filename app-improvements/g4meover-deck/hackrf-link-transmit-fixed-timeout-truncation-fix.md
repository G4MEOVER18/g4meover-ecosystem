# g4meover-deck — hackrf_link.transmit() kappt lange Out-of-Band-Replays nach fest 120s statt nach der tatsächlichen Sendedauer

## Was
Ich habe `deck/hackrf_link.py` durchgesehen, eines der seit der `2026-10-13`-Sitzung
als Rückstellpunkt benannten, noch nicht geprüften Module (`device_discovery.py`,
`handshake_capture.py`, `hackrf_link.py`, `deck/tools/wpa_crack.py`).
`device_discovery.py` (reset-sicheres Öffnen, Erkennung) ohne tragfähigen eigenen
Fund durchgesehen — alle `open_safe()`-Aufrufer verwenden ausschließlich
`reset_safe=True`-Geräte, kein Missbrauch gefunden.

`hackrf_link.py` wrappt die `hackrf`-CLI-Tools für die HackRF One, u. a. für
Out-of-Band-Replay außerhalb der Flipper-/ESP-Bänder (laut eigenem Modul-Docstring
und der `OUT_OF_BAND`-Tabelle explizit auch für GPS-L1-Simulation, `1575.42` MHz).
`capture()` leitet seinen `subprocess.run(...)`-`timeout` korrekt aus der
Capture-Dauer ab (`timeout=seconds + 15`) — die tatsächliche Empfangsdauer ist durch
`-n <sample_rate*seconds>` hart begrenzt, der Parameter `seconds` bestimmt also
exakt, wie lange der Prozess laufen kann, und die 15s sind reiner Start/Flush-Puffer.

`transmit()` hat dagegen **keinen** Dauer-Parameter und berechnet auch keine
Sendedauer aus der Datei — es sendet die komplette `path`-Datei einmal
(`hackrf_transfer -t path`, ohne `-R`/Repeat), deren Länge allein die tatsächliche
Sendezeit bestimmt, ruft `subprocess.run()` aber mit einem fest verdrahteten
`timeout=120` auf, der mit der Dateigröße überhaupt nicht in Beziehung steht:

```python
r = subprocess.run(["hackrf_transfer", "-t", path, "-f", str(int(freq_mhz * 1e6)),
                    "-s", str(sample_rate), "-x", "20"],
                   capture_output=True, text=True, timeout=120)
```

Bei Standard-`sample_rate=8_000_000` (8-Bit-I/Q, 2 Byte/Sample) entsprechen 120
Sekunden Sendezeit rund 1,92 GB Dateigröße. Jede längere IQ-Aufnahme — und
GPS-SDR-Sim-Aufnahmen für die im selben Modul dokumentierte `gps_l1`-Simulation
laufen in der Praxis typischerweise mehrere Minuten, weit über dieser Grenze —
wird von `subprocess.run()` nach 120s zwangsweise gekillt, **bevor** die Datei
vollständig gesendet ist (abgeschnittenes Replay). Zusätzlich fängt `transmit()`
die dabei von `subprocess.run()` ausgelöste `subprocess.TimeoutExpired`-Exception
nicht auf — die Funktion stürzt in diesem Fall komplett ab, statt wie dokumentiert
(Rückgabetyp `-> bool`) ein `False` zu liefern. Jeder Aufrufer, der nur `bool`
erwartet (wie es die Signatur verspricht) und keinen eigenen `try/except` um den
Aufruf legt, crasht mit.

## Nutzen
- Behebt eine Diskrepanz zwischen dem dokumentierten Rückgabetyp (`-> bool`, "liefert
  Erfolg/Fehlschlag") und dem tatsächlichen Verhalten bei langen Dateien (unbehandelte
  Exception statt `False`).
- Macht Out-of-Band-Replays jeder Länge möglich, die durch das zugrunde liegende
  Protokoll ohnehin schon abgedeckt sind (GPS-Sim, ACARS/AIS-Log-Replay, lange
  Sub-GHz-Captures) — vorher war jede Wiedergabe über ~120s (bei 8 Msps) strukturell
  unmöglich, unabhängig vom tatsächlichen Anwendungsfall.
- `capture()` bleibt unverändert, da dort die Sendedauer (hier: Empfangsdauer) bereits
  korrekt aus dem eigenen `seconds`-Parameter abgeleitet wird — kein paralleler Fund.

## Betroffene Dateien
- `deck/hackrf_link.py` — `transmit()`: Timeout wird jetzt aus der tatsächlichen
  Dateigröße (`os.path.getsize(path)`) und `sample_rate` abgeleitet
  (`air_seconds + 15`, mindestens 30s als Sicherheitsboden für sehr kleine Dateien);
  `subprocess.TimeoutExpired` wird aufgefangen und liefert `False` statt die Exception
  durchzulassen. Neuer `import os` ergänzt. Keine Änderung an `detect()`, `sweep()`
  oder `capture()`.

## Patch
`hackrf-link-transmit-fixed-timeout-truncation-fix/hackrf_link_py.patch`
(Unified-Diff gegen `deck/hackrf_link.py`). Gegen zwei frische, unabhängige
Zweitkopien der unveränderten `G4MEOVER18/g4meover-deck`-Basis (`b030e45`) mit
`git apply --check` verifiziert, real angewendet, Ergebnis per `md5sum` gegen die
hier geprüfte Arbeitskopie abgeglichen — identisch. Anschließend
`python3 -m py_compile deck/hackrf_link.py` auf der gepatchten Datei bestanden.

## Testhinweis (ohne Hardware)
Host-Repro (`repro-hackrf-link-transmit-fixed-timeout-truncation/repro.py`):
`transmit()`s Logik als `transmit_alt()`/`transmit_neu()`-Funktionspaar
transkribiert, `subprocess.run()` durch einen `MockProc` ersetzt, der
`subprocess.TimeoutExpired` wirft, sobald der übergebene `timeout` kürzer ist als
eine konfigurierte tatsächliche Sendezeit (`air_seconds`), sonst den konfigurierten
`returncode` liefert — kein HackRF-/Hardwarezugriff nötig. **9 Prüfungen**: `ALL
CHECKS PASSED (9 total)`. Geprüft:
1. Regressionsfall — kurze Datei (10s Sendezeit): ALT und NEU senden beide
   erfolgreich; NEU bekommt dafür den 30s-Mindest-Timeout (nicht die alten 120s).
2. Der eigentliche Fund — 300s-Datei (realistische GPS-Sim-Replay-Länge, deutlich
   über der alten 120s-Grenze): ALT stürzt mit `TimeoutExpired` ab; NEU sendet
   vollständig durch, mit einem auf `air_seconds + 15 = 315s` berechneten Timeout.
3. Ein Transfer, der selbst länger braucht als der neu berechnete Timeout (z. B.
   durch USB-Stottern): NEU liefert `False` statt eine Exception durchzulassen —
   der Fix tauscht "abstürzen" gegen "korrekt `False` melden", erfindet aber keine
   unbegrenzte Wartezeit.
4. Echter Tool-Fehlschlag (`returncode=1`, unabhängig vom Timeout): beide Varianten
   liefern unverändert `False` — kein neues Fehlverhalten.

Lokal zusätzlich sinnvoll: `hackrf_transfer` mit einer selbst aufgenommenen,
mehrminütigen IQ-Datei an einer abgeschirmten Dummy-Last/ohne Antenne testen (reine
Verifikation, dass die Übertragung jetzt bis zum Dateiende läuft statt vorzeitig
gekillt zu werden) — ausdrücklich nur in abgeschirmter Umgebung, siehe
Sicherheitsrahmen.

## Risiko
- Der neue Mindest-Timeout von 30s ändert das Verhalten für sehr kurze Dateien
  (< 15s Sendezeit) nicht sichtbar — sie liefen vorher ebenfalls innerhalb der
  120s-Grenze durch, laufen jetzt innerhalb von 30s durch; `returncode`-Auswertung
  unverändert.
- `os.path.getsize()` auf einem nicht existierenden/nicht lesbaren Pfad wirft
  `OSError`, die ich explizit auffange (`air_seconds = 0` als Fallback) — in diesem
  Fall greift der 30s-Mindest-Timeout, und `hackrf_transfer` selbst meldet dann über
  seinen `returncode` den eigentlichen Dateifehler; kein unbehandelter Absturz an
  dieser Stelle.
- Keine Breaking Change an der Funktionssignatur (`path`, `freq_mhz`, `sample_rate`
  unverändert) — bestehende Aufrufer sind von außen nicht betroffen. Aktuell gibt es
  im Repo ohnehin keinen Aufrufer von `transmit()` (nur `detect()`/`sweep()` werden
  von `scenario_runner.py` genutzt) — der Fix schließt die Lücke, bevor ein künftiger
  Aufrufer (Dashboard, CLI) sie trifft.

## Offen (bewusst nicht mitbehoben)
- `handshake_capture.py` und `deck/tools/wpa_crack.py` (CLI-Frontend) bleiben für
  diese Sitzung ungeprüfte Kandidaten.
- `sweep()`s `one_shot=False`-Dauerlauf-Pfad hat dieselbe Fundklasse latent in sich
  (fester `timeout=30` unabhängig von der tatsächlichen Sweep-Dauer bei
  kontinuierlichem Sweep) — aktuell aber folgenlos, da kein Aufrufer im Repo
  `one_shot=False` übergibt (`scenario_runner.py` nutzt ausschließlich den
  Default) und `scenario_runner.py`s eigener `except Exception`-Fänger eine
  `TimeoutExpired` ohnehin als `"error"`-Envelope abfangen würde, statt den
  Scenario-Lauf abzureißen. Bewusst als separater, kleinerer Rückstellpunkt
  vermerkt statt hier mitgefixt, da `sweep()`s Timeout-Problem strukturell anders
  liegt (keine Dateigröße verfügbar, nur eine Sweep-Dauer, die der Aufrufer gar
  nicht übergibt) und ein eigener Design-Entscheid wäre (z. B. ein neuer
  `duration_s`-Parameter für den Dauerlauf-Fall).

## Quelle
- `G4MEOVER18/g4meover-deck` (main, `b030e45`): `deck/hackrf_link.py`
  (`capture()` — Gegenprobe für die korrekt abgeleitete Variante, `sweep()`,
  `transmit()`), `deck/scenario_runner.py` (`_run_sdr()` — Gegenprobe für
  "kein Aufrufer von `transmit()`", `except Exception`-Fänger um `sweep()`),
  `deck/device_discovery.py` (`open_safe()`, `reset_safe`-Flags — Gegenprobe für
  den zuerst geprüften, folgenlosen Kandidaten).
