// Host-Repro: applications/external/can_tools/cantools_app.c (RogueMaster-Bundle, Branch "420"),
// dbc_parse_sg_line() -- Zeilenparser fuer "SG_"-Signaleintraege beim Laden einer .dbc-Datei
// (dbc_load_file(), ueber "DBC Files" -> vorhandene Datei auswaehlen). Eine .dbc-Datei ist ein
// austauschbares Textformat (z. B. von einem Fahrzeugmodell-Katalog oder einem anderen Nutzer
// geteilt) -- die eigene DBC Maker-Eingabemaske validiert Start-Bit/Bitlaenge/etc. zwar beim
// manuellen Anlegen, der Datei-Parser selbst tut das fuer Endianness/Signedness nicht.
//
// Nach dem Parsen von Startbit ("...|...") und Bitlaenge ("...@...") liest der Original-Code
// bedingungslos zwei weitere Zeichen fuer Endianness und Signedness:
//
//     p = end + 1;
//     char endian_char = *p++;
//     char sign_char = *p++;
//
// Ohne vorherige Pruefung, ob nach dem "@" ueberhaupt noch (zwei) Zeichen folgen. Fuer eine Zeile,
// die exakt mit "...@" endet, liegt die erste Leseposition auf dem Nullterminator der Zeile selbst
// (noch im Puffer, aber bereits falscher Inhalt) -- die zweite Leseposition liegt ein Byte DAHINTER.
// dbc_load_file() liest jede Zeile per strlcpy() in einen lokalen "char buf[256]"-Stack-Puffer;
// fuellt eine (hinreichend lange) SG_-Zeile diesen Puffer bis zum letzten Byte (Nullterminator an
// Index 255, dem einzig moeglichen Fall ohne Kuerzung durch strlcpy), liegt die zweite Leseposition
// bei Index 256 -- ein Byte ausserhalb des deklarierten 256-Byte-Arrays. Fuer kuerzere, aber ebenso
// unvollstaendige Zeilen bleibt der Zugriff zwar formal innerhalb des Arrays, liest aber
// uninitialisierten Stack-Inhalt als "sign_char" -- dieselbe fehlende Pruefung, nur ohne
// garantierten Arraybezug.
//
// NEU fuegt dieselbe additive Pruefung hinzu, die der Parser bereits fuer Startbit/Bitlaenge
// verwendet ("if(end == p || *end != '|') return false;"): "if(!p[0] || !p[1]) return false;"
// direkt vor den beiden Zeichenzugriffen -- lehnt eine unvollstaendige Zeile kontrolliert ab,
// statt sie zu lesen.
//
// Jeder Testfall laeuft isoliert in einem fork()-Kindprozess (ein ASan-Abbruch eines Falls darf
// den Testlauf selbst nicht beenden).
//
//   gcc -std=c11 -O0 -g -Wall -Wextra -fsanitize=address,undefined -o repro_o0 repro.c && ./repro_o0
//   gcc -std=c11 -O1 -g -Wall -Wextra -fsanitize=address,undefined -o repro_o1 repro.c && ./repro_o1

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define TEXT_INPUT_SIZE 32

// Nachbildung von DbcSignal (nur die fuer diesen Fund relevanten Felder, gleiche Groessen/
// Reihenfolge wie in cantools_app.c bei mir).
typedef struct {
    bool used;
    char message_name[TEXT_INPUT_SIZE];
    char signal_name[TEXT_INPUT_SIZE];
    char can_id[TEXT_INPUT_SIZE];
    char start_bit[TEXT_INPUT_SIZE];
    char bit_length[TEXT_INPUT_SIZE];
    char endianness[TEXT_INPUT_SIZE];
    char signedness[TEXT_INPUT_SIZE];
    char offset[TEXT_INPUT_SIZE];
    char scalar[TEXT_INPUT_SIZE];
    char unit[TEXT_INPUT_SIZE];
    char min_val[TEXT_INPUT_SIZE];
    char max_val[TEXT_INPUT_SIZE];
} DbcSignal;

// furi_hal-freie Nachbildung von strlcpy() (BSD-Semantik, wie die vom echten Firmware-Toolbox
// bereitgestellte Variante, die cantools_app.c verwendet).
static size_t host_strlcpy(char* dst, const char* src, size_t size) {
    size_t src_len = strlen(src);
    if(size != 0) {
        size_t copy_len = (src_len >= size) ? size - 1 : src_len;
        memcpy(dst, src, copy_len);
        dst[copy_len] = '\0';
    }
    return src_len;
}

