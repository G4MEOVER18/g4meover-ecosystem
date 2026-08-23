# ProtoPirate — Chrysler-V0 `Key_2` (2-Byte-Hex) geht beim Speichern verloren

## Was
`helpers/protopirate_storage.c`, `protopirate_storage_write_capture_data()`, kopiert jedes
gecapturte Feld aus dem Live-`FlipperFormat` des Decoders in die zu speichernde Datei — sowohl
für den Verlauf (`protopirate_history_add_to_history()` → `protopirate_storage_save_history_capture()`)
als auch für die manuelle "Save"-Funktion (`protopirate_storage_save_capture()`). Für das Feld
`"Key_2"` läuft das über `protopirate_storage_copy_key_2()`:

```c
static bool protopirate_storage_copy_key_2(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format,
    FuriString* value) {
    bool copied = false;
    if(!protopirate_storage_copy_hex_fixed(save_file, flipper_format, "Key_2", 8, &copied)) {
        return false;
    }
    if(copied) {
        return true;
    }
    return protopirate_storage_copy_string_optional(save_file, flipper_format, "Key_2", value) &&
           protopirate_storage_copy_u32_optional(save_file, flipper_format, "Key_2");
}
```

Das Feld `"Key_2"` wird bei mir in vier verschiedenen Formen geschrieben, je nach Protokoll:

| Protokoll | Datei/Zeile | Form |
|---|---|---|
| `ford_v1.c:781` | `flipper_format_insert_or_update_hex(..., "Key_2", key2_bytes, 8)` | Hex, 8 Byte |
| `psa.c:1301` | `flipper_format_write_string_cstr(..., "Key_2", key2_str)` | String |
| `kia_v6.c:711` | `flipper_format_write_uint32(..., "Key_2", &key2_low, 1)` | u32 |
| `chrysler_v0.c:472/652` | `flipper_format_write_hex(..., "Key_2", (const uint8_t*)&key2, 2)` | Hex, **2 Byte** |
| `honda_v1.c` (`HONDA_V1_KEY_2_FIELD`) | `flipper_format_write_uint32(...)` | u32 |

`protopirate_storage_copy_key_2()` probiert genau drei Formen durch: Hex/8, String, u32. Der
vierte real vorkommende Fall — Chrysler V0s 2-Byte-Hex — fällt durch alle drei Versuche durch:
`protopirate_storage_copy_hex_fixed(..., 8, ...)` verlangt exakt 8 Byte und liefert bei einem
2-Byte-Feld `copied = false` (kein Fehler, `flipper_format_read_hex` lehnt die Längen-Diskrepanz
einfach ab); der String- und der u32-Versuch scheitern ebenso, weil das Feld weder als String
noch als u32 im Format steht. Alle drei Hilfsfunktionen behandeln "Feld nicht in diesem Format
vorhanden" als regulären, fehlerfreien Fall (`return true`) — genau das richtige Verhalten für
ein optionales Feld, hier aber die Ursache dafür, dass ein tatsächlich vorhandenes Feld in der
falschen Form komplett unbemerkt verschwindet. `protopirate_storage_write_capture_data()` läuft
danach ohne Fehler durch, die gespeicherte Datei enthält kein `"Key_2"` mehr.

Das trifft direkt den Ladepfad: `subghz_protocol_encoder_chrysler_v0_deserialize()`
(`protocols/chrysler_v0.c:370-373`) verlangt `"Key_2"` zwingend und bricht ohne dieses Feld
sofort mit `SubGhzProtocolStatusError` ab, bevor irgendein anderes Feld ausgewertet wird:

```c
uint16_t key2 = 0U;
if(!flipper_format_read_hex(flipper_format, "Key_2", (uint8_t*)&key2, 2)) {
    return SubGhzProtocolStatusError;
}
```

## Nutzen
- Jedes über mich gespeicherte Chrysler-V0-Capture (Verlaufseintrag oder manuelles Save) ist
  danach nicht mehr ladbar — weder zur Emulation/erneuten Übertragung noch zur späteren
  Detailansicht über den Encoder-Pfad. Das Capture selbst war beim Empfang korrekt, der
  Datenverlust passiert ausschließlich beim eigenen Speichern.
- Betrifft ausschließlich Chrysler V0 — die anderen drei real vorkommenden `"Key_2"`-Formen
  (Ford V1 Hex/8, PSA String, Kia V6/Honda V1 u32) werden bereits heute korrekt übernommen, wie
  der Host-Repro unten für alle vier Fälle nachweist.
- Passt in dieselbe Kategorie wie der bereits gelieferte `pp_parse_hex_u64_strict`-Fix: ein
  Feld-Parsing-Fall, der bei der Ersteinführung nicht an alle real vorkommenden Formate gedacht
  hat.

