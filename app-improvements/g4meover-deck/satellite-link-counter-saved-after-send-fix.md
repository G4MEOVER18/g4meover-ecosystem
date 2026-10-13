# g4meover-deck — SatelliteLink persistiert den Rolling-Counter erst nach dem Senden

## Was
Ich habe `deck/satellite_link.py` geprüft (bisher kein eigener `app-improvements/
g4meover-deck/`-Eintrag vorhanden) — den Python-Gegenpart zum bereits mehrfach in
Firmware (`heltec-ukfe-rx`/`lilygo-ukfe-rx`/`heltec-v4-hub`/`usb-army-penetrator`)
geprüften `ukfe_rf`-Anti-Replay-Schema, hier auf der Sendeseite des Raspberry-Pi-Decks:

```python
# ---- Counter-Persistenz (Anti-Replay ueber Reboots) ----
def _load_counter(self) -> int:
    try:
        with open(self.counter_file) as f:
            return int(f.read().strip() or "0")
    except (OSError, ValueError):
        return int(time.time())

def _save_counter(self) -> None:
    try:
        os.makedirs(os.path.dirname(self.counter_file), exist_ok=True)
        tmp = self.counter_file + ".tmp"
        with open(tmp, "w") as f:
            f.write(str(self._counter))
        os.replace(tmp, self.counter_file)  # atomar
    except OSError:
        pass

# ---- Senden ----
def send(self, cmd: int, args: bytes = b"") -> int:
    with self._lock:
        self.open()
        self._counter += 1
        frame = ukfe_rf.build_frame(self.secret, self._counter, cmd, args)
        assert self._serial is not None
        self._serial.write(frame)
        self._serial.flush()
        self._save_counter()          # <-- erst HIER persistiert
        return self._counter
```

Anders als die bisher gefundenen Fälle (Empfänger hält `last_counter` nur im RAM,
verliert ihn beim eigenen Neustart, öffnet dadurch ein Replay-Fenster) ist
`SatelliteLink` als *Sender* bereits bewusst auf Persistenz ausgelegt — der
Datei-Kommentar sagt das explizit ("überlebt Reboots"). Der Fund liegt in der
Reihenfolge: `send()` schreibt den Frame erst per `self._serial.write()`/`flush()` auf
die UART-Leitung (der Satellit empfängt und akzeptiert ihn an dieser Stelle bereits —
sein `last_counter` steht danach auf dem neuen Wert) und persistiert den dafür
verwendeten Zähler erst **danach** mit `_save_counter()`.

Stirbt der Pi-seitige Prozess genau in diesem Fenster — `kill -9`, SSH-Verbindungs-
abbruch, vor allem aber ein Stromausfall auf einer SD-Karten-basierten
Raspberry-Pi-Installation ohne USV, exakt das in `g4meover-deck/README.md` beschriebene
Zielsystem — wurde der Frame bereits zugestellt und vom Satelliten akzeptiert, aber der
dafür verwendete Counter-Wert steht nicht (mehr) auf der Platte. Der nächste
Prozessstart (nächster `deck-ctl sat ...`-Aufruf oder Neustart des `deck_daemon`-
Systemd-Dienstes) lädt den alten, niedrigeren Stand, erhöht ihn um eins und sendet
einen Frame mit einem Counter, den der Satellit schon kennt — `counter <= last_counter`
greift, der Satellit verwirft den Befehl kommentarlos. Jeder weitere Aufruf zählt
normal weiter hoch und holt irgendwann auf; der praktische Effekt ist also kein
dauerhafter, sondern ein **stiller Verlust des jeweils ersten Befehls nach einem
unsauberen Absturz** — am Pi sichtbar nur als "`deck-ctl sat ping` lief durch, der
Satellit hat aber nicht reagiert", ohne jede Fehlermeldung.

**Zusätzlich festgestellt, hier mitbehoben statt nur dokumentiert:** `_save_counter()`
schreibt in die `.tmp`-Datei und ruft danach `os.replace()` — atomar bezüglich eines
Prozessabsturzes, aber ohne `fsync()` nicht zwingend haltbar gegenüber einem echten
Stromausfall auf dem Zielsystem, da die Schreibdaten bis dahin im Seiten-Cache des
Kernels liegen können. Gerade auf einer SD-Karte ohne USV (das in `README.md`
beschriebene Zielsystem) ist das der realistischere Ausfall als ein reiner
Prozess-Kill. Ohne `fsync()` kann ein Stromausfall sogar einen bereits erfolgreich per
`os.replace()` umbenannten Stand wieder auf eine ältere Version zurückfallen lassen und
damit das oben beschriebene Fenster vergrößern.