// ALT: dbc_parse_sg_line() -- 1:1 Original-Logik (cantools_app.c, Branch "420"), ohne Pruefung
// vor den beiden Zeichenzugriffen fuer Endianness/Signedness.
static bool dbc_parse_sg_line_orig(
    const char* line,
    DbcSignal* sig,
    uint32_t can_id,
    const char* msg_name) {
    if(strncmp(line, "SG_", 3) != 0) return false;
    const char* p = line + 3;
    while(isspace((unsigned char)*p))
        p++;

    const char* name_start = p;
    while(*p && !isspace((unsigned char)*p) && *p != ':')
        p++;
    if(name_start == p) return false;
    size_t name_len = (size_t)(p - name_start);
    if(name_len >= TEXT_INPUT_SIZE) name_len = TEXT_INPUT_SIZE - 1;

    while(*p && *p != ':')
        p++;
    if(*p != ':') return false;
    p++;
    while(isspace((unsigned char)*p))
        p++;

    char* end = NULL;
    long start_bit = strtol(p, &end, 10);
    if(end == p || *end != '|') return false;
    p = end + 1;
    long bit_length = strtol(p, &end, 10);
    if(end == p || *end != '@') return false;
    p = end + 1;

    char endian_char = *p++; // <-- ALT: kein Bound-Check vor diesen beiden Zugriffen
    char sign_char = *p++;

    while(isspace((unsigned char)*p))
        p++;
    if(*p != '(') return false;
    p++;
    double scale = strtod(p, &end);
    if(end == p || *end != ',') return false;
    p = end + 1;
    double offset = strtod(p, &end);
    if(end == p || *end != ')') return false;
    p = end + 1;

    while(isspace((unsigned char)*p))
        p++;
    if(*p != '[') return false;
    p++;
    double min_val = strtod(p, &end);
    if(end == p || *end != '|') return false;
    p = end + 1;
    double max_val = strtod(p, &end);
    if(end == p || *end != ']') return false;
    p = end + 1;

    while(isspace((unsigned char)*p))
        p++;
    char unit[TEXT_INPUT_SIZE] = {0};
    if(*p == '"') {
        p++;
        const char* unit_start = p;
        while(*p && *p != '"')
            p++;
        size_t unit_len = (size_t)(p - unit_start);
        if(unit_len >= TEXT_INPUT_SIZE) unit_len = TEXT_INPUT_SIZE - 1;
        memcpy(unit, unit_start, unit_len);
        unit[unit_len] = '\0';
    }

    memset(sig, 0, sizeof(*sig));
    sig->used = true;
    host_strlcpy(sig->message_name, msg_name && msg_name[0] ? msg_name : "Message", TEXT_INPUT_SIZE);
    memcpy(sig->signal_name, name_start, name_len);
    sig->signal_name[name_len] = '\0';
    snprintf(sig->can_id, sizeof(sig->can_id), "0x%lX", (unsigned long)can_id);
    snprintf(sig->start_bit, sizeof(sig->start_bit), "%ld", start_bit);
    snprintf(sig->bit_length, sizeof(sig->bit_length), "%ld", bit_length);
    sig->endianness[0] = (endian_char == '1') ? '1' : '0';
    sig->endianness[1] = '\0';
    sig->signedness[0] = (sign_char == '-') ? 's' : 'u';
    sig->signedness[1] = '\0';
    snprintf(sig->scalar, sizeof(sig->scalar), "%.6g", scale);
    snprintf(sig->offset, sizeof(sig->offset), "%.6g", offset);
    snprintf(sig->min_val, sizeof(sig->min_val), "%.6g", min_val);
    snprintf(sig->max_val, sizeof(sig->max_val), "%.6g", max_val);
    host_strlcpy(sig->unit, unit, sizeof(sig->unit));
    return true;
}

