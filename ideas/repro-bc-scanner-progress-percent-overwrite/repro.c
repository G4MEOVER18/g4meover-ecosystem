#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#define FILE_BUFFER_LEN 50

/* Transcribed state fields, unchanged names/types from bc_scanner_script.h/.c
 * (BarCodeState.line_cur, BarCodeState.line_nb) and the worker loop in
 * bc_scanner_worker()/bc_script_read_file() in bc_scanner_script.c. */
typedef struct {
    uint16_t line_cur;
    uint16_t line_nb;
} State;

/* Mock storage: a file of known total size, read in FILE_BUFFER_LEN chunks. */
typedef struct {
    uint32_t total_size;
    uint32_t offset;
} MockFile;

static uint16_t mock_storage_file_read(MockFile* f, uint8_t* buf, uint16_t want) {
    uint32_t remain = f->total_size - f->offset;
    uint16_t n = (remain < want) ? (uint16_t)remain : want;
    memset(buf, 'X', n);
    f->offset += n;
    return n;
}

static int mock_storage_file_eof(MockFile* f) {
    return f->offset >= f->total_size;
}

/* ---- ALT: bc_script_read_file() exactly as in bc_scanner_script.c ---- */
static uint16_t alt_bc_script_read_file(State* st, MockFile* f, uint8_t* buf, int* is_file_end) {
    st->line_nb = 0; /* bc_scanner_script.c:101 */
    uint16_t ret = mock_storage_file_read(f, buf, FILE_BUFFER_LEN);
    if(mock_storage_file_eof(f)) {
        *is_file_end = 1;
    }
    st->line_nb += ret; /* bc_scanner_script.c:106 -- st.line_nb ends up == ret, not the file size */
    return ret;
}

/* The exact view-layer percent formula, bc_scanner_view.c:71 */
static unsigned view_percent(State* st) {
    return ((unsigned)st->line_cur * 100u) / (unsigned)st->line_nb;
}

/* ---- NEU: fix sketch -- keep the Init-time total size in line_nb, accumulate
 * cumulative bytes read so far into line_cur instead of overwriting it with
 * the size of only the most recent chunk. ---- */
static uint16_t neu_bc_script_read_file(State* st, MockFile* f, uint8_t* buf, int* is_file_end) {
    (void)st;
    uint16_t ret = mock_storage_file_read(f, buf, FILE_BUFFER_LEN);
    if(mock_storage_file_eof(f)) {
        *is_file_end = 1;
    }
    return ret; /* line_nb untouched: stays the Init-time total file size */
}

static void run_alt(uint32_t total_size, unsigned* percents, int* n_steps) {
    State st; st.line_cur = 0; st.line_nb = (uint16_t)total_size; /* Init, worker.c:139 */
    MockFile f = { total_size, 0 };
    uint8_t buf[FILE_BUFFER_LEN];
    int is_file_end = 0;
    int i = 0;
    while(!is_file_end) {
        uint16_t size = alt_bc_script_read_file(&st, &f, buf, &is_file_end);
        st.line_cur = size; /* bc_scanner_script.c worker loop: st->line_cur = size */
        percents[i++] = view_percent(&st);
    }
    *n_steps = i;
}

static void run_neu(uint32_t total_size, unsigned* percents, int* n_steps) {
    State st; st.line_cur = 0; st.line_nb = (uint16_t)total_size; /* Init, unchanged */
    MockFile f = { total_size, 0 };
    uint8_t buf[FILE_BUFFER_LEN];
    int is_file_end = 0;
    int i = 0;
    uint32_t cumulative = 0;
    while(!is_file_end) {
        uint16_t size = neu_bc_script_read_file(&st, &f, buf, &is_file_end);
        cumulative += size;
        st.line_cur = (uint16_t)cumulative; /* fix: accumulate instead of overwrite */
        percents[i++] = view_percent(&st);
    }
    *n_steps = i;
}

