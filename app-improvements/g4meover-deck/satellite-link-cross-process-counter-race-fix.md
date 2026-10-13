# g4meover-deck — SatelliteLink.send() schützt die counter_file nicht über Prozessgrenzen

## Was
Ich habe den bisher zweimal als eigenständiger, größerer Fund zurückgestellten Punkt
aus `satellite-link-counter-saved-after-send-fix.md` aufgegriffen: `deck_daemon` (ein
langlaufender `systemd`-Dienst mit genau einem, beim Prozessstart erzeugten
`SatelliteLink`-Objekt) und jeder einzelne `deck-ctl sat ...`-Aufruf (ein eigener,
kurzlebiger Prozess, der sich über `make_link()` sein eigenes, frisches
`SatelliteLink`-Objekt baut) teilen sich standardmäßig dieselbe `counter_file`
(`/var/lib/g4meover-deck/counter`, aus `deck.conf` bzw. `DECK_COUNTER_FILE`) — ohne
jedes Datei-Locking über die Prozessgrenze hinweg:

```python
class SatelliteLink:
    def __init__(self, ...):
        ...
        self._counter = self._load_counter()   # NUR bei __init__, einmalig

    def send(self, cmd, args=b""):
        with self._lock:                        # threading.Lock -- nur INNERHALB
            self.open()                          # dieses einen Prozesses wirksam
            self._counter += 1
            frame = ukfe_rf.build_frame(self.secret, self._counter, cmd, args)
            self._save_counter()
            self._serial.write(frame)
            self._serial.flush()
            return self._counter
```

`self._lock` ist ein `threading.Lock` — serialisiert zuverlässig mehrere Threads
*innerhalb* eines Prozesses (relevant für `deck_daemon`s `ThreadingHTTPServer`, der je
HTTP-Request einen eigenen Thread startet), schützt aber nichts zwischen zwei
*verschiedenen* `SatelliteLink`-Instanzen in zwei verschiedenen Prozessen. `self._counter`
lebt nur im RAM der jeweiligen Instanz und wird nach `__init__` nie wieder von der
Platte neu eingelesen.

Lauft `deck_daemon` dauerhaft (üblicher Betrieb laut `README.md`/`deck_daemon.py`s
eigener Doku, "läuft als systemd-Dienst") und wird währenddessen — wie es jeder Nutzer
des ausdrücklich für genau diesen Zweck gebauten `deck-ctl`-CLI jederzeit tun kann/soll
(`deck_ctl.py`s eigene Doku: "Ein Werkzeug für beide Ebenen des Decks") — zusätzlich ein
`deck-ctl sat ...`-Befehl ausgeführt, geschieht Folgendes:

1. `deck-ctl` startet, `make_link()` liest `counter_file` (z. B. Stand `1000`) in die
   eigene, frische `self._counter`.
2. Bevor dieser eine `deck-ctl`-Prozess tatsächlich sendet (Argparse/Config-Laden
   braucht etwas Zeit), bedient `deck_daemon` in der Zwischenzeit mehrere eingehende
   HTTP-Requests über die Ökosystem-UI (`/sat/ping`, `/sat/trigger`, …) — sein eigener,
   seit dem eigenen Start unverändert im RAM gehaltener Zähler zählt dabei unabhängig
   weiter hoch (z. B. auf `1005`), inklusive korrekter Persistenz auf die Platte nach
   jedem einzelnen Send (das vorige, bereits vorgeschlagene additive Fix dafür
   vorausgesetzt).
3. Der `deck-ctl`-Prozess sendet jetzt seinen einen Befehl — mit `self._counter + 1 =
   1001`, weil er den Daemon-Burst nicht kennt. Der Satellit hat Counter `1005` aber
   längst akzeptiert → `1001 <= 1005` → der Befehl wird vom Satelliten **kommentarlos
   verworfen**, exakt das stille Verlust-Symptom aus dem vorigen Fund, hier aber durch
   eine andere Ursache.