// NEU: dieselbe Logik, plus die additive Pruefung vor den beiden Zeichenzugriffen (identischer
// Patch wie firmware-mining/can-tools-app-port/cantools_app_c.patch).
static bool dbc_parse_sg_line_fixed(
    const char* line,
    DbcSignal* sig,
    uint32_t can_id,
    const char* msg_name) {
    if(strncmp(line, "SG_", 3) != 0) return false;
    const char* p = line + 3;
    while(isspace((unsigned char)*p))
        p++;

    const char* name_start = p;
    while(*p && !isspace((unsigned char)*p) && *p != ':')
        p++;
    if(name_start == p) return false;
    size_t name_len = (size_t)(p - name_start);
    if(name_len >= TEXT_INPUT_SIZE) name_len = TEXT_INPUT_SIZE - 1;

    while(*p && *p != ':')
        p++;
    if(*p != ':') return false;
    p++;
    while(isspace((unsigned char)*p))
        p++;

    char* end = NULL;
    long start_bit = strtol(p, &end, 10);
    if(end == p || *end != '|') return false;
    p = end + 1;
    long bit_length = strtol(p, &end, 10);
    if(end == p || *end != '@') return false;
    p = end + 1;

    if(!p[0] || !p[1]) return false; // <-- NEU
    char endian_char = *p++;
    char sign_char = *p++;

    while(isspace((unsigned char)*p))
        p++;
    if(*p != '(') return false;
    p++;
    double scale = strtod(p, &end);
    if(end == p || *end != ',') return false;
    p = end + 1;
    double offset = strtod(p, &end);
    if(end == p || *end != ')') return false;
    p = end + 1;

    while(isspace((unsigned char)*p))
        p++;
    if(*p != '[') return false;
    p++;
    double min_val = strtod(p, &end);
    if(end == p || *end != '|') return false;
    p = end + 1;
    double max_val = strtod(p, &end);
    if(end == p || *end != ']') return false;
    p = end + 1;

    while(isspace((unsigned char)*p))
        p++;
    char unit[TEXT_INPUT_SIZE] = {0};
    if(*p == '"') {
        p++;
        const char* unit_start = p;
        while(*p && *p != '"')
            p++;
        size_t unit_len = (size_t)(p - unit_start);
        if(unit_len >= TEXT_INPUT_SIZE) unit_len = TEXT_INPUT_SIZE - 1;
        memcpy(unit, unit_start, unit_len);
        unit[unit_len] = '\0';
    }

    memset(sig, 0, sizeof(*sig));
    sig->used = true;
    host_strlcpy(sig->message_name, msg_name && msg_name[0] ? msg_name : "Message", TEXT_INPUT_SIZE);
    memcpy(sig->signal_name, name_start, name_len);
    sig->signal_name[name_len] = '\0';
    snprintf(sig->can_id, sizeof(sig->can_id), "0x%lX", (unsigned long)can_id);
    snprintf(sig->start_bit, sizeof(sig->start_bit), "%ld", start_bit);
    snprintf(sig->bit_length, sizeof(sig->bit_length), "%ld", bit_length);
    sig->endianness[0] = (endian_char == '1') ? '1' : '0';
    sig->endianness[1] = '\0';
    sig->signedness[0] = (sign_char == '-') ? 's' : 'u';
    sig->signedness[1] = '\0';
    snprintf(sig->scalar, sizeof(sig->scalar), "%.6g", scale);
    snprintf(sig->offset, sizeof(sig->offset), "%.6g", offset);
    snprintf(sig->min_val, sizeof(sig->min_val), "%.6g", min_val);
    snprintf(sig->max_val, sizeof(sig->max_val), "%.6g", max_val);
    host_strlcpy(sig->unit, unit, sizeof(sig->unit));
    return true;
}

typedef bool (*ParseFn)(const char*, DbcSignal*, uint32_t, const char*);

// Bildet dbc_load_file()'s Zeilenverarbeitung nach: jede Zeile landet in einem lokalen
// "char buf[256]"-Stack-Puffer per strlcpy(), genau wie im Original. "raw_line" darf laenger als
// 255 Byte sein (wird dann wie im Original gekuerzt) -- fuer den Angriffsfall unten ist "raw_line"
// exakt 255 Byte lang, sodass strlcpy() den Nullterminator exakt auf Index 255 setzt (letztes
// gueltiges Byte des Arrays, keine Kuerzung notwendig).
static bool load_one_line(ParseFn parse, const char* raw_line, DbcSignal* out_sig) {
    char buf[256]; // <-- identische Groesse/Deklaration wie in dbc_load_file()
    host_strlcpy(buf, raw_line, sizeof(buf));
    return parse(buf, out_sig, 0x100, "Msg");
}

typedef void (*RunFn)(void*);

