# g4meover-deck — flipper_link.py::find_flipper() akzeptiert im Fallback jedes ttyACM*-Gerät ungeprüft

## Was
Ich habe `deck/flipper_link.py` jetzt Zeile für Zeile als eigenständigen Fund-Kandidaten
durchgesehen — bislang nur als "dünner Wrapper um flipperbridge" überflogen, aber nicht
im Detail geprüft (Rückstellpunkt aus der `zigbee_link.py`-Sitzung).

`find_flipper()` sucht den Port in zwei Stufen:

```python
def find_flipper() -> str | None:
    try:
        import device_discovery
        port = device_discovery.find_port("Flipper Zero")
        if port:
            return port
    except Exception:  # noqa: BLE001
        pass
    ports = sorted(glob.glob("/dev/ttyACM*"))
    return ports[0] if ports else None
```

Die erste Stufe (`device_discovery.find_port`) prüft korrekt per USB-VID:PID
(`0x0483:0x5740` für den Flipper, siehe `device_discovery.KNOWN`). Schlägt sie fehl —
`device_discovery` ist nicht im `sys.path` (z. B. weil `install.sh` es nicht neben
`flipper_link.py` ablegt, oder ein älterer Deck-Stand ohne dieses Modul), `pyserial`
fehlt, oder eine andere Ausnahme tritt auf — fängt der breite `except Exception` das
ab und die zweite Stufe greift: der erste `/dev/ttyACM*`-Pfad in alphabetischer
Sortierung wird **ohne jede Identitätsprüfung** als Flipper-Port zurückgegeben.

Das Ökosystem selbst dokumentiert in `device_discovery.KNOWN`, dass der Flipper nicht
das einzige Gerät ist, das unter Linux als `/dev/ttyACM*` erscheinen kann:
- CC2531-Zigbee-Koordinator (VID `0x0451`, eigener Eintrag `"CC2531 Zigbee"`,
  `access=["USB-ZNP"]`).
- LilyGo T-Dongle S3 (VID `0x303A`, eigener Hinweis im Code:
  `note="native-USB-CDC remote still (S3-Quirk)"` — natives USB-CDC bindet unter Linux
  genau wie beim Flipper über den generischen `cdc_acm`-Treiber, nicht über eine
  FTDI/CP21xx-Bridge).

Sind Flipper **und** eines dieser Geräte gleichzeitig angeschlossen (im Vollbetrieb des
Ökosystems der Normalfall, nicht die Ausnahme) und schlägt `device_discovery` aus
irgendeinem Grund fehl, kann `find_flipper()` den Pfad des *anderen* Geräts
zurückliefern, sobald dessen `/dev/ttyACM*`-Nummer alphabetisch vor der des Flippers
liegt. `FlipperLink` öffnet diesen Port dann mit `FlipperBridge(self.port)` und sendet
flipperbridge-RPC-Bytes an ein Gerät, das sie nicht versteht — ohne jede Fehlermeldung
an dieser Stelle, nur ein für den Nutzer verwirrender Fehlschlag irgendwo tiefer im
RPC-Handshake, der aussieht wie ein kaputter Flipper statt wie das eigentliche Problem
(falscher Port gewählt).

Kleinerer Nebenfund in derselben Zeile: `sorted(glob.glob(...))` sortiert rein
lexikografisch. Ab zweistelligen Portnummern kehrt das die naheliegende Reihenfolge um
(`"/dev/ttyACM10"` < `"/dev/ttyACM2"` als String).

## Nutzen
- Schließt die stille Fehlerquelle in der zweiten Fallback-Stufe: bevor überhaupt
  ungeprüft der erste `/dev/ttyACM*`-Pfad akzeptiert wird, prüft `find_flipper()` jetzt
  per `pyserial` direkt die VID:PID aller erkannten seriellen Geräte gegen die bereits
  in `device_discovery.KNOWN` hinterlegte Flipper-Kennung — unabhängig davon, ob das
  `device_discovery`-Modul selbst verfügbar ist, solange nur `pyserial` installiert ist
  (das Deck hängt an mehreren Stellen bereits davon ab).
- Nur wenn auch diese zweite Stufe fehlschlägt (kein `pyserial` verfügbar), greift der
  bisherige blinde Fallback — jetzt zusätzlich mit einer längenbewussten Sortierung
  statt rein lexikografisch.
- Reine Zusatzabsicherung: der bisherige Haupt-Erfolgspfad (`device_discovery` findet
  den Flipper direkt) bleibt unverändert und wird durch den Patch nie berührt.

## Betroffene Dateien
- `deck/flipper_link.py` — `find_flipper()`: neue Zwischenstufe mit direktem
  `serial.tools.list_ports`-VID:PID-Abgleich vor dem blinden Glob-Fallback; der
  verbleibende Glob-Fallback sortiert jetzt längenbewusst statt rein lexikografisch.

## Patch
Unified-Diff gegen die unveränderte `G4MEOVER18/g4meover-deck`-Basis (main, Commit
`b030e45`) in
[`flipper-link-unverified-fallback-fix/flipper_link_py.patch`](flipper-link-unverified-fallback-fix/flipper_link_py.patch).
Gegen zwei frische, unabhängige Zweitkopien mit `git apply --check` verifiziert, real
angewendet, byte-für-byte abgeglichen — identisch. Mit `python3 -m py_compile` geprüft.

