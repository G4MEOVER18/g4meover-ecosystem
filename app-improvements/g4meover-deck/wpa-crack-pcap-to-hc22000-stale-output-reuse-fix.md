# g4meover-deck — wpa_crack.pcap_to_hc22000() liest beim Fehlschlag oder bei Wiederverwendung des Default-Pfads einen stehengebliebenen Altstand statt des aktuellen .pcap

## Was
Ich habe `deck/tools/wpa_crack.py` (CLI-Frontend) und `deck/handshake_capture.py`
durchgesehen — beide seit der letzten Sitzung als Rückstellpunkt benannten,
noch nicht geprüften Module. `handshake_capture.py` (serielles [HSRAW]-Parsing,
pcap-Schreiben) ohne tragfähigen eigenen Fund gelesen: `parse_hsraw()` verwirft
ungerade/zu kurze Hex-Fragmente bereits korrekt, `write_pcap()`s Global-Header
(24 Byte) und Record-Header (16 Byte) entsprechen exakt dem klassischen
.pcap-Format, `capture_from_serial()` hält DTR/RTS beim Öffnen bewusst tief
(reset-sicher, analog zu `device_discovery.py`). `deck/tools/wpa_crack.py`
selbst (Argparse-Frontend für `deck/wpa_crack.py`) ebenfalls ohne eigenen Fund
durchgesehen — die Weiterleitung `capture` → `info`/`dict` setzt `args.pcap`/
`args.cmd`/`args.timeout` konsistent.

Das führte zurück zur bereits einmal (für `show_cracked()`s `:`-Trunkierung)
geprüften, aber nicht vollständig gelesenen `deck/wpa_crack.py` — dort jetzt
`pcap_to_hc22000()` genauer angesehen:

```python
out_path = out_path or (os.path.splitext(pcap)[0] + ".hc22000")
try:
    subprocess.run(["hcxpcapngtool", "-o", out_path, pcap],
                   capture_output=True, text=True, timeout=120)
except Exception as e:
    return {"error": f"hcxpcapngtool-Fehler: {e}", "networks": [], "count": 0}
if not os.path.isfile(out_path) or os.path.getsize(out_path) == 0:
    return {"error": "keine Handshakes/PMKID im .pcap gefunden", ...}
nets = parse_hc22000(open(out_path, ...).read())
return {"hc22000": out_path, "networks": nets, "count": len(nets), "error": ""}
```

Zwei zusammenhängende Lücken:

1. **`hcxpcapngtool` hängt an eine bestehende `-o`-Zieldatei an, statt sie zu
   überschreiben** — das ist kein eigener Verdacht, sondern vom Maintainer-Team
   selbst im hashcat-Forum dokumentiert ("hcxpcapngtool appends entries to an
   existing file. Deleting a previous output file before re-running can avoid
   confusing results."). `pcap_to_hc22000()` löscht `out_path` vor dem Aufruf
   nirgends. Der von mir selbst in `deck/tools/wpa_crack.py` gebaute
   `capture`-Workflow benutzt standardmäßig **denselben** Dateinamen
   (`out_pcap` default `handshake.pcap` → Default-`out_path`
   `handshake.hc22000`) für jede Sitzung, sofern der Aufrufer nichts anderes
   angibt. Zwei aufeinanderfolgende `wpa_crack.py capture COM26` (zwei
   unterschiedliche Luftaufnahmen, z. B. erst das eigene Testnetz, dann ein
   zweites) liefern beim zweiten Mal eine **Mischung** aus dem alten und dem
   neuen Netz/Passwort — nicht nur das, was in der zweiten Aufnahme tatsächlich
   drinsteckt. `parse_hc22000()`s Dedup nach `(bssid,ssid)` hilft hier nicht,
   weil es sich um zwei *verschiedene* Netze handelt, nicht um ein doppelt
   geparstes.
2. **Kein `returncode`-Check.** `subprocess.run()`s Rückgabewert wird komplett
   verworfen. Bricht `hcxpcapngtool` ab, **bevor** es `out_path` überhaupt zum
   Schreiben öffnet (ungültiges/beschädigtes `.pcap`, falsches Argument,
   fehlende Berechtigung auf das Zielverzeichnis) — und liegt am selben
   `out_path` noch eine nicht-leere Datei aus einer früheren, unabhängigen
   Sitzung —, greift die bestehende `isfile`/`getsize`-Prüfung nicht: Sie sieht
   eine vorhandene, nicht-leere Datei und meldet sie als **Erfolg** mit
   `error: ""`, obwohl der aktuelle Lauf komplett fehlgeschlagen ist. Der
   Aufrufer (CLI oder ein künftiges Dashboard) zeigt dann ein Netz/Passwort aus
   einer alten Aufnahme an und hält es für den Fund der aktuellen.

Beide Lücken greifen über denselben Mechanismus (stehengebliebener Altstand am
`out_path`) und werden hier zusammen behoben.

## Nutzen
- Jeder `pcap_to_hc22000()`-Aufruf liest ab jetzt ausschließlich, was
  `hcxpcapngtool` **in diesem** Lauf tatsächlich aus **diesem** `.pcap`
  extrahiert hat — kein Vermischen mit einer früheren Sitzung mehr, auch wenn
  `out_path` (Standardfall: Default-Namensschema) wiederverwendet wird.
- Ein echter Tool-Fehlschlag wird jetzt als Fehler mit `rc`/Tool-Meldung
  gemeldet, statt durch einen zufällig am selben Pfad liegenden Altstand
  verdeckt zu werden — wichtig gerade für den `capture`-Workflow, der
  standardmäßig denselben Dateinamen über mehrere Sitzungen hinweg verwendet.
- `handshake_capture.py` und `deck/tools/wpa_crack.py` sind damit vollständig
  durchgesehen; der komplette ehemalige Rückstellpunkt (`device_discovery.py`,
  `handshake_capture.py`, `hackrf_link.py`, `deck/tools/wpa_crack.py`,
  `wpa_crack.py`) ist jetzt abgearbeitet.

## Betroffene Dateien
- `deck/wpa_crack.py` — `pcap_to_hc22000()`: löscht einen vorhandenen `out_path`
  vor dem `hcxpcapngtool`-Aufruf (mit explizitem Fehler, falls das Löschen
  selbst scheitert); prüft danach `proc.returncode` und liefert bei einem
  echten Tool-Fehlschlag eine eigene Fehlermeldung inkl. `stderr`/`stdout`
  statt stillschweigend weiterzumachen. `detect()`, `parse_hc22000()`,
  `_fmt_mac()`, `show_cracked()`, `crack_dictionary()`, `crack_mask()`
  unverändert — kein paralleler Fund in diesen Funktionen.

## Patch
`wpa-crack-pcap-to-hc22000-stale-output-reuse-fix/wpa_crack_py.patch`
(Unified-Diff gegen `deck/wpa_crack.py`). Gegen zwei frische, unabhängige
Zweitkopien der unveränderten `G4MEOVER18/g4meover-deck`-Basis (`b030e45`) mit
`git apply --check` verifiziert, real angewendet, Ergebnis per `md5sum`
byte-für-byte abgeglichen — identisch. Anschließend
`python3 -m py_compile deck/wpa_crack.py` auf der gepatchten Datei bestanden.

## Testhinweis (ohne hcxtools/hashcat/Hardware)
Host-Repro
(`repro-wpa-crack-pcap-to-hc22000-stale-output-reuse/repro.py`): ein winziges
Fake-`hcxpcapngtool`-Skript wird auf `PATH` gelegt, das genau das vom
Maintainer-Team dokumentierte Verhalten simuliert (Modus `ok_truncate` =
einziger Fund, überschreibt; `ok_append` = zweiter Fund, hängt an die
bestehende Datei an; `fail_before_open` = Abbruch vor dem Öffnen von
`out_path`) — kein echtes `hcxtools`/`hashcat` nötig. `pcap_to_hc22000_alt()`/
`_neu()` transkribieren die ALT- bzw. NEU-Logik 1:1, rufen aber dasselbe echte,
unveränderte `wpa_crack.parse_hc22000()` auf. **7 Prüfungen**: `ALL CHECKS
PASSED (7 total)`. Geprüft:
1. Regressionsfall — ein einzelner, frischer Lauf ohne jede Vorgeschichte
   liefert bei ALT und NEU identisch das eine echte Netz.
2. Der eigentliche Fund — zwei aufeinanderfolgende Erfolgsläufe auf denselben
   `out_path` (z. B. zwei `capture`-Sitzungen mit Standard-Dateinamen): ALT
   meldet beim zweiten Lauf fälschlich **beide** Netze (altes + neues)
   vermischt; NEU meldet beim zweiten Lauf korrekt **nur** das neue.
3. Tool-Fehlschlag vor dem Öffnen von `out_path`, mit einem Altstand aus einer
   früheren, unabhängigen Sitzung am selben Pfad: ALT meldet fälschlich Erfolg
   mit dem alten Fund und leerem `error`; NEU meldet `count: 0` und einen
   echten Fehler, der Altstand ist danach vom Pfad entfernt.

Lokal zusätzlich sinnvoll: zwei echte `wpa_crack.py capture <port>`-Sitzungen
mit demselben Standard-`out_pcap`-Namen gegen zwei unterschiedliche eigene
Testnetze hintereinander ausführen und `info` dazwischen aufrufen — vor dem
Fix zeigt der zweite `info`-Aufruf beide Netze, nach dem Fix nur das zuletzt
aufgenommene.

## Risiko
- `os.remove(out_path)` vor dem Tool-Aufruf ist die einzige neue
  Seiteneffekt-Änderung — betrifft ausschließlich die vom Aufrufer selbst
  übergebene/abgeleitete `.hc22000`-Datei, nie das Eingabe-`.pcap`. Schlägt das
  Löschen selbst fehl (z. B. Datei durch einen anderen Prozess gesperrt),
  bricht die Funktion jetzt mit einer klaren Fehlermeldung ab, statt (wie
  vorher implizit) mit möglicherweise vermischtem Ergebnis weiterzumachen.
- Keine Breaking Change an der Funktionssignatur oder am Rückgabeformat
  (`{hc22000, networks, count, error}` unverändert) — bestehende Aufrufer
  (`deck/tools/wpa_crack.py`) sind von außen nicht betroffen, erhalten bei
  einem echten Tool-Fehlschlag jetzt nur eine ehrlichere `error`-Meldung statt
  eines stillen Fehlerfolgs.
- `crack_dictionary()`/`crack_mask()` rufen `pcap_to_hc22000()` nicht auf (sie
  bekommen ein bereits fertiges `hc22000` übergeben) — von diesem Fix nicht
  betroffen.

## Quelle
- `G4MEOVER18/g4meover-deck` (main, `b030e45`): `deck/wpa_crack.py`
  (`pcap_to_hc22000()`, Gegenprobe `detect()`/`parse_hc22000()`/
  `show_cracked()`/`crack_dictionary()`/`crack_mask()`), `deck/tools/wpa_crack.py`
  (CLI-Frontend, `capture`→`info`/`dict`-Weiterleitung), `deck/handshake_capture.py`
  (`parse_hsraw()`, `write_pcap()`, `capture_from_serial()`).
- hashcat-Forum (Maintainer-Aussage zum Anhänge-statt-Überschreiben-Verhalten
  von `hcxpcapngtool` bei einer bestehenden `-o`-Zieldatei):
  https://hashcat.net/forum/
