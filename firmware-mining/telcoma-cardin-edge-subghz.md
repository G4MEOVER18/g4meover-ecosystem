# Telcoma/Cardin EDGE — SubGHz-Protokoll-Port (aus Unleashed unlshd-090)

## Was
Unleashed hat in unlshd-090 (PR #1001, @half2me) einen neuen SubGHz-Decoder/-Encoder für
Telcoma/Cardin-EDGE-Handsender (Torantriebe) ergänzt (`telcoma_edge.c/.h`). Fixed-Code-Protokoll
(kein Rolling Code), 433.92 MHz, AM/OOK mit Manchester-Kodierung.

Ich habe gegen meine Basis mntm-012 geprüft (`lib/subghz/protocols/protocol_items.c`): Weder
"telcoma" noch "cardin" noch "edge" ist dort registriert — das Protokoll fehlt mir komplett.
Handsender dieser Familie kann ich damit aktuell nur als unklassifiziertes RAW sehen, nicht
gezielt decodieren.

Technische Eckdaten (aus der Unleashed-Quelle):
- Frequenz 433.92 MHz, Modulation AM650 (OOK), Manchester-codiert.
- Timing: Halbbit (TE) ≈ 1270 µs, Vollbit ≈ 2540 µs, Toleranz ±520 µs (bewusst breit für Jitter).
- Rahmen: 0xFF-Präambel + 21-Bit Seriennummer + 3-Bit One-Hot-Kanalauswahl (Tor = 0,
  Kanal1 = 0x1, Kanal2 = 0x2, Kanal4 = 0x4).
- Gate-Kanal sendet 32 Manchester-Bits (+ Stop-Halbbit), andere Kanäle 33 Bit — wird auf einen
  kanonischen 32-Bit-Key normalisiert (`payload & 0xFFFFFF`, Serial = `(payload >> 3) & 0x1FFFFF`).
- Fixed Code — kein Schlüsselzähler, kein Rolling-Code-Handling nötig.

## Nutzen
- Erweitert meine SubGHz-Analyse (RollForge/ProtoPirate) um eine real verbreitete
  Torantriebs-Familie (Cardin/Telcoma), die ich sonst nur als unklassifiziertes RAW sehe.
- Fixed-Code + öffentlich dokumentierte Bitstruktur → sauber deterministisch decodier-/
  encodierbar, kein Bedarf an Schlüssel-Wiederherstellung wie bei Rolling-Code-Systemen.
- Kleine, isolierte Änderung (ein neues Protokollpaar + eine Registry-Zeile), die sich am
  Muster bestehender Fixed-Code-Protokolle in meiner Basis orientiert (`clemsa.c` als
  strukturell nächstes Vorbild: gleiche Decoder/Encoder-Aufteilung, State-Machine
  reset/decode/save/load).

## Betroffene Dateien (lokal)
Neu:
- `lib/subghz/protocols/telcoma_edge.c`
- `lib/subghz/protocols/telcoma_edge.h`

Änderung (je 1–2 Zeilen):
- `lib/subghz/protocols/protocol_items.h` — `extern const SubGhzProtocol subghz_protocol_telcoma_edge;` ergänzen.
- `lib/subghz/protocols/protocol_items.c` — `#include "telcoma_edge.h"` + Eintrag
  `&subghz_protocol_telcoma_edge,` im Registry-Array `subghz_protocol_registry_items[]` ergänzen.

Optional (nur für manuelles Erzeugen im Menü, nicht zwingend fürs Decodieren/Replay):
- `applications/main/subghz/scenes/subghz_scene_set_type.c` — Menüeintrag "Telcoma/Cardin EDGE".
- passendes Typ-Enum in `subghz_i.h`.
- `documentation/SubGHzSupportedSystems.md` — Protokoll in meiner eigenen Liste nachtragen.

Ich verzichte bewusst auf die Test-RAW-Dateien aus Unleashed (`telcoma_edge_raw.sub`,
`telcoma_edge_ch_raw.sub`) — CONSTRAINTS verbieten mir generell das Pushen von `.sub`-Dateien in
dieses Repo, unabhängig davon ob es Testdaten oder echte Captures sind. Wer die Referenz-RAWs
zum lokalen Funktionstest braucht, holt sie sich direkt aus dem Unleashed-Repo.

## Risiko
- Niedrig: additive Änderung (neue Dateien + eine Registry-Zeile), berührt keinen bestehenden
  Protokoll-Handler und keinen gemeinsamen State.
- Einzige Vorsicht: mntm-012 kann im `SubGhzProtocol`-Struct bzw. bei
  `SubGhzProtocolDecoderBase`/`EncoderBase` von Stock/Unleashed abweichen (Momentum pflegt
  eigene Erweiterungen, z. B. bei Presets/Flags). Vor dem Übernehmen `telcoma_edge.c/.h` gegen
  `clemsa.c/.h` in meiner Basis (funktional nächstliegendes Fixed-Code-Vorbild) auf
  Struct-Feldnamen/Funktionssignaturen abgleichen — kein Blind-Copy.
- Der Build (`fbt`/`ufbt`) deckt Signaturabweichungen als Compile-Fehler zuverlässig auf.
- Funktionale Prüfung nur mit echtem Cardin/Telcoma-Sender oder der Unleashed-Referenz-RAW
  möglich (beides lokal, nicht Teil dieses Repos).
- Kein zusätzliches Sicherheits-/Missbrauchsrisiko über das hinaus, was jeder SubGHz-Fixed-Code-
  Handler ohnehin hat (autorisierte Tests an eigenen bzw. vertraglich freigegebenen Toranlagen).

## Port-Skizze
1. `telcoma_edge.c` + `telcoma_edge.h` aus Unleashed (`dev`-Branch, PR #1001, @half2me) in
   `lib/subghz/protocols/` übernehmen.
2. Gegen `clemsa.c/.h` in meiner Basis gegenlesen: Decoder-Reset/State-Machine-Namen,
   `SubGhzProtocolDecoder`/`EncoderBase`-Feldnamen, `SubGhzRadioPreset`-Handling — Signaturen an
   meine Basis anpassen, falls sie abweichen.
3. `protocol_items.h`: `extern const SubGhzProtocol subghz_protocol_telcoma_edge;` ergänzen.
4. `protocol_items.c`: `#include "telcoma_edge.h"` + `&subghz_protocol_telcoma_edge,` ans Ende
   von `subghz_protocol_registry_items[]` anhängen.
5. Build: `ufbt` (bzw. `fbt firmware_all`) — Compile-Fehler sind der erste Gegencheck für
   Struct-Abweichungen aus Schritt 2.
6. Funktionstest lokal: Empfang eines echten Cardin/Telcoma-EDGE-Handsenders oder Wiedergabe der
   Unleashed-Referenz-RAW → prüfen, ob der Decoder anspringt und Seriennummer/Kanal korrekt
   anzeigt.
7. Optionalen UI-Menüeintrag (manuelles Erzeugen) erst nach erfolgreichem Decode-Test ergänzen.

## Quelle
- Unleashed-Firmware, Release unlshd-090, PR #1001 (@half2me): `telcoma_edge.c/.h`,
  `documentation/SubGHzSupportedSystems.md`.
- Momentum-Firmware, Tag `mntm-012`: `lib/subghz/protocols/protocol_items.c` (Registry-Abgleich —
  Protokoll fehlt), `clemsa.c/.h` (Referenzstruktur für den Abgleich).
