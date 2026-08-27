# ProtoPirate — RAW-Decode: fester 512er-Chunk ignoriert echte `RAW_Data:`-Zeilengrenzen

## Was
`helpers/raw_file_reader.c` liest eine geladene `.sub`-RAW-Datei für die "Sub Decode"-Funktion
(Versuch, ein bekanntes Protokoll in einer rohen Aufnahme zu erkennen) blockweise über
`raw_file_reader_load_chunk()`:

```c
static bool raw_file_reader_load_chunk(RawFileReader* reader) {
    if(reader->file_finished) return false;

    size_t to_read = (reader->count < RAW_READER_BUFFER_SIZE) ? reader->count :
                                                                RAW_READER_BUFFER_SIZE;

    if(!flipper_format_read_int32(reader->ff, "RAW_Data", reader->buffer, to_read)) {
        reader->file_finished = true;
        return false;
    }

    reader->buffer_count = to_read;
    reader->buffer_index = 0;
    reader->count -= to_read;

    return true;
}
```

`reader->count` ist die beim Öffnen vorab über alle `RAW_Data:`-Vorkommen aufsummierte
Gesamtzahl der Werte in der Datei; `RAW_READER_BUFFER_SIZE` ist `512`. Jeder Aufruf verlangt also
pauschal bis zu 512 Werte, ohne zu wissen, wie viele Werte die *aktuelle* `RAW_Data:`-Zeile
tatsächlich enthält.

`flipper_format_read_int32()` (`flipper_format_stream_read_value_line()` in
`lib/flipper_format/flipper_format_stream.c` der Firmware) sucht bei **jedem** Aufruf zunächst
das nächste `RAW_Data`-Vorkommen ab der aktuellen Stream-Position und verlangt dann **exakt**
`data_size` Werte aus **dieser einen Zeile** — reicht die Zeile nicht (Zeilenende erreicht, bevor
`data_size` Werte gelesen wurden), scheitert der **gesamte** Aufruf, ohne die bereits gelesenen
Werte zurückzugeben:

```c
for(size_t i = 0; i < data_size; i++) {
    bool last = false;
    result = flipper_format_stream_read_value(stream, value, &last);
    ...
    if(last && ((i + 1) != data_size)) {
        result = false;   // Zeile zu Ende, aber data_size noch nicht erreicht -> kompletter Fehlschlag
        break;
    }
}
```

`raw_file_reader_load_chunk()` behandelt diesen Fehlschlag als Dateiende (`file_finished = true`).
Laut Format-Doku (`documentation/file_formats/SubGhzFileFormats.md`, Abschnitt `RAW_Data`) sind
`RAW_Data:`-Zeilen auf **höchstens 512 Werte** begrenzt, aber mit **beliebiger** Aufteilung
darunter — nur die Obergrenze ist vorgeschrieben, nicht ein fester Wert. Dass
`RAW_READER_BUFFER_SIZE == 512` ist, verdeckt den Bug nur für Aufnahmen, die exakt dem
Puffer-Flush der Stock-Firmware entsprechen: `lib/subghz/protocols/raw.c` definiert
`SUBGHZ_DOWNLOAD_MAX_SIZE = 512` und schreibt während einer laufenden "Read RAW"-Aufnahme
tatsächlich immer dann eine neue `RAW_Data:`-Zeile, wenn der interne Sammelpuffer exakt 512
Werte erreicht (`subghz_protocol_decoder_raw_feed()`), sodass jede Zeile außer der letzten
zufällig genau 512 Werte hat.

Jede andere, ebenso spec-konforme Zeilenaufteilung — eine handgebaute Testdatei mit kleineren
Zeilen, eine Aufnahme eines anderen RAW-fähigen Tools/Forks mit eigener interner Puffergröße,
oder schlicht eine von Hand editierte/gekürzte `.sub`-Datei — bricht die Annahme: Sobald eine
Zeile vor Erreichen von 512 Werten endet, schlägt bereits der erste `load_chunk()`-Aufruf fehl,
`reader->count` (das eigentlich noch alle Werte anzeigt) wird gar nicht erst dekrementiert, und
`raw_file_reader_get_next()` liefert sofort `false`. `raw_file_reader_is_finished()` meldet
daraufhin "fertig" (0 gelesene Samples), und die aufrufende Szene
(`scenes/protopirate_scene_sub_decode.c`, `DecodeStateDecodingRaw`) zeigt

