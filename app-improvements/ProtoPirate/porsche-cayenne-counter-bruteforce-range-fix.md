# ProtoPirate — Porsche/Touareg-Decoder findet den Zähler nur für die ersten 256 Werte

## Was
Der Porsche/Touareg-Cayenne-Decoder (`protocols/porsche_touareg.c`) hat keinen direkten
Zugriff auf den 16-Bit-Rolling-Zähler im empfangenen Frame — er steckt verschlüsselt in den
Bytes `pkt[4..7]` und wird beim Empfang durch reines Brute-Force-Ausprobieren
zurückgewonnen:

```c
static void porsche_cayenne_parse_data(SubGhzProtocolDecoderPorscheCayenne* instance) {
    ...
    instance->generic.cnt = 0;
    ...
    for(uint16_t try_cnt = 1; try_cnt <= 256; try_cnt++) {
        porsche_cayenne_compute_frame(
            instance->generic.serial, instance->generic.btn,
            (uint16_t)(try_cnt - 1), frame_type, try_pkt);
        if(try_pkt[4] == pkt[4] && try_pkt[5] == pkt[5] &&
           try_pkt[6] == pkt[6] && try_pkt[7] == pkt[7]) {
            instance->generic.cnt = try_cnt;
            break;
        }
    }
}
```

`porsche_cayenne_compute_frame()` verschlüsselt den vollen 16-Bit-Zähler (`cnt_lo`+`cnt_hi`)
in `pkt[4..7]`. Die Sender-Seite (`subghz_protocol_encoder_porsche_cayenne_get_upload()`,
gleiche Datei) sendet exakt den vom Nutzer/der Datei vorgegebenen `instance->cnt`
(voller `uint16_t`-Bereich, 0–65535) — jeder reale Sender erhöht diesen Zähler bei jedem
Tastendruck um 1 und erreicht Werte über 256 nach spätestens 256 Bedienungen. Die
Such-Obergrenze im Decoder ist aber fest auf `256` begrenzt, deckt also nur `cnt` 1–256 ab.
Für jeden empfangenen Frame mit `cnt > 256` (jeder Sender, der öfter als 256-mal benutzt
wurde) findet keine der 256 Iterationen einen Treffer, die Schleife bricht ohne Match ab,
und `instance->generic.cnt` bleibt auf dem vor der Schleife gesetzten Initialwert `0` stehen
— ohne jede Fehlermeldung, der Rest des Frames (Serial, Button, CRC-lose Byte-Vergleich)
wird trotzdem als gültig gemeldet.

Mit einem eigenständigen Host-Repro (`porsche_cayenne_compute_frame()` 1:1 übernommen,
Sender-Frame für zwölf realistische Zählerstände erzeugt, danach ALT- gegen NEU-Suchbereich
verglichen) verifiziert: **7 von 12 Fällen** (`cnt` 257, 300, 1000, 5000, 30000, 65000,
65535) liefern unter ALT `Cnt:0000` statt des echten Werts; unter NEU (Suchbereich auf den
vollen `uint16_t`-Bereich erweitert) sind alle zwölf Fälle korrekt.

## Nutzen
- Der angezeigte Zähler (`Cnt:%04lX` in `subghz_protocol_decoder_porsche_cayenne_get_string()`)
  ist für praktisch jeden real benutzten Sender falsch (`0` statt des tatsächlichen Werts),
  sobald er öfter als 256-mal betätigt wurde — bei einem Autoschlüssel im Alltagseinsatz der
  Normalfall, nicht der Ausnahmefall.
- Betrifft direkt die Kernfunktion des Rolling-Code-Auslesens: der Zähler ist neben Serial und
  Button das einzige inhaltlich interessante Feld eines empfangenen Frames (für Diagnose,
  Zähler-Abstand-Analyse zwischen zwei Captures, spätere Emulation mit korrektem
  Folgezähler `cnt+1`). Mit `Cnt:0000` ist keine dieser Auswertungen möglich, ohne dass die
  App das als Fehlschlag kennzeichnet.
- Reine Bereichserweiterung einer bestehenden Suchschleife — keine neue Krypto-/Protokoll-
  Erkenntnis nötig, `porsche_cayenne_compute_frame()` selbst bleibt unverändert und ist bereits
  korrekt (das bestätigt der Host-Repro: alle Fälle bis 256 sind schon unter ALT richtig).

## Betroffene Dateien
- `protocols/porsche_touareg.c` — nur die Suchschleife in `porsche_cayenne_parse_data()`.

Keine Änderung an `porsche_cayenne_compute_frame()`, am Encoder
(`subghz_protocol_encoder_porsche_cayenne_get_upload()` sendet bereits den vollen Zähler
unverändert korrekt) oder an der Feed-/Sync-Zustandsmaschine nötig.

## Patch
```c
static void porsche_cayenne_parse_data(SubGhzProtocolDecoderPorscheCayenne* instance) {
    uint8_t pkt[8];
    uint64_t raw = instance->generic.data;

    for(int8_t i = 7; i >= 0; i--) {
        pkt[i] = (uint8_t)(raw & 0xFF);
        raw >>= 8;
    }

    instance->generic.serial = ((uint32_t)pkt[1] << 16) | ((uint32_t)pkt[2] << 8) | pkt[3];
    instance->generic.btn = (uint8_t)(pkt[0] >> 4);
    instance->generic.cnt = 0;

    uint8_t frame_type = pkt[0] & 0x07;
    uint8_t try_pkt[8];
    for(uint32_t try_cnt = 1; try_cnt <= 65535; try_cnt++) {
        porsche_cayenne_compute_frame(
            instance->generic.serial,
            instance->generic.btn,
            (uint16_t)(try_cnt - 1),
            frame_type,
            try_pkt);
        if(try_pkt[4] == pkt[4] && try_pkt[5] == pkt[5] && try_pkt[6] == pkt[6] &&
           try_pkt[7] == pkt[7]) {
            instance->generic.cnt = (uint16_t)try_cnt;
            break;
        }
    }
}
```

