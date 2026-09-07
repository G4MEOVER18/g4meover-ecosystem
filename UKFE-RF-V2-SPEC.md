# ukfe_rf v2 — Protokoll-Spec (Entwurf)

Status: **Entwurf** (Masterplan-TODO P0.4). Cloud-Anteil (Format/Mapping/Command-Vorschläge)
abgeschlossen — lokale Firmware-Owner-Review und Umsetzung offen (siehe Abschnitt 7).

## 1. Ausgangslage: v1 (Ist-Zustand)

Aus [`FLEET-COMMANDS.md`](FLEET-COMMANDS.md):

```
[LEN][0x47][0x01][COUNTER(4)][CMD][ALEN][ARGS][MAC4][CRC16(2)]
```

- `0x47` = Magic, `0x01` = Versions-Byte
- `COUNTER(4)` = Rolling Counter gegen Replay
- `CMD` = Befehls-Byte (siehe Befehls-Matrix in FLEET-COMMANDS.md)
- `ALEN`+`ARGS` = Argument-Länge + -Daten
- `MAC4` = keyed MAC (4 Byte) zur Authentifizierung
- `CRC16` = Integritätsprüfung

Transporte heute: 868-FSK (CC1101↔SX1262), ESP-NOW, UART. Es gibt **kein Quell-Geräte-ID-Feld** —
Antworten werden heute implizit über den Transport-Pfad zugeordnet. Das funktioniert nur, weil
aktuell überwiegend 1:1-Pfade existieren (ein Flipper, ein Hub).

## 2. Warum v2

Sobald mehrere Kontroll-Ebenen (Win-/Kali-GUI, Android-APK, Flipper, On-Device-Buttons) denselben
Hub gleichzeitig ansprechen können (Masterplan Phase 2b, Vollvermaschung K1–K6) und jedes Ergebnis
sowohl am Ursprungsgerät **als auch** zentral angezeigt werden soll (Kern-Baustein B im Masterplan),
reicht implizites Routing nicht mehr. v2 ergänzt:

1. **Source-Device-ID** im Anfrage-Frame — wer hat den Befehl ausgelöst.
2. **Ergebnis-Envelope** mit Origin-Feld — welches Gerät die Aktion tatsächlich ausgeführt hat.
3. **BLE als fünfter Transport** (bisher nur 868-FSK/ESP-NOW/UART).

## 3. Frame v2 (Vorschlag)

```
[LEN][0x47][0x02][COUNTER(4)][SRC_ID(1)][CMD][ALEN][ARGS][MAC4][CRC16(2)]
```

- Versions-Byte `0x01`→`0x02`: Parser kann altes/neues Format anhand dieses Bytes unterscheiden,
  ein v1-Gerät ohne Firmware-Update muss v2-Frames nur ignorieren können (kein Absturz), nicht
  verarbeiten.
- `SRC_ID(1)`: 1-Byte-Enum für die anfragende Kontroll-Ebene bzw. das anfragende Gerät. Vorschlag,
  final durch Firmware-Owner zu bestätigen: `0x01`=Flipper, `0x02`=Win-GUI, `0x03`=Kali-GUI,
  `0x04`=Android-APK, `0x05`=On-Device-V4, `0x06`=On-Device-V3 — Werte sollten sich an den `id`-
  Feldern aus `ecosystem/devices.json` orientieren, sobald dort auch Kontroll-Ebenen (GUI/APK)
  als Einträge geführt werden.
- Rest (`COUNTER`/`CMD`/`ALEN`/`ARGS`/`MAC4`/`CRC16`) unverändert zu v1. MAC- und CRC-Berechnung
  müssen `SRC_ID` mit einschließen, sonst wäre das neue Feld unauthentifiziert fälschbar.

## 4. Ergebnis-Envelope

Antwort-Frame nutzt denselben v2-Header. `ARGS` beginnt zusätzlich mit:

```
[ORIGIN_ID(1)][RESULT_CODE(1)][PAYLOAD...]
```

- `ORIGIN_ID`: welches Gerät die Aktion ausgeführt hat (kann vom `SRC_ID` der Anfrage abweichen,
  z. B. Flipper fragt an, Heltec V3 antwortet als ausführendes Gerät).
- `RESULT_CODE`: `0x00` OK, `0x01` Fehler, `0x02` Timeout, `0x03` Nicht unterstützt — Feinschliff
  (weitere Codes, Fehlertext-Payload) mit Firmware-Owner lokal festzulegen.

## 5. BLE-Transport-Mapping

- GATT-Service-UUID: **Vorschlag** (zufällig erzeugte 128-Bit-UUID, noch nicht lokal bestätigt):
  `f47ac9c1-be9d-4c1a-8709-3d84c074e600` — eigene UUID statt Nordic-UART-Service, um Verwechslung
  mit fremden BLE-UART-Tools zu vermeiden.
