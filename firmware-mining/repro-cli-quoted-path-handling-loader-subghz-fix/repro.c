// Host-Repro fuer den args_read_probably_quoted_string_and_trim()-Fund
// (lib/toolbox/args.c). Bildet die drei tatsaechlich genutzten mlib-Primitive
// (m_string_search_char/m_string_set_n/m_string_right, siehe m-string.h in
// github.com/P-p-H-d/mlib, Commit-Stand vom Gegenpruefen dieses Funds)
// 1:1 in ihrer Semantik nach -- insbesondere den M_MIN()-Clamp in
// m_string_set_n() und den index==0-Fruehausstieg in m_string_right() --,
// damit das Verhalten ohne furi/mlib-Toolchain host-kompilierbar ist. Die
// beiden Varianten von args_read_probably_quoted_string_and_trim() selbst
// sind Zeile fuer Zeile aus der eigenen mntm-012-Basis (ALT) bzw. dem Patch
// (NEU) uebernommen.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define FURI_STRING_FAILURE ((size_t)-1)

typedef struct {
    char* buf;
    size_t len;
} MiniString;

static MiniString* ms_alloc_set_str(const char* s) {
    MiniString* m = malloc(sizeof(MiniString));
    m->len = strlen(s);
    m->buf = malloc(m->len + 1);
    memcpy(m->buf, s, m->len + 1);
    return m;
}

static MiniString* ms_alloc(void) {
    return ms_alloc_set_str("");
}

static void ms_free(MiniString* m) {
    free(m->buf);
    free(m);
}

static size_t ms_size(const MiniString* m) {
    return m->len;
}

static char ms_get_char(const MiniString* m, size_t idx) {
    assert(idx < m->len);
    return m->buf[idx];
}

// Nachbildung von m_string_search_char() (m-string.h): linearer Scan ab
// start, FURI_STRING_FAILURE wenn nicht gefunden -- kein Sonderfall fuer
// "nicht gefunden -> 0".
static size_t ms_search_char(const MiniString* m, char c, size_t start) {
    for(size_t i = start; i < m->len; i++) {
        if(m->buf[i] == c) return i;
    }
    return FURI_STRING_FAILURE;
}

// Nachbildung von m_string_set_n() (m-string.h Zeile ~514): size = MIN(size(ref)
// - offset, length) -- clamped, deshalb kein Overread trotz riesigem "length"
// bei einer fehlenden schliessenden Anfuehrung.
static void ms_set_n(MiniString* word, const MiniString* ref, size_t offset, size_t length) {
    assert(offset <= ref->len);
    size_t avail = ref->len - offset;
    size_t size = (avail < length) ? avail : length; // M_MIN
    free(word->buf);
    word->buf = malloc(size + 1);
    memmove(word->buf, ref->buf + offset, size);
    word->buf[size] = 0;
    word->len = size;
}

// Nachbildung von m_string_right() (m-string.h Zeile ~832): index>=size -> leer;
// sonst memmove der letzten (size-index) Zeichen nach vorne. index==0 ist ein
// reines No-op (kopiert den String auf sich selbst).
static void ms_right(MiniString* m, size_t index) {
    if(index >= m->len) {
        m->buf[0] = 0;
        m->len = 0;
        return;
    }
    size_t s2 = m->len - index;
    memmove(&m->buf[0], &m->buf[index], s2 + 1);
    m->len = s2;
}

static void ms_trim(MiniString* m) {
    size_t start = 0;
    while(start < m->len && (m->buf[start] == ' ' || m->buf[start] == '\t')) start++;
    size_t end = m->len;
    while(end > start && (m->buf[end - 1] == ' ' || m->buf[end - 1] == '\t')) end--;
    size_t new_len = end - start;
    memmove(m->buf, m->buf + start, new_len);
    m->buf[new_len] = 0;
    m->len = new_len;
}

static const char* ms_cstr(const MiniString* m) {
    return m->buf;
}

// args_get_first_word_length() + args_read_string_and_trim() -- unveraendert
// in ALT und NEU, wird vom Fallback-Zweig (kein fuehrendes Anfuehrungszeichen)
// genutzt.
static size_t args_get_first_word_length(MiniString* args) {
    size_t ws = ms_search_char(args, ' ', 0);
    if(ws == FURI_STRING_FAILURE) ws = ms_size(args);
    return ws;
}