## Betroffene Dateien
- `helpers/protopirate_storage.c` — nur `protopirate_storage_copy_key_2()`.

Keine Änderung an `protopirate_storage_copy_hex_fixed()`, an der Aufrufreihenfolge in
`protopirate_storage_write_capture_data()` oder an `chrysler_v0.c` nötig — der Fix ergänzt
lediglich den fehlenden Hex/2-Versuch an der bereits vorhandenen Fallback-Kette.

## Patch
```c
static bool protopirate_storage_copy_key_2(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format,
    FuriString* value) {
    bool copied = false;
    if(!protopirate_storage_copy_hex_fixed(save_file, flipper_format, "Key_2", 8, &copied)) {
        return false;
    }
    if(copied) {
        return true;
    }
    if(!protopirate_storage_copy_hex_fixed(save_file, flipper_format, "Key_2", 2, &copied)) {
        return false;
    }
    if(copied) {
        return true;
    }
    return protopirate_storage_copy_string_optional(save_file, flipper_format, "Key_2", value) &&
           protopirate_storage_copy_u32_optional(save_file, flipper_format, "Key_2");
}
```

Einzige Änderung: ein zweiter `protopirate_storage_copy_hex_fixed(..., 2, ...)`-Versuch
zwischen dem bestehenden Hex/8- und dem String/u32-Zweig, nach demselben
`copied`-Kurzschluss-Muster, das die Funktion bereits für Hex/8 verwendet (und das an anderer
Stelle in derselben Datei, `protopirate_storage_copy_hex_or_u32()` für `"Key_3"`, schon für
genau diesen Zweck — mehrere mögliche Speicherformen desselben Schlüssels — im Einsatz ist).

## Testhinweis (ohne Hardware)
Reine Feld-Kopierlogik, host-nachvollziehbar ohne `FlipperFormat`/Storage-Abhängigkeit.
Reproduktionsprogramm in diesem Ordner (`repro-key2-hex2-lost-on-save/repro.c`) bildet
`protopirate_storage_copy_hex_fixed()`/`copy_string_optional()`/`copy_u32_optional()` sowie
`copy_key_2()` vor und nach dem Fix 1:1 nach, mit einem minimalen getypten Feld-Modell, das
genau die vier real vorkommenden Schreibformen von `"Key_2"` abbildet (Chrysler V0, Ford V1,
PSA, Kia V6):

```
gcc -O1 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro
```

Ergebnis: ALT verliert `Key_2` ausschließlich im Chrysler-V0-Fall (Hex/2), die anderen drei
Fälle sind bereits heute korrekt. NEU erhält alle vier Fälle, ohne die drei bereits
funktionierenden Fälle zu verändern.

Funktionaler Test danach lokal auf dem Gerät: ein Chrysler-V0-Signal empfangen, über den
Verlauf oder "Save" speichern, das Gerät neu starten (oder die Datei erneut aus dem Verlauf
laden) und erneut senden — vor dem Fix bricht das Laden mit einem Fehler ab
(`SubGhzProtocolStatusError`), danach funktioniert Laden/Senden wie bei jedem anderen
gespeicherten Capture.

## Risiko
- Sehr niedrig: reine Ergänzung eines zusätzlichen Fallback-Versuchs in einer bereits
  bestehenden, sequenziell kurzschließenden Versuchskette (`if(copied) return true;` nach jedem
  Versuch) — kein bestehender Pfad wird entfernt oder umsortiert.
- Für die drei bereits funktionierenden Formate (Hex/8, String, u32) identisches Verhalten
  vorher/nachher: der neue Hex/2-Versuch greift nur, wenn der vorherige Hex/8-Versuch nichts
  gefunden hat, und `flipper_format_read_hex(..., 2)` lehnt ein Feld, das tatsächlich als
  String oder u32 (nicht als Hex-Bytes) gespeichert ist, wegen der Werteanzahl-/Typ-Prüfung ab
  (im Host-Repro durch das getypte Feld-Modell nachgebildet) — kein Risiko einer Fehlinterpretation.
- Kein zusätzliches Sicherheits-/Missbrauchsrisiko.

## Quelle
- `G4MEOVER18/ProtoPirate` (main, `e3d9da5`): `helpers/protopirate_storage.c`
  (`protopirate_storage_copy_key_2`), `protocols/chrysler_v0.c`
  (`subghz_protocol_encoder_chrysler_v0_deserialize`, Zeilen 370-373 als Beleg für die
  zwingende `"Key_2"`-Anforderung beim Laden).