- Zwei Characteristics (Vorschlag, gleiche Herkunft/Status wie Service-UUID):
  - `CMD_WRITE` (Write, Kontroll-Ebene→Gerät): `f47ac9c1-be9d-4c1a-8709-3d84c074e601`
  - `RESP_NOTIFY` (Notify, Gerät→Kontroll-Ebene): `f47ac9c1-be9d-4c1a-8709-3d84c074e602`
  - Herleitung: Basis-UUID `f47ac9c1-be9d-4c1a-8709-3d84c074e6XX` mit `XX` = `00` Service /
    `01` CMD_WRITE / `02` RESP_NOTIFY — leicht erweiterbar (`03`+) falls P0.4e eine dritte
    Characteristic ergibt (z. B. getrennte Fragment-Control).
- MTU: Ohne Negotiation liegt die ATT-MTU bei 23 B (20 B nutzbar). v2-Frames sind mit
  Header+`MAC4`+`CRC16` (≥12 B Overhead) fast immer größer. Fragmentierung nötig, wenn keine
  Negotiation möglich ist:
  ```
  [FRAG_IDX(1)][FRAG_TOTAL(1)][CHUNK...]
  ```
  Reassemblierung erst nach dem letzten Fragment, Frame-Validierung (MAC/CRC) erst auf dem
  vollständig zusammengesetzten Frame.
- NimBLE (V4/V3, ESP32-S3) unterstützt MTU-Negotiation bis 517 B. Wenn der Peer (Android/Win)
  mitzieht, kann Fragmentierung übersprungen werden — negotiated MTU prüfen, Fallback auf
  Fragmentierung wenn `MTU < Frame-Länge`.

## 6. Neue Commands (Bedarf aus Phase 2b / Phase 8)

| Code | Befehl | Args | Phase | Notiz |
|---|---|---|---|---|
| `0x60` | BleConnect | target_mac | K3/K4 | Flipper/V4 als BLE-Peripheral verbindbar machen |
| `0x61` | BleDisconnect | – | K3/K4 | |
| `0x70` | RouteResult | origin_id, dest_mask | Envelope | Ergebnis zusätzlich an weitere Kontroll-Ebene(n) spiegeln |

Gegen die Befehls-Matrix in `FLEET-COMMANDS.md` geprüft: Bereich `0x60`–`0x7F` ist in v1 nicht
belegt (v1 nutzt `0x01`–`0x51`), keine Kollision.

## 7. Offen — Entscheidung lokal durch Firmware-Owner

- `SRC_ID`/`ORIGIN_ID`-Enum final festlegen (Vorschlag steht, siehe Abschnitt 3 — lokal nur noch
  zu bestätigen oder abzuändern, keine Neuerfindung nötig).
- BLE-GATT-Service-/Characteristic-UUIDs final vergeben (Vorschlag steht, siehe Abschnitt 5 —
  ebenfalls nur noch zu bestätigen/abzuändern; **vor dem ersten Firmware-Patch (P0.4f) fixieren**,
  da die UUID sonst nach dem ersten Flash auf einem Gerät erneut geändert werden müsste).
- Rückwärtskompatibilität: Versionsbyte-Check + gestaffelter Firmware-Rollout
  (Vorschlag-Reihenfolge V4 → V3 → LilyGo → WROOM) vs. harter Cutover — Entscheidung offen.
- Muss lokal gegen reale Timing-/Payload-Grenzen durchgerechnet werden (LoRa-Airtime bei SF9,
  ESP-NOW-Payload-Limit ~250 B) — dieses Dokument beschreibt nur die Protokoll-Form, keine
  Kapazitätsprüfung.
- Kein Code hier — Umsetzung ist ein Firmware-Patch in den jeweiligen Geräte-Repos
  (`heltec-v4-hub`, `heltec-ukfe-rx`, `lilygo-ukfe-rx`, `g4meover-wifi-relay`, Flipper-App), lokal
  zu bauen/flashen/testen. Empfohlene Reihenfolge für den ersten Patch (P0.4f): nur `SRC_ID`-Feld
  + Versionsbyte-Umstellung auf `heltec-v4-hub` (kleinste, isolierteste Änderung), BLE-Transport
  selbst (Service/Characteristics aus Abschnitt 5) erst in einem eigenen zweiten Patch, sobald
  P8.1 ansteht — vermeidet, UUID-Entscheidung und Frame-Formatänderung im selben Test zu verwechseln.

Quelle: `FLEET-COMMANDS.md` (v1-Frame, Befehls-Matrix), `G4MEOVER-ECOSYSTEM-MASTERPLAN.md`
(Phase 0/2b/8, Kern-Baustein B).