4. **Schwerer als Schritt 3**: `deck-ctl`s eigener `_save_counter()`-Aufruf schreibt
   anschließend `1001` auf die Platte — und überschreibt damit den vom Daemon korrekt
   auf `1005` gebrachten Stand mit einem *niedrigeren* Wert. Jeder weitere,
   eigenständig gestartete `deck-ctl`-Aufruf liest ab jetzt wieder von diesem zu
   niedrigen Stand ab und wird seinerseits verworfen — nicht nur der eine ursprüngliche
   Befehl, sondern eine **ganze Serie** nachfolgender `deck-ctl`-Aufrufe, bis der
   wiederholt um 1 erhöhte, aber jedes Mal erneut verworfene Zähler den wahren,
   dem Satelliten bereits bekannten Stand wieder eingeholt hat.

`deck_daemon.py`, `deck_ctl.py` und `tools/hid_stream_send.py` nutzen alle drei
dieselbe `SatelliteLink.send()`-Methode und sind damit gleich betroffen, sobald zwei
ihrer Prozesse (typischerweise Daemon + CLI) auf dieselbe `counter_file` zeigen.
`scenario_runner.py` ist hiervon nicht betroffen (eigener, unpersistenter In-Memory-
Zähler für isolierte Labor-Szenarien, wie schon im vorigen Fund vermerkt).

## Nutzen
- Schließt die Lücke dauerhaft: `send()` liest die `counter_file` jetzt bei **jedem**
  Aufruf unter einem exklusiven, prozessübergreifenden Datei-Lock neu ein
  (`max(self._counter, self._load_counter()) + 1`) und persistiert unter demselben
  Lock, bevor es sendet. Ein langlaufender `deck_daemon` sieht dadurch jeden
  zwischenzeitlichen `deck-ctl`-Aufruf (und umgekehrt) garantiert, bevor er seinerseits
  den nächsten Counter vergibt — die Platte bleibt die alleinige Quelle der Wahrheit
  über Prozessgrenzen hinweg, kein Prozess kann sie mehr unter den wahren Stand
  zurückfallen lassen.
- Der Lock (`fcntl.flock` unter POSIX, `msvcrt.locking` unter Windows — beides
  Standardbibliothek, kein neuer Fremd-Dependency) läuft auf einer eigenen
  `counter_file + ".lock"`-Datei statt direkt auf der Counter-Datei selbst, damit das
  bestehende, bereits additiv gefixte atomare `os.replace()`-Schreibmuster in
  `_save_counter()` unverändert bleibt.
- `self._lock` (innerhalb eines Prozesses) und der neue Datei-Lock (über
  Prozessgrenzen) ergänzen sich, statt sich zu ersetzen — Mehrfach-Threads im selben
  `deck_daemon`-Prozess bleiben weiterhin zusätzlich durch `self._lock` serialisiert,
  bevor sie überhaupt um den Datei-Lock konkurrieren.
- Wirkt für alle drei Aufrufer (`deck_ctl.py`, `deck_daemon.py`,
  `tools/hid_stream_send.py`) gleichzeitig, weil der Fix zentral in
  `SatelliteLink.send()` sitzt — keine Änderung an einer der drei Aufruferstellen
  nötig.

## Betroffene Dateien
- `deck/satellite_link.py` — neue Klasse `_CounterFileLock` (POSIX/Windows-Datei-Lock)
  sowie `send()` (Counter-Kritikalabschnitt jetzt unter diesem Lock, mit Re-Read von
  der Platte vor dem Erhöhen).

## Patch
Unified-Diff **gegen den bereits vorgeschlagenen Zwischenstand** aus
`satellite-link-counter-saved-after-send-fix/satellite_link_py.patch` (nicht gegen die
unveränderte `G4MEOVER18/g4meover-deck`-Basis) in
[`satellite-link-cross-process-counter-race-fix/satellite_link_py.patch`](satellite-link-cross-process-counter-race-fix/satellite_link_py.patch).
**Anwendungsreihenfolge:** zuerst `satellite-link-counter-saved-after-send-fix/
satellite_link_py.patch`, danach diesen Patch — beide zusammen ändern dieselbe
Methode (`send()`), ein alleinstehendes Anwenden dieses Patches auf die unveränderte
Datei schlägt daher fehl (keine passende Kontextzeile).

