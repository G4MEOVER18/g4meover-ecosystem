# g4meover-deck — wpa_crack.show_cracked() trunkiert Passwörter mit ':' im Klartext

## Was
Ich habe `deck/wpa_crack.py` durchgesehen, eines der in
`zigbee-link-sreq-areq-confusion-fix.md` als Rückstellpunkt benannten, noch nicht
geprüften Module (`device_discovery.py`, `handshake_capture.py`, `hackrf_link.py`,
`lorix_link.py`, `wpa_crack.py`).

`show_cracked()` liest die Ausgabe von `hashcat -m 22000 <hc22000> --show` zeilenweise
ein. Jede Zeile besteht aus dem vollständigen hc22000-Hash (Felder durch `*` getrennt,
selbst **nie** mit `:` darin) gefolgt von einem `:` und dem geknackten Klartext-Passwort
— genau das von hashcat dokumentierte Potfile-Format `hash:plain`. Der Code extrahiert
das Passwort bisher so:

```python
# letztes Feld hinter dem letzten ':' ist das Passwort
pw = line.rsplit(":", 1)[-1] if ":" in line else ""
```

`rsplit(":", 1)` trennt am **letzten** `:` der Zeile. Das ist nur korrekt, solange das
Passwort selbst kein `:` enthält. WPA-PSK-ASCII-Passphrasen dürfen laut IEEE 802.11i
jedes druckbare ASCII-Zeichen 0x20–0x7E (8–63 Zeichen) enthalten — Doppelpunkt
eingeschlossen, ein völlig gültiges und nicht einmal ungewöhnliches Zeichen in
selbstgewählten Passwörtern. Enthält das geknackte Passwort ein oder mehrere `:`,
schneidet `rsplit(":", 1)[-1]` alles bis zum letzten `:` ab und liefert nur den Rest:

- Passwort `pass:word` → zurückgegeben wird nur `word`.
- Passwort `a:b:c:super:geheim` → zurückgegeben wird nur `geheim`.

Der Hash-Teil der Zeile selbst enthält nie `:` (nur `*`-getrennte Felder), sodass der
**erste** `:` in der Zeile immer der von hashcat eingefügte Trenner zwischen Hash und
Klartext ist — alles danach, inklusive eventueller weiterer `:`, ist das vollständige
Passwort.

Für ein Pentest-Werkzeug ist das kein kosmetischer Fehler: `show_cracked()` meldet
keinen Fehler und keinen Hinweis auf Kürzung — der Aufrufer (`crack_dictionary()`/
`crack_mask()`, letztlich die Dashboard-Anzeige) hält das abgeschnittene Fragment für
das vollständige, tatsächliche WLAN-Passwort. Ein darauf aufbauender Verbindungsversuch
mit dem (falschen) Fragment schlägt fehl, ohne dass ersichtlich wäre, dass der
eigentliche Cracking-Erfolg bereits vorlag — der tatsächliche Fund wird im Potfile
weiterhin korrekt gespeichert (hashcat selbst schneidet nichts ab), nur diese
Python-Extraktion verliert ihn.

## Nutzen
- `show_cracked()` liefert ab jetzt für jedes bereits geknackte Netz das vollständige
  Passwort, unabhängig davon, ob und wie oft `:` darin vorkommt.
- Betrifft jeden Aufrufer, der auf ein vollständiges, korrektes Passwort angewiesen ist
  (`crack_dictionary()`/`crack_mask()` geben `show_cracked()`s Ergebnis unverändert als
  `cracked` zurück; die Dashboard-Anzeige übernimmt es 1:1).
- `ssid`/`bssid`-Extraktion in derselben Funktion ist von dem Fund nicht betroffen (sie
  liest `f[5]`/`f[3]` aus dem bereits durch `split("*")` zerlegten Zeilenanfang,
  unabhängig vom Passwort-Suffix) — der Fix ändert ausschließlich die Passwort-Zeile.

## Betroffene Dateien
- `deck/wpa_crack.py` — `show_cracked()`: `line.rsplit(":", 1)[-1]` (letzter
  Doppelpunkt) durch `line.split(":", 1)[1]` (erster Doppelpunkt) ersetzt.

## Patch
Unified-Diff gegen die unveränderte `G4MEOVER18/g4meover-deck`-Basis (main, Commit
`b030e45`) in
[`wpa-crack-show-cracked-colon-password-truncated-fix/wpa_crack_py.patch`](wpa-crack-show-cracked-colon-password-truncated-fix/wpa_crack_py.patch).
Gegen zwei frische, unabhängige Zweitkopien mit `git apply --check` verifiziert, real
angewendet, per `md5sum` abgeglichen — identisch.