```
RAW Signal
Freq: ...
No ProtoPirate protocol
detected in signal.
```

— obwohl die Datei nie tatsächlich in den Decoder (`subghz_receiver_decode()`) eingespeist wurde.
Der Nutzer bekommt keinen Hinweis darauf, dass die Datei selbst nicht das Problem ist, sondern
nur ihre Zeilenaufteilung nicht zum fest angenommenen 512er-Chunk passt.

## Nutzen
- "Sub Decode" funktioniert danach für **jede** spec-konforme `RAW_Data`-Zeilenaufteilung, nicht
  nur zufällig für Aufnahmen, deren Zeilen exakt 512 Werte lang sind.
- Deckt insbesondere den in CONSTRAINTS vorgesehenen Testfall ab: eine für einen autorisierten
  Test gezielt gebaute/verkürzte RAW-Testdatei (z. B. um ein bestimmtes Protokoll-Muster isoliert
  zu prüfen) wird nicht mehr grundlos als "kein Protokoll erkannt" verworfen.
- Kein Verhaltensunterschied für den bereits funktionierenden Fall (Stock-"Read RAW"-Aufnahmen,
  Zeilen exakt 512): der neue Pfad liest dort exakt dieselbe Chunk-Größe wie vorher.

## Betroffene Dateien
- `helpers/raw_file_reader.c` — `raw_file_reader_load_chunk()` (einzige zu ändernde Funktion,
  plus ein neuer kleiner Peek-Helfer direkt darüber).

## Risiko
- Niedrig: der neue Peek-Helfer liest nichts, was nicht ohnehin gelesen würde — er nutzt exakt
  dieselbe bereits vorhandene `local_flipper_format_stream_get_value_count()`-Funktion, die
  `raw_file_reader_open()` schon zum Vorab-Zählen aller Zeilen verwendet, sichert die
  Stream-Position vor dem Peek über `stream_tell()`/`stream_seek(..., StreamOffsetFromStart)`
  und stellt sie danach exakt wieder her, bevor der eigentliche
  `flipper_format_read_int32()`-Aufruf (unverändert) läuft.
- Für Dateien, die dem dokumentierten Format entsprechen (Zeilen ≤ 512 Werte), ist `to_read`
  nach dem Fix immer exakt die aktuelle Zeilenlänge (durch `RAW_READER_BUFFER_SIZE` nur als
  zusätzliche Absicherung nach oben begrenzt) — der bestehende `reader->buffer[512]`-Puffer wird
  nie überschritten.
- Restfall außerhalb der Spezifikation: eine einzelne `RAW_Data:`-Zeile mit **mehr** als 512
  Werten (spec-widrig, keine mir bekannte Quelle erzeugt das) würde weiterhin nur bis zur
  Puffergrenze gelesen und der Rest dieser einen Zeile beim nächsten Chunk übersprungen — exakt
  das schon vor dem Fix bestehende Verhalten für diesen Grenzfall, also keine Verschlechterung.
- Ein zusätzlicher `stream_tell()`/`stream_seek()`-Umweg pro Chunk (statt nur des einen
  `flipper_format_read_int32()`-Aufrufs) — vernachlässigbarer Zusatzaufwand gegenüber dem
  SD-Karten-Zugriff, der ohnehin pro Chunk stattfindet.

## Port-Skizze
```diff
--- a/helpers/raw_file_reader.c
+++ b/helpers/raw_file_reader.c
@@
+static bool raw_file_reader_peek_line_count(RawFileReader* reader, uint32_t* count) {
+    Stream* stream = reader->ff->stream;
+    size_t pos = stream_tell(stream);
+    bool found = local_flipper_format_stream_get_value_count(
+        stream, "RAW_Data", count, reader->ff->strict_mode);
+    stream_seek(stream, (int32_t)pos, StreamOffsetFromStart);
+    return found;
+}
+
 static bool raw_file_reader_load_chunk(RawFileReader* reader) {
     if(reader->file_finished) return false;

-    size_t to_read = (reader->count < RAW_READER_BUFFER_SIZE) ? reader->count :
-                                                                RAW_READER_BUFFER_SIZE;
+    uint32_t line_count = 0;
+    if(!raw_file_reader_peek_line_count(reader, &line_count) || line_count == 0) {
+        reader->file_finished = true;
+        return false;
+    }
+
+    size_t to_read = (line_count < RAW_READER_BUFFER_SIZE) ? line_count : RAW_READER_BUFFER_SIZE;
+    if(to_read > reader->count) {
+        to_read = reader->count;
+    }

     if(!flipper_format_read_int32(reader->ff, "RAW_Data", reader->buffer, to_read)) {
         reader->file_finished = true;
         return false;
     }
```

