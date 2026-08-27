// Host-Modell von helpers/raw_file_reader.c (RawFileReader in ProtoPirate). Bildet exakt das
// Verhalten von flipper_format_read_int32()/flipper_format_stream_read_value_line() nach, wie es
// in lib/flipper_format/flipper_format_stream.c der Firmware implementiert ist: jeder Aufruf
// sucht per local_flipper_format_stream_seek_to_key() zunaechst das naechste Vorkommen des
// Schluessels "RAW_Data" (Text-Suche vorwaerts ab der aktuellen Stream-Position) und liest dann
// GENAU data_size Werte AUS DIESER EINEN ZEILE -- reicht die Zeile nicht (Zeilenende/"last" vor
// Erreichen von data_size), schlaegt der GESAMTE Aufruf fehl, ohne die Restwerte der Zeile
// irgendwo zu vermerken. Ein Folgeaufruf sucht danach den naechsten Schluessel-Treffer und
// ueberspringt damit stillschweigend jeden Rest, der auf der vorherigen Zeile nicht gelesen wurde.
//
// Laut Firmware-Doku (documentation/file_formats/SubGhzFileFormats.md, RAW_Data-Feld) sind
// "RAW_Data:"-Zeilen auf hoechstens 512 Werte begrenzt; laengere Aufnahmen verteilen sich auf
// mehrere Zeilen, mit BELIEBIGER Aufteilung pro Zeile (nur die Obergrenze 512 ist vorgegeben).
// raw_file_reader_load_chunk() fordert pro Aufruf jedoch immer exakt RAW_READER_BUFFER_SIZE = 512
// Werte an und geht damit stillschweigend davon aus, dass JEDE Zeile (bis auf die letzte) GENAU
// 512 Werte enthaelt. Das trifft zufaellig auf Aufnahmen zu, die per Stock-Firmware-"Read RAW"
// entstanden sind (lib/subghz/protocols/raw.c, SUBGHZ_DOWNLOAD_MAX_SIZE = 512: die Firmware
// flusht dort selbst exakt bei 512 gepufferten Werten pro Zeile) -- die Format-Doku erlaubt
// jedoch ausdruecklich JEDE Aufteilung bis 512/Zeile, nicht nur exakt-512-Zeilen. Eine
// handgebaute Testdatei, eine mit einem anderen Tool/Fork (eigene interne Puffergroesse)
// erzeugte Aufnahme, oder schlicht eine Datei mit kleineren Zeilen ist damit ein voellig
// spec-konformes RAW_Data-Layout, das raw_file_reader_load_chunk() trotzdem nicht lesen kann:
// sobald eine Zeile VOR Erreichen von 512 Werten endet, schlaegt der GESAMTE
// flipper_format_read_int32()-Aufruf fehl (kein Teilergebnis), raw_file_reader_load_chunk()
// haelt das faelschlich fuer Dateiende, und raw_file_reader_get_next() liefert sofort false --
// der Aufrufer (protopirate_scene_sub_decode.c) zeigt "No ProtoPirate protocol detected in
// signal", obwohl die Datei nie tatsaechlich in den Decoder eingespeist wurde.
//
// ALT/NEU unten bilden raw_file_reader_load_chunk() (bzw. dessen einzige zu aendernde
// to_read-Berechnung) 1:1 nach; das darunterliegende flipper_format_read_int32()-Verhalten ist in
// FakeStream nachgebildet (Zeilen-Skip bei nicht exakt beendeter Vorzeile, Fehlschlag bei
// data_size > aktuelle Zeilenlaenge -- siehe Kommentar in read_int32()).
//
// Build & run:
//   gcc -O1 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro
//
// Erwartete Ausgabe: fuer eine Stock-"Read RAW"-Aufnahme (Zeilen exakt 512, letzte Zeile kuerzer)
// liest bereits ALT alles korrekt -- der Bug wird durch den Zufall verdeckt, dass
// RAW_READER_BUFFER_SIZE == SUBGHZ_DOWNLOAD_MAX_SIZE der Stock-Firmware ist. Fuer jede andere,
// ebenso spec-konforme Zeilenaufteilung (z. B. Testdatei mit 300er-Zeilen) liest ALT 0 von N
// Samples. NEU liest in jedem Testfall alle N Samples vollstaendig.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define RAW_READER_BUFFER_SIZE 512U