## Testhinweis (ohne Hardware)
[`repro-flipper-link-unverified-fallback/repro.py`](repro-flipper-link-unverified-fallback/repro.py)
transkribiert `find_flipper()` ALT (vor dem Fix) und NEU (nach dem Fix) 1:1 als
eigenständige Funktionen. `device_discovery` und `serial.tools.list_ports` werden pro
Szenario als Fake-Module in `sys.modules` eingehängt (bzw. bewusst weggelassen, um ein
echtes `ModuleNotFoundError` nachzubilden) — kein echter Flipper, kein echtes
`pyserial` nötig:

```
$ python3 -I repro.py
[OK] 1) Regression: device_discovery findet Flipper direkt
[OK] 2) Fund: device_discovery fehlt, CC2531 vor Flipper auf ttyACM*
       ALT='/dev/ttyACM0' (erwartet '/dev/ttyACM0')  NEU='/dev/ttyACM1' (erwartet '/dev/ttyACM1')
[OK] 3) Fund-Variante: device_discovery fehlt, LilyGo T-Dongle S3 vor Flipper
[OK] 4) Grenzfall: weder device_discovery noch pyserial verfuegbar
[OK] 5) Nebenfund: Glob-Fallback bei zweistelligen ttyACM-Nummern
       ALT='/dev/ttyACM10' (erwartet '/dev/ttyACM10')  NEU='/dev/ttyACM2' (erwartet '/dev/ttyACM2')
[OK] 6) Kein Geraet angeschlossen
[OK] 7.0..7.19) Aequivalenz: device_discovery liefert direkt den jeweiligen Port
ALLE CHECKS BESTANDEN (26/26)
```

Deckt ab: (1) Regression ohne jede Störung — `device_discovery` findet den Flipper
direkt, ALT=NEU, die neue Stufe wird nie erreicht; (2) den eigentlichen Fund —
`device_discovery` fällt aus, der CC2531-Zigbee-Koordinator liegt alphabetisch vor dem
Flipper: ALT liefert fälschlich den Zigbee-Port, NEU findet den Flipper trotzdem per
direktem VID:PID-Abgleich; (3) dieselbe Situation mit dem LilyGo T-Dongle S3 als
Störer; (4) den dokumentierten Grenzfall, dass weder `device_discovery` noch `pyserial`
verfügbar sind — beide Stufen fallen gleich auf den blinden Glob-Fallback zurück
(ALT=NEU, keine Verschlechterung); (5) den Nebenfund bei zweistelligen Portnummern im
blinden Fallback; (6) kein Gerät angeschlossen; (7) einen Äquivalenztest über 20 von
`device_discovery` direkt gelieferte Ports — NEU darf das nie verändern.

Lokaler Funktionstest (mit Hardware): Flipper **und** CC2531 (oder LilyGo T-Dongle S3)
gleichzeitig am Pi anschließen, das vendorte `device_discovery.py` kurzzeitig
umbenennen oder aus dem `sys.path` entfernen (simuliert eine unvollständige
`install.sh`-Installation), dann `FlipperLink().info()` aufrufen — vor dem Fix kann der
gewählte Port je nach alphabetischer Reihenfolge der `/dev/ttyACM*`-Nummern das falsche
Gerät sein (RPC-Fehlschlag ohne erkennbaren Grund); nach dem Fix wird unabhängig von der
Portnummer immer der per VID:PID als Flipper identifizierte Port gewählt. Mit
vorhandenem `device_discovery.py` (Normalfall) unverändertes Verhalten vor/nach dem
Fix.

## Risiko
- Niedrig: additive Zwischenstufe zwischen den beiden bestehenden Stufen, kein
  bestehender Erfolgspfad wird verändert. Greift nur, wenn die erste Stufe
  (`device_discovery`) bereits fehlgeschlagen ist.
- Die neue Stufe nutzt ausschließlich `serial.tools.list_ports.comports()` — rein
  lesend, keine Portöffnung, kein Seiteneffekt auf angeschlossene Geräte.
- Schließt den blinden Fallback (dritte Stufe) nicht komplett — nur wenn auch
  `pyserial` komplett fehlt, bleibt das bisherige, ungeprüfte Verhalten (dokumentierte,
  bewusst in Kauf genommene Grenze für diesen seltenen Fall).

## Quelle
- `G4MEOVER18/g4meover-deck` (main, Commit `b030e45`): `deck/flipper_link.py`
  (`find_flipper()`), `deck/device_discovery.py` (`KNOWN`-Tabelle mit den
  VID:PID-Einträgen für Flipper Zero, CC2531 Zigbee, LilyGo T-Dongle S3 inklusive des
  eigenen "S3-Quirk"-Hinweises zum nativen USB-CDC).
- Linux `cdc_acm`-Treiberverhalten: bindet nach USB-Geräteklasse (CDC-ACM), nicht nach
  Hersteller-VID — jedes Gerät mit nativer USB-CDC-Implementierung (STM32-Flipper,
  Espressif-S3-natives-USB) kann dieselbe `/dev/ttyACM*`-Namensreihe belegen.