Jede der drei Aufruferstellen (`deck_ctl.py` — jeder `deck-ctl sat ...`-Befehl ist ein
eigener, kurzlebiger Prozess, der `SatelliteLink` frisch instanziiert; `deck_daemon.py`
— ein langlaufender Dienst mit eigenem `SatelliteLink` pro Prozesslauf;
`tools/hid_stream_send.py`) nutzt dieselbe `send()`-Methode und ist damit gleich
betroffen. `scenario_runner.py` baut Frames direkt über `ukfe_rf.make_trigger()` mit
einem reinen In-Memory-Zähler (`counter = 5000`, "monoton im Lauf") ohne jede
Persistenz — das ist für Labor-Szenarien (bewusst isolierter Testlauf, kein Dauerbetrieb
gegen reale Satelliten-Zustände) kein eigenständiger Fund und nicht Gegenstand dieses
Patches.

## Nutzen
- Schließt das oben beschriebene Verlustfenster vollständig: Persistieren geschieht ab
  jetzt **vor** dem UART-Write. Stirbt der Prozess dazwischen, bleibt höchstens ein
  Counter-Wert ungenutzt liegen — das ist bereits in `_load_counter()`s eigenem
  Kommentar als unkritischer Fall vorgesehen (der nächste Start zählt einfach ab einem
  höheren Wert weiter, der Satellit sieht nie einen falschen/doppelten Counter).
- `fsync()` vor `os.replace()` macht die Persistenz auch gegenüber einem echten
  Stromausfall (nicht nur einem Prozess-Kill) haltbar — genau das Szenario, für das ein
  Raspberry-Pi-Dauerbetrieb ohne USV anfällig ist.
- Reine Reihenfolgeänderung plus zwei zusätzliche Syscalls (`flush()`/`fsync()`) — keine
  neue Abhängigkeit, kein verändertes Dateiformat, kein verändertes Wire-Protokoll.

## Betroffene Dateien
- `deck/satellite_link.py` — `_save_counter()` (zwei neue Zeilen: `f.flush()`/
  `os.fsync(f.fileno())`) und `send()` (ein Aufruf von `self._save_counter()` von nach
  dem UART-Write nach davor verschoben, mit Begründungskommentar).

## Patch
Unified-Diff gegen die unveränderte `G4MEOVER18/g4meover-deck`-Basis (`main`, Commit
`b030e45`) in
[`satellite-link-counter-saved-after-send-fix/satellite_link_py.patch`](satellite-link-counter-saved-after-send-fix/satellite_link_py.patch).
Gegen eine frische, unabhängige Zweitkopie mit `git apply --check` verifiziert, real
angewendet, byte-für-byte mit der Arbeitsfassung abgeglichen — identisch.

## Testhinweis (ohne Hardware)
[`repro-satellite-link-counter-saved-after-send/repro.py`](repro-satellite-link-counter-saved-after-send/repro.py)
nutzt die echten, unveränderten Dateien `ukfe_rf.py` (1:1 aus `deck/ukfe_rf.py`) sowie
`satellite_link_orig.py`/`satellite_link_fixed.py` (1:1 Stand vor/nach dem Patch) —
`serial.py` in demselben Verzeichnis bildet nur die von `satellite_link.py` benutzte
pyserial-Oberfläche (`write()`/`flush()`/`close()`) nach, keine echte UART-Abhängigkeit
nötig:

```
$ python3 repro.py
--- satellite_link_orig.py (Counter-Save NACH dem Write) ---
Sitzung 1: 0 verworfen (erwartet), Satellit zuletzt akzeptiert: Counter 1791851164
Crash bei Counter 1791851165 (vom Satelliten bereits akzeptiert)
Nach Neustart: 1/10 Status-Befehle vom Satelliten verworfen

--- satellite_link_fixed.py (Counter-Save VOR dem Write) ---
Sitzung 1: 0 verworfen (erwartet), Satellit zuletzt akzeptiert: Counter 1791851164
Crash bei Counter 1791851165 (vom Satelliten bereits akzeptiert)
Nach Neustart: 0/10 Status-Befehle vom Satelliten verworfen

BUG bestaetigt: satellite_link_orig.py verwirft nach dem simulierten Absturz den naechsten Befehl als bereits verbraucht (Selbstaussperrung fuer genau den einen verlorenen Counter-Schritt)
FIX bestaetigt: satellite_link_fixed.py verwirft nach demselben simulierten Absturz keinen einzigen Befehl

Alle Szenarien bestaetigt
```

