# g4meover-companion / flipperbridge — `storage_send()` meldet jede leere Datei fälschlich als Fehler

## Was

`flipperbridge/flipperbridge.py` ist die serielle USB-CDC-Steuerung des
Flipper Zero (Fundament des Pi-Decks; `cockpit.py`/`webdeck.py` nutzen sie für
Datei-Uploads). `FlipperBridge.storage_send()` lädt eine lokale Datei per
`storage write_chunk` hoch und verifiziert den Transfer anschließend über
einen MD5-Soll/Ist-Vergleich:

```python
self.s.write(data)
self.s.flush()
self._read_until(PROMPT, 15.0)
# Verifizieren via md5
want = _md5(data)
got = self.cmd(f'storage md5 "{remote_path}"').strip().split()[-1] if data else ""
return want == got
```

`want` wird **unbedingt** aus den tatsächlichen Daten berechnet — auch wenn
`data` leer ist (`hashlib.md5(b"").hexdigest()` ist `d41d8cd98f00b204e9800998ecf8427e`,
niemals ein leerer String). `got` wird dagegen für `data == b""` auf den
leeren String `""` gesetzt, offensichtlich um das `storage md5`-Kommando für
eine leere Datei gar nicht erst abzusetzen. Der abschließende Vergleich
`want == got` steht damit für jede leere Datei immer einem nicht-leeren
32-Zeichen-Hex-String gegen `""` gegenüber — **das kann nie gleich sein**,
unabhängig davon, ob der Upload auf dem Gerät tatsächlich korrekt war (der
vorherige `Ready`-Handshake und das Schreiben selbst laufen unverändert
korrekt durch).

`storage_send("", ziel)` bzw. jeder Upload einer echten 0-Byte-Datei liefert
also garantiert `False`, obwohl auf dem Gerät eine leere Zieldatei korrekt
angelegt wurde. Betroffen ist jeder Aufrufer von `storage_send()` — aktuell
die CLI selbst (`flipperbridge.py ... send`, druckt "FEHLER" und beendet sich
mit Exit-Code 1) sowie `cockpit.py`, soweit es `storage_send()` für eigene
Uploads nutzt.

## Nutzen des Fixes

Eine leere Datei ist kein Sonderfall, der nie vorkommt: ein leeres `.sub`/
`.nfc`-Platzhalter-Capture, eine absichtlich leere Log-/Marker-Datei oder ein
versehentlich 0 Byte großes lokales File (z. B. ein fehlgeschlagener
vorheriger Download) lösen alle denselben, irreführenden "FEHLER:
md5-Verifikation"-Pfad aus — obwohl der eigentliche Transfer auf dem Gerät
funktioniert hat. Der Fix bringt `storage_send()` in Übereinstimmung mit der
Absicht, die der `if data else ""`-Sonderfall im Status quo schon erkennen
lässt (leere Datei = nichts zu hashen = kein Verifikationskommando nötig),
indem er für diesen Fall direkt `True` statt eines unvergleichbaren Hash-vs-
Leerstring-Ergebnisses liefert. Für jede nicht-leere Datei ändert sich am
Verifikationsverhalten nichts — ein echter MD5-Mismatch wird weiterhin exakt
genauso als `False` erkannt wie vorher.

## Betroffene Dateien

- `flipperbridge/flipperbridge.py` — nur `FlipperBridge.storage_send()`,
  letzte drei Zeilen der Methode.

## Patch

`connective-patches/flipperbridge_py.patch` (unified diff gegen
`flipperbridge/flipperbridge.py`, Commit `ceb068bf278feae4a8180d24330111e0b911301d`
von `G4MEOVER18/g4meover-companion`, `main`). Per `git apply --check` sowie
`patch -p1 --dry-run` gegen zwei frische, unabhängige Zweitkopien angewendet,
Ergebnis in beiden Fällen per `md5sum` byte-identisch zur vollständigen
Zieldatei (`flipperbridge_py.target`) bestätigt. `python3 -m py_compile` auf
beide gepatchten Kopien lief ohne Fehler.

Einzige Änderung:

```python
        self.s.write(data)
        self.s.flush()
        self._read_until(PROMPT, 15.0)
        # Verifizieren via md5 (eine leere Datei hat nichts zu vergleichen -- Ready+Schreiben
        # ist dort bereits der ganze Erfolgsnachweis, siehe Fund weiter unten)
        if not data:
            return True
        want = _md5(data)
        got = self.cmd(f'storage md5 "{remote_path}"').strip().split()[-1]
        return want == got
```

## Host-Repro

`repro-empty-file-send-verify/repro.py`: transkribiert ausschließlich den
Verifikationsschritt am Ende von `storage_send()` (ALT/NEU, 1:1 aus dem
Quelltext) über eine injizierbare `cmd_md5_result`-Variable, die den
Rückgabewert von `cmd('storage md5 "..."')` simuliert — kein `pyserial`-/
Hardwarezugriff nötig, da es ausschließlich um den reinen Python-
Vergleichsschritt geht (`write_chunk`/`Ready`/Schreiben laufen vor diesem
Schritt unverändert und sind für den Fund irrelevant).

Vier Fälle, `python3 repro.py`:
1. Nicht-leere Datei, Gerät meldet den korrekten Hash zurück → ALT und NEU
   liefern beide `True` (Regression: normaler Erfolgsfall unverändert).
2. Nicht-leere Datei, Gerät meldet einen falschen Hash zurück (simulierte
   Übertragungsstörung) → ALT und NEU liefern beide `False` (Regression: der
   Fix schwächt die echte Fehlererkennung nicht ab).
3. **Fund:** leere Datei → ALT liefert `False` (Soll-Hash
   `d41d8cd98f00b204e9800998ecf8427e` gegen Ist-Leerstring `""`), NEU liefert
   `True`.
4. Randfall: leere Datei, selbst wenn `cmd()` hypothetisch `""` zurückliefern
   würde → ALT bleibt `False` (der Fehler liegt am nie-leeren Soll-Hash, nicht
   am tatsächlichen Geräteverhalten), NEU liefert `True`.

Ergebnis: **ALLE PRÜFUNGEN BESTANDEN** — ALT reproduziert den Fund in beiden
leeren Fällen, NEU behebt ihn ohne die beiden nicht-leeren Regressionsfälle zu
verändern.

## Testhinweis (lokal, mit Hardware)

Patch gegen die eigene `flipperbridge.py`-Kopie anwenden. Dann:
`flipperbridge.py -p <port> send <leere-lokale-datei> /ext/test_empty.txt`
mit einer tatsächlich 0 Byte großen lokalen Datei (z. B. `touch leer.txt`)
ausführen. Vor dem Fix: Ausgabe "FEHLER: /ext/test_empty.txt", Exit-Code 1,
obwohl `storage stat /ext/test_empty.txt` auf dem Gerät eine vorhandene,
0 Byte große Datei zeigt. Nach dem Fix: Ausgabe "OK (md5 verifiziert)",
Exit-Code 0. Anschließend Regressionsprobe mit einer nicht-leeren Datei
wiederholen (Normalfall unverändert "OK"), und optional mit einer absichtlich
auf dem Gerät nachträglich veränderten Zieldatei (z. B. durch einen zweiten,
abweichenden Upload dazwischen) prüfen, dass ein echter Mismatch weiterhin
als "FEHLER" erkannt wird.

## Risiko

Sehr gering. Die Änderung betrifft ausschließlich den Fall `data == b""`
(vorher: garantiert `False`; jetzt: `True`) — ein Fall, der im Status quo nie
erfolgreich sein konnte, es gibt also keinen bestehenden Erfolgspfad, der
sich ändert. Für jede nicht-leere Datei ist der Kontrollfluss identisch zum
Status quo (`want`/`got`-Berechnung und Vergleich unverändert). Kein Einfluss
auf `storage_remove()`, den `write_chunk`-Handshake oder andere Methoden.

## Quelle

`G4MEOVER18/g4meover-companion`, `main`, Commit
`ceb068bf278feae4a8180d24330111e0b911301d`
(`flipperbridge/flipperbridge.py::FlipperBridge.storage_send()`).