int main(void) {
    int checks = 0;

    /* Scenario 1: 237-byte file, 50-byte chunks -> 5 reads (50,50,50,50,37). */
    {
        unsigned alt_p[16], neu_p[16];
        int alt_n, neu_n;
        run_alt(237, alt_p, &alt_n);
        run_neu(237, neu_p, &neu_n);
        assert(alt_n == 5);
        assert(neu_n == 5);
        /* ALT: every single read reports 100%, including the very first one. */
        for(int i = 0; i < alt_n; i++) { assert(alt_p[i] == 100); }
        checks++;
        /* NEU: ramps monotonically with the real file-wide progress, ends at 100%. */
        unsigned expect[5] = { 21, 42, 63, 84, 100 }; /* round_down((i*50 or 37 cumulative)*100/237) */
        for(int i = 0; i < neu_n; i++) { assert(neu_p[i] == expect[i]); }
        checks++;
        printf("[1/6] 237-byte file: ALT reports 100%% on every one of %d reads (expected ramp), NEU ramps %u/%u/%u/%u/%u%% -- OK\n",
               alt_n, neu_p[0], neu_p[1], neu_p[2], neu_p[3], neu_p[4]);
    }

    /* Scenario 2: exact multiple of the chunk size (150 bytes, 3 full 50-byte reads). */
    {
        unsigned alt_p[16], neu_p[16];
        int alt_n, neu_n;
        run_alt(150, alt_p, &alt_n);
        run_neu(150, neu_p, &neu_n);
        assert(alt_n == 3);
        for(int i = 0; i < alt_n; i++) { assert(alt_p[i] == 100); }
        checks++;
        unsigned expect[3] = { 33, 66, 100 };
        for(int i = 0; i < neu_n; i++) { assert(neu_p[i] == expect[i]); }
        checks++;
        printf("[2/6] 150-byte file (exact chunk multiple): ALT still 100%% on every read, NEU ramps %u/%u/%u%% -- OK\n",
               neu_p[0], neu_p[1], neu_p[2]);
    }

    /* Scenario 3: file smaller than one chunk (12 bytes) -- the one case where ALT
     * happens to be correct, since the single chunk IS the whole file. Equivalence
     * case: confirms the fix does not change behaviour here. */
    {
        unsigned alt_p[16], neu_p[16];
        int alt_n, neu_n;
        run_alt(12, alt_p, &alt_n);
        run_neu(12, neu_p, &neu_n);
        assert(alt_n == 1 && neu_n == 1);
        assert(alt_p[0] == 100 && neu_p[0] == 100);
        checks++;
        printf("[3/6] 12-byte file (single short read): ALT==NEU==100%% -- OK (equivalence case)\n");
    }

    /* Scenario 4: regression -- line_cur/line_nb field layout and the view formula
     * itself (bc_scanner_view.c:71) are untouched by the fix; only what feeds them
     * changes. Re-check the formula directly against hand-picked values. */
    {
        State st;
        st.line_cur = 50; st.line_nb = 200;
        assert(view_percent(&st) == 25);
        st.line_cur = 200; st.line_nb = 200;
        assert(view_percent(&st) == 100);
        checks++;
        printf("[4/6] view_percent() formula itself unchanged, still line_cur*100/line_nb -- OK\n");
    }

    /* Scenario 5: largest size that still fits BarCodeState.line_nb/line_cur
     * (both uint16_t, so max representable file size here is 65535 bytes) --
     * confirms no overflow in the accumulation fix up to that bound. */
    {
        unsigned alt_p[2048], neu_p[2048];
        int alt_n, neu_n;
        run_alt(65500, alt_p, &alt_n);
        run_neu(65500, neu_p, &neu_n);
        assert(alt_n == neu_n);
        assert(neu_p[neu_n - 1] == 100);
        for(int i = 0; i < alt_n; i++) { assert(alt_p[i] == 100); }
        checks++;
        printf("[5/6] 65500-byte file (%d reads): ALT still flatlines at 100%% throughout, NEU ends at %u%% without overflow -- OK\n",
               alt_n, neu_p[neu_n - 1]);
    }

    /* Scenario 6: mutation probe on the repro itself -- if NEU's accumulation is
     * reverted to overwrite (cumulative = size instead of +=), line_nb (fixed at the
     * real total, 237, since this mutation keeps that part of the fix) no longer
     * matches line_cur (just the latest chunk): percent never reaches 100% and is
     * non-monotonic (it drops on the final, shorter chunk) -- a different, but
     * equally wrong, result from the one asserted above. Confirms the asserted ramp
     * in scenario 1 actually depends on the accumulation line, not on some other
     * coincidence. */
    {
        State st; st.line_cur = 0; st.line_nb = 237;
        MockFile f = { 237, 0 };
        uint8_t buf[FILE_BUFFER_LEN];
        int is_file_end = 0;
        unsigned mutated_p[16];
        int i = 0;
        while(!is_file_end) {
            uint16_t size = neu_bc_script_read_file(&st, &f, buf, &is_file_end);
            st.line_cur = size; /* mutated: overwrite instead of accumulate */
            mutated_p[i++] = view_percent(&st);
        }
        unsigned expect_mutated[5] = { 21, 21, 21, 21, 15 };
        for(int j = 0; j < i; j++) { assert(mutated_p[j] == expect_mutated[j]); }
        assert(mutated_p[i - 1] != 100); /* never reaches completion */
        checks++;
        printf("[6/6] mutation probe (revert accumulation to overwrite): %u/%u/%u/%u/%u%% -- never reaches 100%%, drops on the last chunk -- confirms the repro is sensitive to the fixed line\n",
               mutated_p[0], mutated_p[1], mutated_p[2], mutated_p[3], mutated_p[4]);
    }

    printf("\n%d/6 checks passed.\n", checks);
    return 0;
}