Das Repro baut mit `satellite_link_orig.py`/`_fixed.py` je einen `SatelliteLink` mit
einer injizierten `Crash`-Mock-Serial (wirft im `write()`-Aufruf eine Ausnahme, aber
erst *nachdem* der Frame dem Satelliten-Modell zugestellt wurde — bildet exakt nach,
dass der Byte-Strom den Satelliten unabhängig vom weiteren Schicksal des Host-Prozesses
erreicht), simuliert einen Satelliten mit der realen `counter <= last_counter`-Regel,
löst einen Absturz direkt nach einem erfolgreichen Write aus und lässt dann einen
frischen `SatelliteLink` (neuer Prozess, lädt den Counter nur von der Platte) zehn
weitere Befehle senden. Deterministisch bis auf den Zeit-basierten Startwert beim allerersten
Lauf (`_load_counter()`s `int(time.time())`-Fallback) — in zwei aufeinanderfolgenden
Läufen verifiziert, Ergebnis (1/10 vs. 0/10 verworfen) identisch reproduziert.

Lokaler Funktionstest (mit Hardware): zwei Satelliten-Status-Abfragen über `deck-ctl sat
ping` ausführen, danach den Deck-Prozess (bzw. den `systemd`-Dienst `g4meover-deck`)
mitten in einem dritten `deck-ctl sat ping`-Aufruf hart abbrechen (`kill -9` auf die
PID, oder den Pi stromlos schalten unmittelbar nach "gesendet: ping" auf der Konsole),
danach erneut `deck-ctl sat ping` ausführen — vor dem Fix bleibt die Reaktion am
Satelliten beim ersten Aufruf nach dem Neustart aus (kein ACK/Log-Eintrag trotz "gesendet"
auf der Pi-Konsole), nach dem Fix reagiert der Satellit sofort.

## Risiko
- Niedrig: reine Reihenfolgeänderung innerhalb einer bereits bestehenden Methode plus
  zwei zusätzliche, rein lesende/synchronisierende Syscalls (`flush()`, `fsync()`).
  Wire-Format, MAC/CRC-Berechnung (`ukfe_rf.py`) und die Satelliten-seitige
  Anti-Replay-Regel selbst sind unverändert.
- `fsync()` kostet pro `send()`-Aufruf einen zusätzlichen Platten-Sync — bei der in
  diesem Deck üblichen Aufruffrequenz (manuelle `deck-ctl`-Befehle bzw. Szenario-Schritte
  im Sekundenbereich, keine Hochfrequenz-Schleife) ohne spürbare Auswirkung.
- Verschiebt nur den Zeitpunkt der Persistierung innerhalb von `send()` — ändert nichts
  an `_load_counter()`s Fallback-Verhalten bei fehlender/kaputter Datei (bleibt
  zeit-basiert) und nichts an der pro-Prozess-`threading.Lock`-Serialisierung
  gleichzeitiger `send()`-Aufrufe innerhalb eines einzelnen `SatelliteLink`.
- Löst nicht den separat davon bestehenden Mehrprozess-Fall (z. B. `deck_daemon` läuft
  dauerhaft UND parallel ein `deck-ctl sat ...`-Aufruf über dieselbe `counter_file`,
  beide ohne Datei-Locking über Prozessgrenzen) — als eigenständiger, größerer Fund für
  eine künftige Sitzung vorgemerkt, hier bewusst nicht mitgelöst, um den Patch additiv
  und klein zu halten.

## Quelle
- `G4MEOVER18/g4meover-deck` (main, Commit `b030e45`): `deck/satellite_link.py`
  (`_load_counter`/`_save_counter`/`send`), `deck/ukfe_rf.py` (Frame-Bau, unverändert),
  `deck/deck_ctl.py`/`deck/deck_daemon.py`/`deck/tools/hid_stream_send.py` (alle drei
  Aufrufer von `SatelliteLink.send()`), `deck/scenario_runner.py` (separater,
  unbetroffener In-Memory-Zähler für Labor-Szenarien), `README.md` (Zielsystem
  Raspberry Pi 4, SD-Karten-Betrieb).
- `G4MEOVER18/g4meover-ecosystem`:
  `app-improvements/heltec-v4-hub/tx-broadcast-counter-restart-lockout-fix.md` und
  `app-improvements/lora-ukfe/rf-counter-restart-lockout-fix.md` (dasselbe
  Selbstaussperrungs-Muster auf der Sendeseite, dort durch einen reinen RAM-Zähler ohne
  jede Persistenz verursacht statt — wie hier — durch eine ungünstige Reihenfolge
  zwischen Senden und einer bereits vorhandenen Persistenz).