## Testhinweis (ohne Hardware, ohne hashcat/hcxtools)
[`repro-wpa-crack-show-cracked-colon-password-truncated/repro.py`](repro-wpa-crack-show-cracked-colon-password-truncated/repro.py)
transkribiert ausschließlich die betroffene Schleife aus `show_cracked()` als
`_parse_show_ALT()`/`_parse_show_NEU()`; `_fmt_mac()` wird unverändert aus der
unangetasteten Originaldatei importiert (`wpa_crack_orig.py`, 1:1-Kopie des
geprüften Quelltexts), kein hashcat- oder hcxpcapngtool-Aufruf nötig — die Eingabe ist
genau der Text, den `hashcat -m 22000 ... --show` auf stdout schreibt:

```
$ python3 repro.py
[OK] Regression ALT: Passwort ohne ':' korrekt
[OK] Regression NEU: Passwort ohne ':' korrekt
[OK] Fund ALT: Passwort mit einem ':' wird trunkiert (Bug reproduziert)
[OK] Fix NEU: Passwort mit einem ':' bleibt vollstaendig
[OK] Fund ALT: Passwort mit mehreren ':' stark trunkiert
[OK] Fix NEU: Passwort mit mehreren ':' vollstaendig
[OK] Fix NEU: zwei Zeilen, nur zweite mit ':' im Passwort -- beide korrekt
[OK] ssid/bssid unveraendert fuer Passwort 'Sommer2026'
[OK] ssid/bssid unveraendert fuer Passwort 'pass:word'
[OK] ssid/bssid unveraendert fuer Passwort 'a:b:c:super:geheim'
[OK] Randfall ohne ':': ALT liefert leeres Passwort statt Crash
[OK] Randfall ohne ':': NEU liefert leeres Passwort statt Crash
[OK] Mutationstest: mutierte NEU (rsplit) reproduziert den Bug wieder (Repro ist scharf)
13/13 Einzelprüfungen bestanden -- ALLE CHECKS BESTANDEN
```

Deckt ab: (1) Regression ohne `:` im Passwort (ALT/NEU identisch korrekt), (2) den
eigentlichen Fund — ein `:` im Passwort, ALT trunkiert auf das letzte Fragment, NEU
liefert das vollständige Passwort, (3) verschärfter Fall mit mehreren `:`, (4) zwei
Zeilen in derselben `--show`-Ausgabe (nur eine mit `:` im Passwort) unabhängig korrekt
verarbeitet, (5) `ssid`/`bssid`-Extraktion über alle Fälle hinweg unverändert, (6) der
von `show_cracked()` selbst behandelte Randfall ganz ohne `:` (kein Crash, leeres
Passwort). Eine Mutationstest-Gegenprobe (eine mutierte NEU-Variante zurück auf
`rsplit`) reproduziert den Fund erneut — bestätigt, dass das Repro selbst scharf ist
und nicht zufällig immer grün anschlägt.

Lokaler Funktionstest (mit hashcat, ohne Flipper-Hardware): einen `.hc22000`-Eintrag mit
einem absichtlich auf ein Passwort mit `:` gesetzten Potfile-Treffer erzeugen (z. B.
`echo "<hash>:Test:Pass:2026" >> ~/.local/share/hashcat/hashcat.potfile`, Hash-Teil aus
einer eigenen `.hc22000`-Datei), danach `wpa_crack.show_cracked("<hc22000-Pfad>")`
aufrufen — vor dem Fix erscheint nur `"2026"`, nach dem Fix `"Test:Pass:2026"`.

## Risiko
- Niedrig: ändert ausschließlich die Trennstelle innerhalb derselben Zeile (erster statt
  letzter `:`), keine neue Abhängigkeit, kein verändertes Verhalten für den
  Normalfall (Passwort ohne `:`) — dort sind `rsplit(":", 1)[-1]` und
  `split(":", 1)[1]` identisch (Beleg: Regressionsprüfung (1) im Repro).
- Setzt voraus, dass der Hash-Teil der Zeile selbst kein `:` enthält — gilt für das
  hc22000-Format durchgehend (`*`-getrennte Felder, siehe `parse_hc22000()` in
  derselben Datei, das dieselbe Annahme bereits für den Hash-Teil trifft).

## Quelle
- `G4MEOVER18/g4meover-deck` (main, Commit `b030e45`): `deck/wpa_crack.py`
  (`show_cracked()`).
- hashcat-Dokumentation: Potfile-/`--show`-Ausgabeformat `hash:plain`; hc22000
  (`-m 22000`) Hash-Feldformat `WPA*TYPE*HASH*MAC_AP*MAC_STA*ESSID*...` (ausschließlich
  `*`-getrennt, keine `:` innerhalb des Hash-Teils).
- IEEE 802.11i: WPA-PSK-ASCII-Passphrase, 8–63 druckbare ASCII-Zeichen (0x20–0x7E),
  Doppelpunkt eingeschlossen.
