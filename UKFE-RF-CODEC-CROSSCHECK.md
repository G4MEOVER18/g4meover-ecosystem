# ukfe_rf-Codec-Gegenprobe (Cloud-Heartbeat 2026-10-13)

Verifiziert den in `deck/ukfe_rf.py` (`g4meover-deck`) als "byte-genau gegen die
C-Implementierung verifiziert" dokumentierten Python-Port **tatsächlich** gegen die reale
Firmware-Quelle — bisher war das in diesem Tracker (P1.2b/P1.5b, `UKFE-RF-V2-SPEC.md`) als
"lokal, nicht aus der Cloud lieferbar" markiert, mit der Begründung, `heltec-v4-hub`/
`lora-ukfe`/`heltec-ukfe-rx` lägen außerhalb des Zugriffs-Scopes dieser Cloud-Sitzung. Das
stimmt nicht mehr (falls es je zutraf): alle drei Firmware-Repos sind öffentlich und über
`add_repo` lesend erreichbar — dieselbe Lücke, die am 2026-10-12 schon für `g4meover-deck`/
`g4meover-companion` aufgedeckt wurde, betraf offenbar den gesamten Firmware-Repo-Satz.

## Befund 1: `ukfe_rf.c` ist über drei Firmware-Repos byte-identisch
`diff` zwischen den drei `ukfe_rf.c`-Dateien (jeweils frischer, unabhängiger
`--depth 1`-Klon):
- `heltec-v4-hub/src/ukfe_rf.c`
- `lora-ukfe/rf/ukfe_rf.c`
- `heltec-ukfe-rx/src/ukfe_rf.c`

→ alle drei Dateien **byte-identisch** (keine Diff-Ausgabe, Exit-Code 0). Die
`ukfe_rf.h`-Header unterscheiden sich nur im `UkfeRfCmd`/`UkfeRfResp`-Enum (zusätzliche,
später ergänzte Kommando-Codes + Kommentare je Zielgerät) — Frame-Layout, Konstanten
(`MAGIC`/`VERSION`/`SECRET_LEN`/`MAX_ARGS`/`HDR_OVERHEAD`) und Funktionssignaturen sind in
allen drei identisch.

## Befund 2: Python-Port matcht die reale C-Implementierung Byte für Byte
Host-Repro (reines C, keine Hardware-Abhängigkeit laut eigenem Kommentar in `ukfe_rf.c`
Zeile 1 — "plattformunabhaengig, ohne HW-Abhaengigkeit"): ein kleiner Treiber
(`harness.c`) bindet `ukfe_rf.c`/`.h` direkt aus `heltec-v4-hub` ein, baut fünf Frames mit
festem Test-Secret und gibt sie als Hex aus. Dieselben fünf Fälle (gleiches Secret, gleiche
Counter/Cmd/Args) liefen durch `deck/ukfe_rf.py::build_frame()`.

**Testfälle** (Secret `00 11 22 … EE FF`):
1. `CMD_STATUS`, counter=1000, keine Args
2. `CMD_TRIGGER`, counter=1, args=`id=5, delay_ms=1234`
3. `CMD_WIFI_DEAUTH`, counter=`0xFFFFFFFF` (Rand-Wert), args=BSSID+Channel
4. counter=0, cmd=0, keine Args (Rand-Wert)
5. cmd=`0x7F`, args=40 Byte (`UKFE_RF_MAX_ARGS`, Rand-Wert)

**Ergebnis:** alle fünf Frames **byte-identisch** zwischen C (`gcc -O0`/`-O1 -Wall -Wextra
-fsanitize=address,undefined`, keine Warnungen/Sanitizer-Meldungen, identischer Output bei
beiden Optimierungsstufen) und Python (`python3 -I`, `hashlib`/`hmac`-frei, reine
Bit-Arithmetik wie im Original):

```
0E4701E803000001007FC5D08A8A52
13470101000000020505D2040000C6AE15D3E6E3
154701FFFFFFFF2107AABBCCDDEEFF06727AD80D3B74
0E47010000000000004B18C217DE29
3647013F420F007F2841414141414141414141414141414141414141414141414141414141414141414141414141414141D06821990D5D
```

Zusätzlich ein Build→Parse-Rundlauf (`ukfe_rf_build_frame()` → `ukfe_rf_parse_frame()`,
real aus `heltec-v4-hub`): MAC- und CRC-Prüfung bestehen, `last_counter` wird korrekt auf
den neuen Counter gesetzt (Anti-Replay-Pfad).

## Einordnung für den Tracker
- **CRC16-CCITT (`0x1021`/Init `0xFFFF`) und der keyed-`mix32`-MAC sind damit nicht mehr nur
  "laut deck-Doku verifiziert", sondern hier direkt gegen alle drei realen
  Firmware-Repos gegengeprüft** — P1.2b/P1.5b können den Satz "nur lokal gegen die echte
  Firmware verifizierbar" für den Codec selbst streichen; offen bleibt nur noch die
  **Integration** ins Cockpit (echte Serial-Verbindung, reales Secret aus `secret_local.py`
  statt des öffentlichen Platzhalters) — das bleibt zu Recht ein lokaler Schritt.
- Gilt nur für v1 (`UKFE_RF_VERSION = 0x01`). Die in `UKFE-RF-V2-SPEC.md` entworfene
  v2-Erweiterung (`SRC_ID`/`ORIGIN_ID`, BLE-Mapping) existiert in keinem der drei
  Firmware-Repos — dort bleibt die Spec weiterhin ein Entwurf, kein verifizierter Stand.
- Die Begründung "außerhalb des GitHub-Zugriffs-Scopes dieser Cloud-Sitzung" für P2.1b
  (`G4MEOVER-FW`-RPC), P2.4b (Flipper-BLE-RPC-UUIDs/Protobuf), P3.1a/P3.2a
  (OLED-Zeichenroutine), P4.3a (LoRa-Sniff-Register) sollte in einer künftigen Sitzung
  jeweils **zuerst per `add_repo` gegengeprüft** werden, bevor sie als Blocker wiederholt
  wird — mindestens vier der fünf dort genannten Repos (`heltec-v4-hub`, `lora-ukfe`,
  `heltec-ukfe-rx`, `g4meover-companion`) sind nachweislich erreichbar; nur `G4MEOVER-FW`
  selbst (Flipper-Firmware, für P2.1b/P2.4b/P8.1 relevant) war in dieser Sitzung noch nicht
  konkret geprüft.

## Nicht verändert
Nur gelesen (frische `--depth 1`-Klone von `heltec-v4-hub`, `lora-ukfe`, `heltec-ukfe-rx`,
`g4meover-deck`) — keine der vier Repos verändert, committet oder gepusht. Der
Host-Repro-Treiber selbst ist reines Verifikations-Tooling für diese Gegenprobe (kein
Firmware-/GUI-Code für eine der Zielplattformen), lief nur im Cloud-Container.