Gegen zwei frische, unabhängige Zweitkopien verifiziert: Kopie 1 (nur der vorige Patch
angewendet) exakt byte-für-byte gegen den dort referenzierten Zwischenstand
abgeglichen; Kopie 2 (vorigen Patch angewendet, dann diesen Patch mit `git apply
--check` geprüft und real angewendet) ergibt nach beiden Schritten exakt denselben
Endstand wie Kopie 1 plus dieser Patch — die Verzahnung ist widerspruchsfrei.

## Testhinweis (ohne Hardware)
[`repro-satellite-link-cross-process-counter-race/repro.py`](repro-satellite-link-cross-process-counter-race/repro.py)
nutzt die echte, unveränderte `ukfe_rf.py` sowie `satellite_link_before.py` (1:1 Stand
nach dem vorigen Patch, also bereits mit Save-vor-Send/`fsync()`, aber ohne
Cross-Prozess-Lock) und `satellite_link_after.py` (1:1 Stand nach diesem Patch) —
`serial.py` bildet wie beim vorigen Repro nur die von `satellite_link.py` benutzte
pyserial-Oberfläche nach:

```
$ python3 repro.py
--- satellite_link_before.py (Save-vor-Send, aber KEIN Cross-Prozess-Lock) ---
Daemon-Burst: Satellit zuletzt akzeptiert Counter 1005 (5 angenommen, 0 verworfen)
deck-ctl-Aufruf: VERWORFEN (Satellit zuletzt akzeptiert Counter 1005, counter_file jetzt=1001)
10 Nachlauf-deck-ctl-Aufrufe (je frisch gestartet): 4/10 verworfen

--- satellite_link_after.py (mit Cross-Prozess-Lock, dieser Patch) ---
Daemon-Burst: Satellit zuletzt akzeptiert Counter 1005 (5 angenommen, 0 verworfen)
deck-ctl-Aufruf: angenommen (Satellit zuletzt akzeptiert Counter 1006, counter_file jetzt=1006)
10 Nachlauf-deck-ctl-Aufrufe (je frisch gestartet): 0/10 verworfen

BUG bestaetigt: ...
FIX bestaetigt: ...
Alle Szenarien bestaetigt
```

Das Repro modelliert exakt die oben beschriebene Abfolge (ein langlaufendes
`SatelliteLink` als `deck_daemon`-Stellvertreter, ein zweites, zu Beginn parallel
konstruiertes `SatelliteLink` als `deck-ctl`-Stellvertreter, zehn weitere, je frisch
konstruierte Instanzen als Nachlauf-`deck-ctl`-Aufrufe) gegen denselben
Satelliten-Mock mit der realen `counter <= last_counter`-Regel. Deterministisch bis auf
den einmalig gesetzten Startwert (hier fest `1000` statt `_load_counter()`s
zeit-basiertem Fallback) — in mehreren Läufen identisch reproduziert.

Zusätzlich [`repro-satellite-link-cross-process-counter-race/repro_threads.py`](repro-satellite-link-cross-process-counter-race/repro_threads.py):
vier echte, gleichzeitig laufende Threads (je ein eigenes `SatelliteLink`, kein
gemeinsames Lock-Objekt — bildet vier echte Prozesse nach) senden parallel je 50
Befehle auf dieselbe `counter_file`. Vor dem Fix werden von 200 gesendeten Befehlen nur
50 (die jeweils zuerst einen bestimmten Counter-Wert erreichenden) tatsächlich vom
Satelliten angenommen, 150 verworfen; nach dem Fix alle 200 — in drei
aufeinanderfolgenden Läufen identisch reproduziert. Das bestätigt, dass der Fix auch
unter echter, nicht nur sequentiell nachgebildeter Nebenläufigkeit greift.

