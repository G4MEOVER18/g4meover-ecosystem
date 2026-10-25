// Host-Repro: unitemp (quen0n/unitemp-flipperzero), sensors.c::unitemp_sensors_load()
//
// Diese Datei entnimmt die reale Parser-Zeile 1:1 aus
// quen0n/unitemp-flipperzero, Commit ae6271000f9f4a06ea8f36bb86229159331d9e6e,
// sensors.c, Zeilen 368-406 (unitemp_sensors_load()):
//
//   char name[11] = {0};
//   char model[11] = {0};
//   int temp_offset = 0;
//   int str_offset;
//
//   const char* line_cstr = furi_string_get_cstr(line);
//   sscanf(line_cstr, "%s %s %d %n", name, model, &temp_offset, &str_offset);
//   char* args = ((char*)(line_cstr + str_offset));
//
//   for (uint8_t i = 0; i < 10; i++) {
//       if (name[i] == '?') name[i] = ' ';
//   }
//
//   if (sensor_model != NULL && sizeof(name) > 0 && sizeof(name) <= 11) {
//       Sensor* sensor = unitemp_sensor_alloc(name, sensor_model, args);
//       ...
//
// `line_cstr` kommt direkt aus der Datei `sensors.list`
// (APP_DATA_PATH, normalerweise `/ext/apps_data/unitemp/sensors.list`), Zeile
// für Zeile per `stream_read_line()`/`furi_string_trim()` gelesen -- eine
// gewoehnliche Textdatei auf der SD-Karte. `%s` ohne Feldbreite in `sscanf()`
// schreibt beliebig viele Zeichen in den 11-Byte-Stack-Puffer `name`, bis zum
// naechsten Leerzeichen/Tab/Newline/EOF in der Eingabe -- es gibt keine
// Begrenzung auf 10 Nutzzeichen + Nullterminator.
//
// Die anschliessende Pruefung `sizeof(name) > 0 && sizeof(name) <= 11` ist
// wirkungslos: `sizeof(name)` ist eine Compile-Zeit-Konstante (hier immer
// 11, weil `name` als `char[11]` deklariert ist) und haengt nicht von der
// tatsaechlich eingelesenen Zeichenkettenlaenge ab -- der Ausdruck ist bei
// jedem Programmlauf wahr, unabhaengig vom Inhalt der Datei. Die Pruefung
// kommt zudem ohnehin erst NACH dem bereits abgelaufenen `sscanf()`-Aufruf.
//
// Dieses Repro bildet exakt diese Deklarationen und genau diesen
// `sscanf()`-Aufruf nach (keine Mock-Vereinfachung der Pufferlogik selbst)
// und ersetzt nur `furi_string_get_cstr()`/`FuriString*` durch ein
// gewoehnliches `const char*` (das reale Verhalten von
// `furi_string_get_cstr()` nach `furi_string_trim()`: ein simpler,
// nullterminierter C-String -- fuer die Pufferfrage ohne Bedeutung).
//
// Erwartung:
//   - Zeile 1 ("sens1 DS18B20 0 17"): Name <= 10 Zeichen -> kein Fund, ALT
//     und NEU verhalten sich identisch, kein Puffer-Overflow.
//   - Zeile 2 (11 Zeichen vor dem ersten Leerzeichen): ALT ueberschreibt den
//     11-Byte-Stack-Puffer -> AddressSanitizer meldet einen
//     stack-buffer-overflow in sscanf(); NEU (Feldbreitenbegrenzung "%10s")
//     bleibt innerhalb des Puffers.
//
// Build/Run:
//   clang -g -O0 -fsanitize=address,undefined -o repro repro.c && ./repro
//   (ALT schlaegt bei Zeile 2 mit ASan fehl; NEU besteht beide Zeilen.)

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#ifndef FIXED
// ALT: exakt wie im echten sensors.c, Zeile 374
#define PARSE_FORMAT "%s %s %d %n"
#else
// NEU: Feldbreite auf 10 Nutzzeichen begrenzt (Platz fuer den
// Nullterminator im 11-Byte-Puffer bleibt erhalten), minimal-invasiver Fix
#define PARSE_FORMAT "%10s %10s %d %n"
#endif

static int parse_line(const char* line_cstr, char* name, char* model) {
    int temp_offset = 0;
    int str_offset = 0;

    int n = sscanf(line_cstr, PARSE_FORMAT, name, model, &temp_offset, &str_offset);

    for(uint8_t i = 0; i < 10; i++) {
        if(name[i] == '?') name[i] = ' ';
    }

    return n;
}

static void run_case(const char* label, const char* line_cstr) {
    char name[11] = {0};
    char model[11] = {0};

    printf("[%s] Eingabezeile: \"%s\"\n", label, line_cstr);
    int n = parse_line(line_cstr, name, model);

    // Dieselbe (wirkungslose) Pruefung wie im Original, Zeile 388, zur
    // Dokumentation, dass sie den schon abgelaufenen Overflow nicht
    // verhindert hat:
    int bounds_check_ok = (sizeof(name) > 0 && sizeof(name) <= 11);

    printf(
        "  sscanf()-Rueckgabe: %d, name=\"%s\" (strlen=%zu), model=\"%s\", "
        "sizeof(name)-Check (wirkungslos): %s\n",
        n,
        name,
        strlen(name),
        model,
        bounds_check_ok ? "true (immer)" : "false");
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
#ifndef FIXED
    puts("=== ALT (sensors.c Zeile 374, unveraendert) ===");
#else
    puts("=== NEU (Feldbreite auf \"%10s %10s\" begrenzt) ===");
#endif

    // Fall 1: regulaerer, ueber die eigene Tastatur-Eingabe (text_input,
    // auf 11 Byte inkl. Nullterminator begrenzt) vergebener Name --
    // bleibt in beiden Varianten unauffaellig.
    run_case("Fall 1: Name <=10 Zeichen", "sens1 DS18B20 0 17");

    // Fall 2: eine Zeile aus `sensors.list`, deren erstes Token (der
    // "Name") 11 Zeichen lang ist -- z. B. durch manuelles Bearbeiten der
    // Textdatei auf der SD-Karte, einen fehlerhaften externen
    // Erzeuger/Sync-Schritt, oder die eigene Alt-Format-Migration aus
    // `/ext/unitemp/sensors.cfg`. Kein Flipper-/Hardwarezugriff noetig --
    // eine gewoehnliche Textzeile reicht.
    run_case("Fall 2: Name == 11 Zeichen (ueberlang)", "sensor_lang DS18B20 0 17");

    puts("ALLE PRUEFUNGEN DURCHLAUFEN (ohne ASan-Abbruch waere das ein Fund)");
    return 0;
}