static int args_read_string_and_trim(MiniString* args, MiniString* word) {
    size_t cmd_length = args_get_first_word_length(args);
    if(cmd_length == 0) return 0;
    ms_set_n(word, args, 0, cmd_length);
    ms_right(args, cmd_length);
    ms_trim(args);
    return 1;
}

// ALT: eigene mntm-012-Basis, byteidentisch zu lib/toolbox/args.c vor dem Patch.
static int args_read_probably_quoted_string_and_trim_ALT(MiniString* args, MiniString* word) {
    if(ms_size(args) > 1 && ms_get_char(args, 0) == '\"') {
        size_t second_quote_pos = ms_search_char(args, '\"', 1);

        if(second_quote_pos == 0) {
            return 0;
        }

        ms_set_n(word, args, 1, second_quote_pos - 1);
        ms_right(args, second_quote_pos + 1);
        ms_trim(args);
        return 1;
    } else {
        return args_read_string_and_trim(args, word);
    }
}

// NEU: mit dem Patch aus firmware-mining/cli-quoted-path-handling-loader-subghz-fix/patches/args_c.patch.
static int args_read_probably_quoted_string_and_trim_NEU(MiniString* args, MiniString* word) {
    if(ms_size(args) > 1 && ms_get_char(args, 0) == '\"') {
        size_t second_quote_pos = ms_search_char(args, '\"', 1);

        if(second_quote_pos == FURI_STRING_FAILURE) {
            return 0;
        }

        ms_set_n(word, args, 1, second_quote_pos - 1);
        ms_right(args, second_quote_pos + 1);
        ms_trim(args);
        return 1;
    } else {
        return args_read_string_and_trim(args, word);
    }
}

typedef int (*ParseFn)(MiniString*, MiniString*);

static int checks_run = 0;
static int checks_failed = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        checks_run++;                                                     \
        if(!(cond)) {                                                     \
            checks_failed++;                                              \
            printf("  FAIL (%s:%d): %s\n", __FILE__, __LINE__, #cond);    \
        }                                                                  \
    } while(0)

static void run_case(
    const char* label,
    ParseFn fn,
    const char* input,
    int expect_ok,
    const char* expect_word,
    const char* expect_rest) {
    MiniString* args = ms_alloc_set_str(input);
    MiniString* word = ms_alloc();

    int ok = fn(args, word);
    printf("  %-6s in=%-32s -> ok=%d word=\"%s\" rest=\"%s\"\n",
           label, input, ok, ms_cstr(word), ms_cstr(args));

    CHECK(ok == expect_ok);
    if(expect_ok) {
        CHECK(strcmp(ms_cstr(word), expect_word) == 0);
        CHECK(strcmp(ms_cstr(args), expect_rest) == 0);
    }

    ms_free(word);
    ms_free(args);
}