// ---- FakeStream: bildet die fuer diesen Bug relevanten Eigenschaften von
//      flipper_format_read_int32() auf einer Menge von "RAW_Data:"-Zeilen nach ----

typedef struct {
    const int32_t* const* lines; // lines[i] = Werte der i-ten "RAW_Data:"-Zeile
    const size_t* line_lens; // Laenge jeder Zeile
    size_t line_count;
    size_t cur_line; // Index der Zeile, an deren Anfang die naechste Schluessel-Suche landet
    size_t cur_pos; // Position innerhalb cur_line; 0 bedeutet "noch nichts aus dieser Zeile gelesen"
} FakeStream;

// Modelliert flipper_format_read_int32(ff, "RAW_Data", out, data_size):
// seekt zuerst zum naechsten "RAW_Data"-Vorkommen (ueberspringt dabei jeden nicht konsumierten
// Rest der zuletzt angefassten Zeile) und verlangt dann GENAU data_size Werte aus dieser einen
// Zeile; reicht die Zeile nicht, schlaegt der gesamte Aufruf fehl (kein Teilergebnis).
static bool fake_read_int32(FakeStream* s, int32_t* out, size_t data_size) {
    if(s->cur_pos != 0) {
        // vorherige Zeile nicht exakt bis zum Ende gelesen -> naechster Treffer ist die
        // Folgezeile, der Rest der aktuellen Zeile wird stillschweigend uebersprungen
        s->cur_line++;
        s->cur_pos = 0;
    }
    if(s->cur_line >= s->line_count) return false; // kein weiteres "RAW_Data"-Vorkommen -> echtes EOF

    size_t len = s->line_lens[s->cur_line];
    if(data_size > len) return false; // "last" vor Erreichen von data_size -> gesamter Aufruf scheitert

    for(size_t i = 0; i < data_size; i++) out[i] = s->lines[s->cur_line][i];
    s->cur_pos = data_size;
    if(s->cur_pos == len) {
        // Zeile exakt zu Ende gelesen: naechster Aufruf landet direkt auf der Folgezeile
        s->cur_line++;
        s->cur_pos = 0;
    }
    return true;
}

// Liefert die Laenge der Zeile, auf die der naechste Schluessel-Treffer zeigen wuerde, ohne den
// FakeStream zu veraendern -- Modell fuer den neuen Peek-Helfer (nutzt lokal dieselbe Semantik
// wie local_flipper_format_stream_get_value_count(), nur ohne echten Stream-Seek/Restore-Bedarf,
// da das Modell den "aktuellen Zeilen-Cursor" bereits explizit haelt).
static bool fake_peek_line_count(const FakeStream* s, size_t* count) {
    size_t line = s->cur_pos != 0 ? s->cur_line + 1 : s->cur_line;
    if(line >= s->line_count) return false;
    *count = s->line_lens[line];
    return true;
}

// ---- ALT: raw_file_reader_load_chunk(), IST-Zustand (fixer 512er-Chunk) ----

static size_t old_next_to_read(FakeStream* s, size_t remaining_total) {
    (void)s;
    return remaining_total < RAW_READER_BUFFER_SIZE ? remaining_total : RAW_READER_BUFFER_SIZE;
}

// ---- NEU: to_read auf die tatsaechliche naechste Zeilenlaenge begrenzt ----

static bool new_next_to_read(FakeStream* s, size_t remaining_total, size_t* to_read) {
    size_t line_count = 0;
    if(!fake_peek_line_count(s, &line_count) || line_count == 0) return false;
    size_t v = line_count < RAW_READER_BUFFER_SIZE ? line_count : RAW_READER_BUFFER_SIZE;
    if(v > remaining_total) v = remaining_total;
    *to_read = v;
    return true;
}

// ---- Testlauf: liest eine komplette Datei ueber wiederholte load_chunk()-Aufrufe ----