Zwei Änderungen gegenüber dem Original: die Schleifenobergrenze `256` → `65535` (voller
Wertebereich des `uint16_t`-Zählerfelds) sowie die Schleifenvariable `uint16_t try_cnt` →
`uint32_t try_cnt`, da ein `uint16_t`, das bis `65535` hochgezählt wird, beim anschließenden
`try_cnt++` auf `0` überläuft und die Abbruchbedingung `try_cnt <= 65535` dauerhaft erfüllt
bliebe (Endlosschleife) — mit `uint32_t` als Schleifentyp tritt dieser Überlauf nicht ein,
der `(uint16_t)`-Cast beim Aufruf/bei der Zuweisung an `instance->generic.cnt` bleibt
unverändert nötig und korrekt.

## Testhinweis (ohne Hardware)
Reine Bit-/Rotationslogik, host-nachvollziehbar ohne Flipper-/`SubGhz`-Abhängigkeiten.
Reproduktionsprogramm in diesem Ordner
(`repro-porsche-cayenne-counter-bruteforce-range-fix/repro.c`) übernimmt
`porsche_cayenne_compute_frame()` unverändert, erzeugt für zwölf realistische Zählerstände
(1 bis 65535, gezielt beidseits der alten 256er-Grenze) je einen Sender-Frame und vergleicht
die ALT- gegen die NEU-Suchschleife:

```
gcc -O2 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro
```

Ergebnis: ALT liefert für 7 von 12 Fällen (`cnt` > 256) `Cnt:0000` statt des echten Werts,
NEU liefert in allen zwölf Fällen den korrekten Zähler; die fünf Fälle bis 256 sind unter
ALT bereits korrekt und bleiben unter NEU identisch (Regression ausgeschlossen).

Funktionaler Test danach lokal auf dem Gerät: ein eigenes, im Alltag benutztes
Porsche/Touareg-Signal (Zählerstand vermutlich deutlich über 256) empfangen — vor dem Fix
zeigt die Detailansicht `Cnt:0000`, nach dem Fix den tatsächlichen Zählerstand. Zusätzlich
die Such-Laufzeit im ungünstigsten Fall (kein Treffer über den gesamten Bereich, z. B. bei
einem Fremdsignal) auf echter Hardware messen — der Host-Repro braucht für den vollen
65535er-Durchlauf rund 17 ms auf x86_64/`-O2`; auf dem STM32WB55 (64 MHz, deutlich
einfacherer Kern) ist ein spürbar höherer Wert zu erwarten, sollte aber für einen
einmaligen Decode-Vorgang pro empfangenem Frame (kein Hot-Path) unkritisch sein.

## Risiko
- Niedrig, aber nicht null: die Suchschleife läuft im ungünstigsten Fall jetzt bis zu 256-mal
  länger als zuvor (kein Treffer über den vollen Bereich statt über nur 256 Versuche) — bei
  jedem empfangenen Frame, dessen erste vier Byte zufällig durch die 64-Bit-Präambel-/
  Sync-Erkennung rutschen, ohne ein echtes Porsche/Touareg-Signal zu sein. Reine
  Laufzeit-, keine Speicher-/Zustandsauswirkung; `porsche_cayenne_compute_frame()` selbst
  bleibt unverändert und damit hardware-seitig bereits erprobt.
- Falls sich die gemessene Worst-Case-Laufzeit auf dem Gerät als spürbar erweist (z. B.
  hörbare Verzögerung der Anzeige nach dem Empfang, oder verpasste Folge-Frames während
  eines Bursts): mögliche spätere Optimierung, hier bewusst nicht umgesetzt, um den Patch auf
  eine Bereichskorrektur zu beschränken — die interne Rotation (`r_h`/`r_m`/`r_l`) hängt nur
  von `cnt_lo` (0–255) ab und wiederholt sich für jeden `cnt_hi`-Wert identisch; eine
  zweistufige Suche (äußere Schleife über `cnt_lo` mit einmaliger Rotation, innere Schleife
  über `cnt_hi` ohne erneute Rotation) könnte die Zahl der teuren Rotationsschritte um etwa
  den Faktor 250 senken, ohne das Suchergebnis zu ändern.
- Kein zusätzliches Sicherheits-/Missbrauchsrisiko — der Zähler wird nur gelesen/angezeigt,
  nicht in eine Sendefunktion zurückgeschrieben; die Encoder-Seite ist von diesem Fix nicht
  betroffen.

## Quelle
- `G4MEOVER18/ProtoPirate` (main, `e3d9da5`): `protocols/porsche_touareg.c`
  (`porsche_cayenne_parse_data()`, `porsche_cayenne_compute_frame()`,
  `subghz_protocol_encoder_porsche_cayenne_get_upload()` als Gegenstück auf der Sender-Seite).