`raw_file_reader_peek_line_count()` nutzt dieselbe lokale
`local_flipper_format_stream_get_value_count()`, die `raw_file_reader_open()` bereits zum
Vorab-Zählen aller Zeilen aufruft — hier nur für die eine, direkt bevorstehende Zeile, mit
Positions-Save/Restore drumherum, damit der nachfolgende `flipper_format_read_int32()`-Aufruf an
derselben Stelle ansetzt wie vorher.

## Testhinweis
- Host-Repro (ohne Hardware, in `repro-raw-file-reader-chunk-line-boundary/`):
  - `gcc -O1 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro`
  - Modelliert `raw_file_reader_load_chunk()` (ALT/NEU) über einem `FakeStream`, der das
    "immer zur nächsten `RAW_Data`-Zeile springen, exakt `data_size` Werte oder kompletter
    Fehlschlag"-Verhalten von `flipper_format_read_int32()` nachbildet. Vier Fälle: kurze
    Ein-Zeilen-Datei, Stock-"Read RAW"-Layout (512/512/300), handgebaute Testdatei mit
    300er-Zeilen, sowie eine 400er-Zeilenaufteilung wie von einem anderen Tool. Ergebnis: ALT
    liest nur die ersten beiden Fälle vollständig (0 von N Samples bei den beiden anderen), NEU
    liest in allen vier Fällen die volle Aufnahme. Exit 0.
- Lokaler Hardware-Test (nach Einbau des Patches):
  1. Eine reale, längere "Read RAW"-Aufnahme (>512 Flanken) über die Stock-SubGhz-App aufnehmen
     und speichern; mit ProtoPirates "Sub Decode" laden — muss weiterhin wie vorher funktionieren
     (Regressionscheck für den bereits funktionierenden Fall).
  2. Dieselbe `.sub`-Datei auf einem PC in mehrere kleinere `RAW_Data:`-Zeilen umformatieren
     (z. B. je 100–300 Werte statt 512, Gesamtwerte unverändert lassen) und erneut mit
     "Sub Decode" laden — vor dem Fix: sofort "No ProtoPirate protocol detected", ohne dass ein
     einziges Sample verarbeitet wurde; nach dem Fix: identisches Decode-Ergebnis wie mit der
     Originaldatei.
  3. Fortschrittsanzeige während des Decodierens beobachten (`raw_file_reader_get_progress()`
     bzw. die Empfänger-Fortschrittsanzeige) — sollte bei der umformatierten Datei jetzt
     kontinuierlich bis 100 % laufen statt sofort abzubrechen.

## Quelle
- `G4MEOVER18/ProtoPirate` (main, `e3d9da5`): `helpers/raw_file_reader.c`
  (`raw_file_reader_load_chunk()`, `raw_file_reader_open()`,
  `local_flipper_format_stream_get_value_count()`), `helpers/raw_file_reader.h`
  (`RAW_READER_BUFFER_SIZE`), `scenes/protopirate_scene_sub_decode.c`
  (`DecodeStateDecodingRaw`, Aufrufer von `raw_file_reader_get_next()`/`_is_finished()`).
- Firmware-Referenz (Momentum `mntm-012`-Baustand, unverändert relevant seit Stock-Flipper):
  `documentation/file_formats/SubGhzFileFormats.md` (`RAW_Data`-Feldbeschreibung, "Up to 512
  values per line"), `lib/subghz/protocols/raw.c` (`SUBGHZ_DOWNLOAD_MAX_SIZE`,
  `subghz_protocol_decoder_raw_feed()`/`_save_to_file_write()` als Beleg für den
  512er-Zufallstreffer bei Stock-Aufnahmen), `lib/flipper_format/flipper_format_stream.c`
  (`flipper_format_stream_read_value_line()`, Zeilen-scoped-Read-Verhalten).