int main(void) {
    printf("== Test 1: uebereinstimmendes Verhalten (Regression) ==\n");
    const struct {
        const char* input;
        int ok;
        const char* word;
        const char* rest;
    } regression[] = {
        {"/ext/subghz/file.sub 10", 1, "/ext/subghz/file.sub", "10"},
        {"\"/ext/subghz/Some File.sub\" 10 0", 1, "/ext/subghz/Some File.sub", "10 0"},
        {"\"\" rest", 1, "", "rest"},
        {"\"only\"", 1, "only", ""},
        {"", 0, "", ""},
    };
    for(size_t i = 0; i < sizeof(regression) / sizeof(regression[0]); i++) {
        run_case("ALT", args_read_probably_quoted_string_and_trim_ALT, regression[i].input,
                 regression[i].ok, regression[i].word, regression[i].rest);
        run_case("NEU", args_read_probably_quoted_string_and_trim_NEU, regression[i].input,
                 regression[i].ok, regression[i].word, regression[i].rest);
    }

    printf("\n== Test 2: fehlende schliessende Anfuehrung -- ALT vs. NEU ==\n");
    const char* unterminated_cases[] = {
        "\"/ext/subghz/no_close.sub",
        "\"a",
        "\"/ext/nfc/dict with space and no close",
    };
    int alt_wrongly_ok = 0;
    for(size_t i = 0; i < sizeof(unterminated_cases) / sizeof(unterminated_cases[0]); i++) {
        MiniString* args_alt = ms_alloc_set_str(unterminated_cases[i]);
        MiniString* word_alt = ms_alloc();
        int ok_alt = args_read_probably_quoted_string_and_trim_ALT(args_alt, word_alt);
        printf("  ALT in=%-40s -> ok=%d word=\"%s\" rest=\"%s\" (rest unveraendert: %s)\n",
               unterminated_cases[i], ok_alt, ms_cstr(word_alt), ms_cstr(args_alt),
               strcmp(ms_cstr(args_alt), unterminated_cases[i]) == 0 ? "ja" : "nein");
        CHECK(ok_alt == 1); // Bug: meldet faelschlich Erfolg
        CHECK(strcmp(ms_cstr(args_alt), unterminated_cases[i]) == 0); // args bleibt unveraendert (right(args,0))
        if(ok_alt) alt_wrongly_ok++;

        MiniString* args_neu = ms_alloc_set_str(unterminated_cases[i]);
        MiniString* word_neu = ms_alloc();
        int ok_neu = args_read_probably_quoted_string_and_trim_NEU(args_neu, word_neu);
        printf("  NEU in=%-40s -> ok=%d\n", unterminated_cases[i], ok_neu);
        CHECK(ok_neu == 0); // Fix: meldet korrekt Fehler

        ms_free(word_alt);
        ms_free(args_alt);
        ms_free(word_neu);
        ms_free(args_neu);
    }
    printf("  ALT meldet bei %d/%zu Faellen mit fehlender schliessender Anfuehrung faelschlich"
           " Erfolg, NEU bei 0/%zu\n",
           alt_wrongly_ok, sizeof(unterminated_cases) / sizeof(unterminated_cases[0]),
           sizeof(unterminated_cases) / sizeof(unterminated_cases[0]));

    printf("\n== Test 3: subghz-CLI-Aufrufe -- unquoted-Variante schneidet Pfade mit Leerzeichen ab ==\n");
    // Vor dem Patch riefen subghz decode_raw/tx_from_file/encrypt_keeloq/encrypt_raw
    // args_read_string_and_trim() (unquoted) statt der quoted-Variante -- ein
    // Pfad mit Leerzeichen wird dadurch am ersten Leerzeichen abgeschnitten, die
    // Anfuehrungszeichen bleiben als Teil des Dateinamens erhalten.
    {
        MiniString* args_old = ms_alloc_set_str("\"/ext/subghz/Some File.sub\" 10 0");
        MiniString* word_old = ms_alloc();
        args_read_string_and_trim(args_old, word_old); // alte subghz_cli.c-Aufrufstelle
        printf("  VORHER (subghz, unquoted-Aufruf) word=\"%s\" rest=\"%s\"\n",
               ms_cstr(word_old), ms_cstr(args_old));
        CHECK(strcmp(ms_cstr(word_old), "\"/ext/subghz/Some") == 0); // abgeschnitten, mit Anfuehrung

        MiniString* args_new = ms_alloc_set_str("\"/ext/subghz/Some File.sub\" 10 0");
        MiniString* word_new = ms_alloc();
        args_read_probably_quoted_string_and_trim_NEU(args_new, word_new); // neue Aufrufstelle
        printf("  NACHHER (subghz, quoted-Aufruf)  word=\"%s\" rest=\"%s\"\n",
               ms_cstr(word_new), ms_cstr(args_new));
        CHECK(strcmp(ms_cstr(word_new), "/ext/subghz/Some File.sub") == 0);
        CHECK(strcmp(ms_cstr(args_new), "10 0") == 0);

        ms_free(word_old);
        ms_free(args_old);
        ms_free(word_new);
        ms_free(args_new);
    }

    printf("\n%d/%d Checks bestanden\n", checks_run - checks_failed, checks_run);
    if(checks_failed == 0) {
        printf("ALLE CHECKS BESTANDEN\n");
        return 0;
    } else {
        printf("%d CHECK(S) FEHLGESCHLAGEN\n", checks_failed);
        return 1;
    }
}
