# g4meover-magspoof — Track-2/3-Start-Sentinel-Fix in `mag_device_parse_card_string`

## Was
Meine MagSpoof-App basiert auf `zacharyweiss/magspoof_flipper` (den ich über den
`astro-cyberpaws`-Fork verfolge). Die Funktion `mag_device_parse_card_string()` in
`mag_device.c` zerlegt einen eingegebenen Card-String (`%TRACK1?;TRACK2?;TRACK3?`) in die
drei `MagTrack`-Felder und schreibt sie mit Start-/End-Sentinel zurück:

```c
furi_string_printf(mag_dev->dev_data.track[0].str, "%%%.*s?", track1_len, track1_start);
furi_string_printf(mag_dev->dev_data.track[1].str, "%%%.*s?", track2_len, track2_start);
furi_string_printf(mag_dev->dev_data.track[2].str, "%%%.*s?", track3_len, track3_start);
```

Das Format `"%%%.*s?"` erzeugt für **alle drei Tracks** ein führendes `%`-Zeichen. Nach
ISO 7811 ist `%` aber nur der Start-Sentinel von Track 1 (7-Bit/Alphanumerisch). Track 2 und
Track 3 (5-Bit/Numerisch) verwenden `;` als Start-Sentinel.

`mag_encode()` (in `helpers/mag_helpers.c`) kodiert diese gespeicherten Strings direkt für die
Emulation, Zeichen für Zeichen inklusive Sentinel, mit track-spezifischem Bit-Alphabet:
`bitlen[] = {7, 5, 5}`, `sublen[] = {32, 48, 48}` (Track 1: Offset 32/Leerzeichen=0,
Track 2+3: Offset 48/„0"=0). Das führende `%` (0x25) landet damit bei Track 2/3 mit
Offset 48 als Rohwert `0x25 − 0x30 = −11`, statt des korrekten `;` (0x3B) mit Rohwert
`0x3B − 0x30 = 11`.

Ich habe das mit einer eigenständigen, hostseitigen C-Reproduktion der Bit-Extraktion aus
`mag_encode()` (4 Datenbits LSB-zuerst + 1 Odd-Parity-Bit) nachgerechnet:

| Sentinel-Zeichen | Rohwert (Offset 48) | Datenbits (LSB zuerst) | Parity |
|---|---|---|---|
| `;` (korrekt, Track 2/3) | 11 | `1101` | 0 |
| `%` (aktueller Bug, Track 2/3) | −11 | `1010` | 1 |

Der auf den Draht gehende Bit-/Manchester-Code für den Start-Sentinel von Track 2 und Track 3
ist damit komplett falsch (andere Datenbits, andere Parität) — jeder ISO-7811-konforme Leser
verliert den Frame-Sync und kann den Track nicht decodieren. Das erklärt vermutlich, warum
Track-2/3-Emulation bei mir unzuverlässig bzw. gar nicht funktioniert, obwohl Track 1
(dort ist `%` korrekt) sauber läuft.

## Nutzen
- Behebt einen konkreten, reproduzierbaren Encode-Bug für Track 2 **und** Track 3 — bislang
  wurde nur Track 1 korrekt auf die Karte/den Reader gebracht.
- Rein string-/bit-logische Änderung, vollständig ohne Hardware nachrechenbar (siehe Testhinweis).
- Schließt direkt an den offenen Backlog-Punkt „Card-String→MagDevice-Parser +
  Track-3-Encode" an: der Parser existiert bereits (identisch in `zacharyweiss/magspoof_flipper`
  und im `astro-cyberpaws`-Fork), das fehlende Stück war die korrekte Track-3(/2)-Sentinel-Kodierung.

## Betroffene Dateien (lokal, in meiner MagSpoof-App)
- `mag_device.c` — `mag_device_parse_card_string()`: die drei `furi_string_printf(...)`-Aufrufe
  für Track 1/2/3.