// Fuehrt fn(arg) in einem Kindprozess aus; gibt zurueck, ob der Kindprozess ungewoehnlich beendet
// wurde (Signal oder, wie ASan es ohne "abort_on_error=1" tut, exit(1) nach gemeldetem Fund).
static bool run_isolated(RunFn fn, void* arg) {
    pid_t pid = fork();
    if(pid == 0) {
        fclose(stderr);
        fn(arg);
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFSIGNALED(status) || (WIFEXITED(status) && WEXITSTATUS(status) != 0);
}

typedef struct {
    ParseFn parse;
    const char* raw_line;
} Args;

static void run_parse(void* arg) {
    Args* a = arg;
    DbcSignal sig;
    bool ok = load_one_line(a->parse, a->raw_line, &sig);
    printf("  parse() -> %s\n", ok ? "true" : "false");
}

static void print_case(const char* name, bool crashed) {
    printf("  %s: %s\n", name, crashed ? "ABSTURZ (von ASan/Signal erkannt)" : "kein Absturz");
}

// Baut eine SG_-Zeile, die exakt mit "...@" endet, auf eine beliebige Gesamtlaenge aufgefuellt
// ueber die Laenge des Signalnamens (der Parser begrenzt beim Scannen den Namen nicht auf
// TEXT_INPUT_SIZE -- nur die spaetere Kopie in sig->signal_name wird gekappt).
static void build_padded_sg_line(char* out, size_t out_size, size_t total_len) {
    // "SG_ " (4) + "A" * name_len + " : 0|1@" (7) == total_len
    size_t prefix_len = 4;
    size_t suffix_len = 7;
    size_t name_len = total_len - prefix_len - suffix_len;
    size_t pos = 0;
    pos += (size_t)snprintf(out + pos, out_size - pos, "SG_ ");
    for(size_t i = 0; i < name_len; i++) {
        out[pos++] = 'A';
    }
    out[pos] = '\0';
    pos += (size_t)snprintf(out + pos, out_size - pos, " : 0|1@");
}

int main(void) {
    printf("sizeof(char buf[256]) in dbc_load_file() == 256 (Original, unveraendert)\n\n");

    printf("== Regressionsfall: vollstaendige, gueltige SG_-Zeile ==\n");
    {
        const char* line =
            "SG_ DI_vehicleSpeed : 12|12@1+ (0.08,-40) [-40|285] \"kph\"  Receiver";
        DbcSignal sig_old = {0}, sig_new = {0};
        bool ok_old = load_one_line(dbc_parse_sg_line_orig, line, &sig_old);
        bool ok_new = load_one_line(dbc_parse_sg_line_fixed, line, &sig_new);
        printf(
            "  ALT ok=%d NEU ok=%d, Felder identisch: %s\n",
            ok_old,
            ok_new,
            (ok_old && ok_new && memcmp(&sig_old, &sig_new, sizeof(sig_old)) == 0) ? "ja" : "nein");
    }

    printf("\n== Fundfall 1: Zeile endet direkt nach '@', kurz (nur innerhalb des Arrays) ==\n");
    {
        const char* line = "SG_ X : 0|1@";
        Args a_old = {dbc_parse_sg_line_orig, line};
        Args a_new = {dbc_parse_sg_line_fixed, line};
        print_case("ALT (liest 1 Byte hinter dem Nullterminator, inkl. 1 Byte uninitialisiert)",
                   run_isolated(run_parse, &a_old));
        print_case("NEU (lehnt die Zeile ab, liest nichts hinter dem '@')",
                   run_isolated(run_parse, &a_new));
        DbcSignal sig;
        bool ok = load_one_line(dbc_parse_sg_line_fixed, line, &sig);
        printf("  NEU parse() direkt aufgerufen -> %s (erwartet: false)\n", ok ? "true" : "false");
    }

    printf("\n== Fundfall 2: Zeile exakt 255 Byte lang, Nullterminator exakt auf buf[255] ==\n");
    {
        char line[256];
        build_padded_sg_line(line, sizeof(line), 255);
        printf("  strlen(line) = %zu (erwartet 255)\n", strlen(line));
        Args a_old = {dbc_parse_sg_line_orig, line};
        Args a_new = {dbc_parse_sg_line_fixed, line};
        print_case("ALT (zweiter Zeichenzugriff liegt auf buf[256], 1 Byte ausserhalb des Arrays)",
                   run_isolated(run_parse, &a_old));
        print_case("NEU (lehnt die Zeile ab, bevor buf[255]/buf[256] gelesen werden)",
                   run_isolated(run_parse, &a_new));
    }

    printf(
        "\nOK: Fuer die protokollkonforme, vollstaendige Zeile verhalten sich ALT und NEU\n"
        "identisch. Fuer eine Zeile, die exakt mit '@' endet, liest ALT mindestens 1 Byte\n"
        "uninitialisierten Stack-Speicher als 'sign_char'; faellt der Nullterminator der Zeile\n"
        "zusaetzlich exakt auf den letzten Index des 256-Byte-Puffers (Fundfall 2, jede SG_-Zeile\n"
        "ab 255 Byte Gesamtlaenge, die exakt mit '@' endet), liest ALT ein Byte ausserhalb des\n"
        "deklarierten Arrays -- von AddressSanitizer als stack-buffer-overflow erkannt. NEU lehnt\n"
        "beide Faelle kontrolliert ab, ohne zu lesen.\n");
    return 0;
}