static size_t run_old(FakeStream s, size_t total_count) {
    size_t remaining = total_count;
    size_t samples_read = 0;
    int32_t buf[RAW_READER_BUFFER_SIZE];
    while(remaining > 0) {
        size_t to_read = old_next_to_read(&s, remaining);
        if(to_read == 0) break;
        if(!fake_read_int32(&s, buf, to_read)) break; // load_chunk() haelt das faelschlich fuer EOF
        remaining -= to_read;
        samples_read += to_read;
    }
    return samples_read;
}

static size_t run_new(FakeStream s, size_t total_count) {
    size_t remaining = total_count;
    size_t samples_read = 0;
    int32_t buf[RAW_READER_BUFFER_SIZE];
    while(remaining > 0) {
        size_t to_read = 0;
        if(!new_next_to_read(&s, remaining, &to_read)) break;
        if(!fake_read_int32(&s, buf, to_read)) break;
        remaining -= to_read;
        samples_read += to_read;
    }
    return samples_read;
}

// Baut Testdaten: Zeilen mit den angegebenen Laengen (Werte selbst sind fuer diesen Bug irrelevant).
#define MAX_LINES 16
static int32_t g_line_storage[MAX_LINES][RAW_READER_BUFFER_SIZE];
static const int32_t* g_line_ptrs[MAX_LINES];
static size_t g_line_lens[MAX_LINES];

static FakeStream build_case(const size_t* lens, size_t n, size_t* total_out) {
    size_t total = 0;
    for(size_t i = 0; i < n; i++) {
        g_line_lens[i] = lens[i];
        for(size_t j = 0; j < lens[i]; j++) g_line_storage[i][j] = (int32_t)(j + 1);
        g_line_ptrs[i] = g_line_storage[i];
        total += lens[i];
    }
    *total_out = total;
    FakeStream s = {.lines = g_line_ptrs, .line_lens = g_line_lens, .line_count = n, .cur_line = 0, .cur_pos = 0};
    return s;
}

int main(void) {
    struct {
        const char* label;
        size_t lens[MAX_LINES];
        size_t n;
    } cases[] = {
        {"1 Zeile, 300 Werte (kurze Aufnahme, passt in eine Zeile)", {300}, 1},
        {"Stock-\"Read RAW\": 512,512,300 (SUBGHZ_DOWNLOAD_MAX_SIZE-Flush, 1324 Werte)",
         {512, 512, 300},
         3},
        {"Handgebaute Testdatei: 300,300,300 (900 Werte, spec-konform, andere Zeilengroesse)",
         {300, 300, 300},
         3},
        {"Anderes Capture-Tool/Fork: 400,400,400,400 (1600 Werte, eigene Puffergroesse)",
         {400, 400, 400, 400},
         4},
    };
    const size_t n_cases = sizeof(cases) / sizeof(cases[0]);

    int failures = 0;
    for(size_t c = 0; c < n_cases; c++) {
        size_t total = 0;
        FakeStream s_old = build_case(cases[c].lens, cases[c].n, &total);
        FakeStream s_new = s_old;

        size_t got_old = run_old(s_old, total);
        size_t got_new = run_new(s_new, total);

        printf(
            "%-58s  ALT: %4zu/%4zu Samples gelesen  |  NEU: %4zu/%4zu Samples gelesen\n",
            cases[c].label,
            got_old,
            total,
            got_new,
            total);

        if(got_new != total) failures++;
    }

    printf(
        "\n%s\n",
        failures == 0 ?
            "Bestaetigt: NEU liest in jedem Testfall die volle Aufnahme (0 Verlust), auch bei "
            "abweichender Zeilenaufteilung. ALT liest nur die Stock-\"Read RAW\"-Aufnahme korrekt "
            "(Zeilen zufaellig exakt 512, wie die Stock-Firmware selbst flusht) und scheitert bei "
            "jeder anderen, ebenso spec-konformen Zeilenaufteilung bereits im allerersten Chunk "
            "mit 0 gelesenen Samples." :
            "FEHLER: NEU verliert in mindestens einem Testfall Samples.");

    return failures == 0 ? 0 : 1;
}