Keine Änderung an `mag_device.h`, `mag_helpers.c`/`mag_encode()` oder am `.mag`-Dateiformat
nötig — der Fix bleibt vollständig innerhalb der Parser-Funktion.

## Patch
```c
bool mag_device_parse_card_string(MagDevice* mag_dev, FuriString* f_card_str) {
    furi_assert(mag_dev);
    FURI_LOG_D(TAG, "mag_device_parse_card_string");

    const char* card_str = furi_string_get_cstr(f_card_str);

    FURI_LOG_D(TAG, "Parsing card string: %s", card_str);

    // Track 1 (ISO 7811-2, Start-Sentinel '%')
    const char* track1_start = strchr(card_str, '%');
    if(!track1_start) {
        FURI_LOG_D(TAG, "Could not find track 1 start");
        return false;
    }
    track1_start++;
    const char* track1_end = strchr(track1_start, '?');
    if(!track1_end) {
        FURI_LOG_D(TAG, "Could not find track 1 end");
        return false;
    }
    size_t track1_len = track1_end - track1_start;

    FURI_LOG_D(TAG, "Track 1: %.*s", track1_len, track1_start);

    mag_dev->dev_data.track[0].len = track1_len;
    furi_string_printf(mag_dev->dev_data.track[0].str, "%%%.*s?", track1_len, track1_start);

    // Track 2 (ISO 7811-3, Start-Sentinel ';' — NICHT '%')
    const char* track2_start = strchr(track1_end, ';');
    if(!track2_start) {
        FURI_LOG_D(TAG, "Could not find track 2 start");
        return true;
    }

    track2_start++;
    const char* track2_end = strchr(track2_start, '?');
    if(!track2_end) {
        FURI_LOG_D(TAG, "Could not find track 2 end");
        return true;
    }
    size_t track2_len = track2_end - track2_start;

    FURI_LOG_D(TAG, "Track 2: %.*s", track2_len, track2_start);

    mag_dev->dev_data.track[1].len = track2_len;
    furi_string_printf(mag_dev->dev_data.track[1].str, ";%.*s?", track2_len, track2_start);

    // Track 3 (ISO 7811-3, Start-Sentinel ';' — NICHT '%')
    const char* track3_start = strchr(track2_end, ';');
    if(!track3_start) {
        FURI_LOG_D(TAG, "Could not find track 3 start");
        return true;
    }

    track3_start++;
    const char* track3_end = strchr(track3_start, '?');
    if(!track3_end) {
        FURI_LOG_D(TAG, "Could not find track 3 end");
        return true;
    }
    size_t track3_len = track3_end - track3_start;

    FURI_LOG_D(TAG, "Track 3: %.*s", track3_len, track3_start);

    mag_dev->dev_data.track[2].len = track3_len;
    furi_string_printf(mag_dev->dev_data.track[2].str, ";%.*s?", track3_len, track3_start);

    return true;
}
```

Einzige Änderung gegenüber der Vorlage: die beiden `furi_string_printf`-Aufrufe für Track 2
und Track 3 verwenden jetzt das Formatliteral `";%.*s?"` (führendes `;`) statt `"%%%.*s?"`
(führendes `%`). Track 1 bleibt unverändert bei `"%%%.*s?"` (führendes `%`), das ist dort
korrekt.

## Testhinweis (ohne Hardware)
Die eigentliche Ursache ist reine Bit-Arithmetik in `mag_encode()`, unabhängig von Furi/Flipper-
Typen nachvollziehbar. Reproduktionsprogramm (host-kompilierbar, `gcc sentinel_check.c -o
sentinel_check && ./sentinel_check`):

