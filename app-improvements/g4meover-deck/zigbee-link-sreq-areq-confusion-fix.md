# g4meover-deck — ZigbeeLink._sreq() verwechselt unaufgeforderte AREQ-Frames mit der SRSP-Antwort

## Was
Ich habe das bisher komplett ungeprüfte `deck/zigbee_link.py` (CC2531-Koordinator über
das TI-Z-Stack-"Monitor-Test"-Protokoll, ZNP/MT) durchgesehen — eines der in
`dogytag-link-hardcoded-credentials-fix.md` als Rückstellpunkt benannten, noch nicht
geprüften Module (`device_discovery.py`, `handshake_capture.py`, `hackrf_link.py`,
`zigbee_link.py`, `wpa_crack.py`, `flipper_link.py`).

Das ZNP-Rahmenformat kodiert in den oberen drei Bit von CMD0 den Frame-Typ: `0x20` =
SREQ (Host → Koordinator, erwartet Antwort), `0x60` = SRSP (die erwartete Antwort),
`0x40` = AREQ (vom Koordinator jederzeit unaufgefordert gesendet, z. B. beim Boot
`SYS_RESET_IND`, während des Netzwerkbetriebs `ZDO_STATE_CHANGE_IND`,
`AF_INCOMING_MSG`, Device-Announce). Die unteren fünf Bit sind das Subsystem (z. B.
`0x01` = SYS). `ZigbeeLink._sreq()` sendet ein SREQ und liest dann den **ersten**
Frame, der mit `SOF` (`0xFE`) beginnt, ohne dessen CMD0/CMD1 gegen das tatsächlich
erwartete SRSP zu prüfen:

```python
def _sreq(self, cmd, data: bytes = b"", timeout: float = 1.5):
    """Sendet SREQ, liest die SRSP-Nutzdaten (oder None)."""
    assert self._s
    self._s.reset_input_buffer()
    self._s.write(_frame(cmd[0], cmd[1], data))
    end = time.time() + timeout
    # auf SOF synchronisieren
    while time.time() < end:
        b = self._s.read(1)
        if b == bytes([SOF]):
            hdr = self._s.read(3)  # LEN, CMD0, CMD1
            if len(hdr) < 3:
                return None
            ln = hdr[0]
            payload = self._s.read(ln + 1)  # DATA + FCS
            if len(payload) < ln + 1:
                return None
            return payload[:ln]          # <-- ungeprüft als Antwort übernommen
    return None
```

Trifft zwischen dem Senden des SREQ und dem Eintreffen der eigentlichen SRSP ein
unaufgefordertes AREQ ein — beim CC2531-als-Koordinator-Betrieb ein normaler, laut
Z-Stack-Dokumentation jederzeit möglicher Vorgang, nicht nur ein seltener Grenzfall —
hält `_sreq()` dessen Nutzdaten für die Antwort auf das eigene SREQ und gibt sie
zurück. `ping()` würde dann z. B. aus einem `ZDO_STATE_CHANGE_IND`-Frame ein völlig
falsches "Capabilities"-Bitmuster herauslesen, `version()` entsprechend eine falsche
Versions-/Transport-ID — beides ohne jede Fehlermeldung, da `_sreq()` aus Sicht des
Aufrufers ganz normal einen Wert zurückliefert. Die echte SRSP bleibt derweil
unverarbeitet im Puffer und wird erst vom nächsten `_sreq()`-Aufruf per
`reset_input_buffer()` verworfen (verschiebt das Problem nur auf den nächsten
Befehl, löst es nicht).

Vergleich mit der eigenen Kopfzeilen-Dokumentation: `"Live verifiziert: SYS_PING ->
fe 02 61 01 79 01 1a (cap=0x0179)"` — die Antwort trägt `CMD0=0x61` (SRSP-Typ-Bits
`0x60` + Subsystem `0x01`), während das gesendete SREQ laut `SYS_PING = (0x21, 0x01)`
`CMD0=0x21` (SREQ-Typ-Bits `0x20` + dasselbe Subsystem) trägt — die Datei dokumentiert
den Unterschied also bereits korrekt, prüft ihn im Code aber nirgends.

## Nutzen
- Schließt eine stille Fehlerquelle: `_sreq()` verwirft jetzt jeden eingehenden Frame,
  dessen CMD0/CMD1 nicht exakt der erwarteten SRSP-Signatur für das gesendete Kommando
  entspricht, und liest innerhalb des bestehenden Zeitbudgets weiter — statt den
  erstbesten SOF-Frame ungeprüft als Antwort zu akzeptieren.
- `ping()`/`version()`/`info()` liefern dadurch entweder die tatsächliche Antwort des
  Koordinators oder `None`/`reachable: False` (korrekt erkennbarer Fehlschlag) — nie
  mehr eine aus einem fremden AREQ fehlinterpretierte, plausibel aussehende, aber
  falsche "Erfolgs"-Antwort.
- Betrifft potenziell jede Stelle, die `ZigbeeLink` im laufenden Netzwerkbetrieb nutzt
  (z. B. ein Status-Check per `info()`, während der Koordinator gerade ein
  Device-Announce oder einen Zustandswechsel meldet) — nicht nur den seltenen
  Boot-Moment.

## Betroffene Dateien
- `deck/zigbee_link.py` — `ZigbeeLink._sreq()`: neue erwartete SRSP-CMD0-Berechnung
  (`(cmd[0] & 0x1F) | 0x60`) sowie eine CMD0/CMD1-Prüfung vor der Rückgabe.

## Patch
Unified-Diff gegen die unveränderte `G4MEOVER18/g4meover-deck`-Basis (main, Commit
`b030e45`) in
[`zigbee-link-sreq-areq-confusion-fix/zigbee_link_py.patch`](zigbee-link-sreq-areq-confusion-fix/zigbee_link_py.patch).
Gegen zwei frische, unabhängige Zweitkopien mit `git apply --check` verifiziert, real
angewendet, byte-für-byte abgeglichen — identisch.