Lokaler Funktionstest (mit Hardware): `deck_daemon`-Dienst starten (oder
`python3 deck_daemon.py` im Vordergrund), parallel mehrfach `deck-ctl sat ping`
ausführen, während über die Ökosystem-UI oder `curl -X POST .../sat/ping` ebenfalls
Befehle an denselben Satelliten gehen — vor dem Fix bleibt die Reaktion am Satelliten
bei einem Teil der `deck-ctl`-Aufrufe aus (kein ACK/Log-Eintrag trotz "gesendet" auf
der Konsole), nach dem Fix reagiert der Satellit auf jeden einzelnen Aufruf.

## Risiko
- Niedrig: additive Erweiterung von `send()` um einen zusätzlichen, eng gefassten
  Lock-Block; Wire-Format, MAC/CRC-Berechnung (`ukfe_rf.py`) und die Satelliten-seitige
  Anti-Replay-Regel selbst sind unverändert.
- Der neue Lock liegt auf einer separaten `counter_file + ".lock"`-Datei, nicht auf der
  Counter-Datei selbst — das bereits bestehende atomare `.tmp`+`os.replace()`-Schreiben
  in `_save_counter()` bleibt unverändert und unabhängig vom Lock-Mechanismus
  funktionsfähig, falls der Lock aus irgendeinem Grund (z. B. Dateisystem ohne
  `flock()`-Unterstützung) nicht greifen sollte.
- Jeder `send()`-Aufruf kostet jetzt zusätzlich einen `open()`/`flock()`/`close()` auf
  der Lock-Datei und ein weiteres `_load_counter()` (ein kleiner Datei-Read) — bei der
  in diesem Deck üblichen Aufruffrequenz (manuelle Befehle bzw. Szenario-Schritte im
  Sekundenbereich, keine Hochfrequenz-Schleife) ohne spürbare Auswirkung.
- Cross-Plattform: `fcntl` (POSIX, Ziel-Plattform Raspberry Pi) und `msvcrt` (Windows,
  für `tools/hid_stream_send.py`s dokumentierten Windows-Einsatz direkt am Hub-USB)
  sind beide Teil der Standardbibliothek — keine neue Abhängigkeit.
- Löst nicht das separat davon bestehende, bereits im vorigen Fund benannte
  Power-Loss-Fenster von `os.fsync()` selbst (Schreib-Caching der zugrunde liegenden
  SD-Karten-Hardware) — das ist eine Hardware-Eigenschaft außerhalb der Kontrolle dieser
  Python-Schicht und nicht Gegenstand dieses Patches.

## Quelle
- `G4MEOVER18/g4meover-deck` (main, Commit `b030e45`): `deck/satellite_link.py`
  (`send()`, `_load_counter()`/`_save_counter()`, unverändert von diesem Patch
  übernommen), `deck/deck_daemon.py` (ein `_link` pro Prozesslauf, `ThreadingHTTPServer`
  mit einem Thread je HTTP-Request), `deck/deck_ctl.py` (`make_link()` instanziiert pro
  CLI-Aufruf frisch), `deck/tools/hid_stream_send.py` (dritter Aufrufer, gleiche
  `SatelliteLink.send()`-Methode), `deck/scenario_runner.py` (separater, unbetroffener
  In-Memory-Zähler).
- `G4MEOVER18/g4meover-ecosystem`:
  `app-improvements/g4meover-deck/satellite-link-counter-saved-after-send-fix.md` (der
  vorige Fund an derselben Methode, dort als "eigenständiger, größerer Fund" auf diese
  Sitzung zurückgestellt: "`deck_daemon` ... und einzelne `deck-ctl sat ...`-Aufrufe
  können dieselbe `counter_file` ohne Datei-Locking über Prozessgrenzen hinweg
  gleichzeitig beschreiben").