```c
#include <stdio.h>

// Reproduziert die Bit-Extraktion aus helpers/mag_helpers.c::mag_encode()
// (4 Datenbits LSB-zuerst + 1 Odd-Parity-Bit fuer Track 2/3, track_bits=5)
void bits_for_char(char c, int track_bits, int offset) {
    int tmp = c - offset;
    int crc = 1;
    printf("char '%c' (0x%02X), offset %d -> tmp=%d : databits(LSB first)=", c, (unsigned char)c, offset, tmp);
    for(int j = 0; j < track_bits - 1; j++) {
        crc ^= tmp & 1;
        printf("%d", tmp & 1);
        tmp >>= 1;
    }
    printf(" parity=%d\n", crc);
}

int main() {
    printf("-- Track 2/3 Start-Sentinel, korrekt (';', Offset 48) --\n");
    bits_for_char(';', 5, 48);   // erwartet: 1101 parity=0
    printf("-- Track 2/3 Start-Sentinel, Bug ('%%', Offset 48) --\n");
    bits_for_char('%', 5, 48);   // aktuell:  1010 parity=1  (falsch)
    return 0;
}
```

Erwartete Ausgabe zeigt zwei unterschiedliche Bitmuster für `;` vs. `%` bei Offset 48 — das
belegt den Encode-Fehler unabhängig vom eigentlichen Gerät.

Funktionaler Test nach dem Patch, lokal auf dem Flipper (Debug-Log, kein Sender/Reader nötig):
1. `ufbt` bauen, App starten, `debug` Loglevel aktivieren.
2. Card-String mit Track 2 und Track 3 eingeben, z. B.
   `%B1234567890123445^SURNAME/GIVEN^25121010000000000?;1234567890123445=25121010000000000?;000000000000000000000?`
3. Im Debug-Log prüfen: `Track 2: ...` / `Track 3: ...` zeigen die reinen Nutzdaten (wie vorher),
   aber `dev_data.track[1].str` / `track[2].str` beginnen jetzt mit `;`, nicht mit `%`
   (z. B. per zusätzlichem `FURI_LOG_D` nach dem `furi_string_printf` oder im Anzeige-Widget
   der Emulate-Szene sichtbar, die `dev_data.track[i].str` direkt anzeigt).
4. Erst danach den eigentlichen Hardware-Funktionstest (Reader/Empfänger) durchführen — das ist
   der Teil, der bei mir lokal passiert.

## Risiko
- Sehr niedrig: Änderung betrifft ausschließlich zwei Formatliterale in einer reinen
  String-Aufbau-Funktion, keine Struct-/API-Änderung, kein Einfluss auf Track 1 oder auf das
  `.mag`-Speicherformat (dort werden die Tracks unverändert als benannte Strings unter
  `Track 1`/`Track 2`/`Track 3` weggeschrieben).
- Bereits gespeicherte `.mag`-Dateien mit fehlerhaftem Track-2/3-Präfix aus einer Version vor
  diesem Fix müssten bei mir lokal neu aus dem Card-String erzeugt (oder das führende Zeichen
  manuell von `%` auf `;` korrigiert) werden, sonst bleibt der alte Bug in bereits gespeicherten
  Dateien bestehen.
- Kein zusätzliches Sicherheits-/Missbrauchsrisiko — der Fix macht die Emulation lediglich
  standardkonform, eröffnet keine neue Funktion.

## Quelle
- `zacharyweiss/magspoof_flipper` (main): `mag_device.c` (`mag_device_parse_card_string`),
  `helpers/mag_helpers.c` (`mag_encode`, `bitlen[]`, `sublen[]`), `scenes/mag_scene_emulate.c`.
- `astro-cyberpaws/magspoof_flipper` (Fork, main): `mag_device.c` — identische
  `mag_device_parse_card_string`-Implementierung, bestätigt den Bug auch dort.
- ISO 7811-2/-3 Sentinel-/Alphabet-Definitionen (Track 1: `%`/`?`, 7-Bit-Alphanumerisch;
  Track 2/3: `;`/`?`, 5-Bit-Numerisch) — allgemein dokumentierter Magnetstreifen-Standard.