## Testhinweis (ohne Hardware)
[`repro-zigbee-link-sreq-areq-confusion/repro.py`](repro-zigbee-link-sreq-areq-confusion/repro.py)
transkribiert `_frame()` sowie `_sreq()` ALT (vor dem Fix) und NEU (nach dem Fix) 1:1
als eigenständige Funktionen; ein `MockSerial` bildet nur das von `_sreq()` tatsächlich
genutzte Teilinterface (`write()`/`read()`/`reset_input_buffer()`) nach, liefert dabei
einen vorab zusammengesetzten Byte-Strom statt echter Hardware. Acht Szenarien:

```
$ python3 repro.py
[OK] Regression ALT: ping() liefert cap=0x0179 ohne AREQ-Stoerung
[OK] Regression NEU: ping() liefert cap=0x0179 ohne AREQ-Stoerung
[OK] Fund ALT: liest die AREQ-Nutzdaten (0x0002) statt der echten SRSP (0x0179) -- cap=0x0002
[OK] Fix NEU: verwirft die AREQ, liest weiter bis zur echten SRSP (cap=0x0179)
[OK] Fund ALT (nur AREQ, keine SRSP): liefert dennoch einen Wert statt eines Fehlers -- cap=0x0002
[OK] Fix NEU (nur AREQ, keine SRSP): liefert korrekt None statt einer falschen Antwort
[OK] Fix NEU (drei AREQs vor der SRSP): findet trotzdem die echte Antwort (cap=0x0179)
[OK] Fix NEU (SYS_VERSION): verwirft AREQ, liefert korrekte SRSP-Nutzdaten
[OK] Fix NEU (fremde SRSP vor der eigenen): verwirft SYS_VERSION-Antwort, findet die echte SYS_PING-SRSP (cap=0x0179)
[OK] Aequivalenz ohne AREQ, len=0..19: ALT == NEU
29/29 Einzelprüfungen bestanden -- ALLE CHECKS BESTANDEN
```

Deckt ab: (1) Regression ohne jede Störung (ALT/NEU identisch), (2) den eigentlichen
Fund — eine AREQ läuft der echten SRSP voraus, ALT liefert deren Nutzdaten als falsche
Antwort, NEU verwirft sie korrekt, (3) nur eine AREQ ohne nachfolgende SRSP innerhalb
des Zeitbudgets — ALT meldet einen falschen "Erfolg", NEU korrekt `None`, (4) mehrere
AREQs hintereinander vor der echten SRSP, (5) ein anderes Kommando (`SYS_VERSION`) und
eine *fremde* SRSP (für ein anderes Kommando), die NEU ebenfalls korrekt verwirft, (6)
einen Äquivalenztest über 20 Antwortlängen ohne Störung.

Lokaler Funktionstest (mit Hardware): CC2531 am Pi anschließen, `ZigbeeLink(port).info()`
während aktivem Netzwerkbetrieb (z. B. während ein weiteres Zigbee-Gerät joint oder der
Koordinator per `zbSystemResetInd`/`ZDO_STATE_CHANGE_IND` typische AREQs sendet)
wiederholt aufrufen — vor dem Fix können einzelne Aufrufe sporadisch ein plausibel
aussehendes, aber falsches `capabilities`-Bitmuster liefern (beobachtbar z. B. als
`cap_names()`, die nicht zum tatsächlichen Koordinator passen); nach dem Fix bleibt
`info()` in diesem Fall entweder korrekt oder liefert `reachable: False`/`None` statt
einer falschen Antwort. Ohne eingehende AREQs (ruhiger Bus, z. B. direkt nach
`SYS_RESET`) unverändertes Verhalten vor/nach dem Fix (siehe Regressionsszenario).

## Risiko
- Niedrig: additive Prüfung vor der bestehenden `return payload[:ln]`-Zeile, kein
  bestehender Erfolgspfad wird verändert — nur zusätzlich fehlerhafte Treffer
  ausgeschlossen. Das Rahmenformat (`_frame()`, FCS-Berechnung) bleibt unverändert.
- Verlängert im ungünstigsten Fall (mehrere AREQs direkt hintereinander vor der echten
  SRSP) die Antwortzeit innerhalb des bereits bestehenden `timeout`-Budgets — führt zu
  keinem neuen Fehlschlag, der nicht auch vorher durch einen generell zu langsamen
  Koordinator möglich gewesen wäre.
- Prüft bewusst nicht die FCS der eingehenden Antwort (bereits vor diesem Fix nicht
  geprüft, eigener, kleinerer Rückstellpunkt für eine künftige Sitzung) — dieser Patch
  behebt ausschließlich die Typverwechslung zwischen SRSP und AREQ.

## Quelle
- `G4MEOVER18/g4meover-deck` (main, Commit `b030e45`): `deck/zigbee_link.py`
  (`ZigbeeLink._sreq()`, `_frame()`, Kopfzeilen-Dokumentation des live verifizierten
  SYS_PING-Antwortrahmens).
- Texas Instruments Z-Stack Monitor-and-Test API (ZNP/MT): CMD0-Frame-Typ-Bits
  (SREQ `0x20`/SRSP `0x60`/AREQ `0x40`/POLL `0x00`) sowie die dokumentierten
  unaufgeforderten AREQs (`SYS_RESET_IND`, `ZDO_STATE_CHANGE_IND`, `AF_INCOMING_MSG`,
  Device-Announce) — Standard-Verhalten jedes Z-Stack-Koordinators, kein
  CC2531-spezifischer Sonderfall.
