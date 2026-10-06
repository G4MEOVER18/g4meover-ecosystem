/* Host repro for the spi_flash_dump hex viewer Left/Right page-scroll fund.
 *
 * Transcribes hex_viewer.c's scroll_offset arithmetic 1:1 (ALT = shipped
 * behaviour, only Up/Down wired; NEU = this patch, adds Left/Right as a
 * ROWS_ON_SCREEN-row page jump, same unit the existing scrollbar already
 * uses). No Flipper/Furi headers needed: scroll_offset is pure uint32_t
 * arithmetic independent of the GUI/storage layer.
 *
 * Scenarios:
 *  1. Regression: Up/Down behave identically in ALT and NEU.
 *  2. Fund: ALT never reacts to Left/Right (input_cb returns false,
 *     scroll_offset unchanged) for a multi-page file.
 *  3. Fix: NEU's Right advances by one page, clamped to max_offset.
 *  4. Fix: NEU's Left retreats by one page, clamped to 0.
 *  5. Edge: file that fits on one screen (total_rows <= ROWS_ON_SCREEN) ->
 *     Left/Right are no-ops in NEU too (nothing to scroll).
 *  6. Edge: full HEX_PREVIEW_SIZE (4096 bytes -> 1024 rows) walked with
 *     repeated Right presses lands exactly on max_offset and stays there.
 *  7. Equivalence: for every data_len in a representative sample, NEU's
 *     Up/Down matches ALT's Up/Down bit-for-bit across many presses.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define BYTES_PER_ROW  4
#define ROWS_ON_SCREEN 6

typedef enum { KeyUp, KeyDown, KeyLeft, KeyRight, KeyOk, KeyBack } Key;

typedef struct {
    uint32_t data_len;
    uint32_t scroll_offset;
} Model;

static uint32_t total_rows_of(const Model* m) {
    return (m->data_len + BYTES_PER_ROW - 1) / BYTES_PER_ROW;
}

/* ALT: shipped hex_viewer_input_cb -- only Up/Down wired. */
static bool alt_input_cb(Model* m, Key key) {
    if(key == KeyUp) {
        if(m->scroll_offset > 0) m->scroll_offset--;
        return true;
    }
    if(key == KeyDown) {
        uint32_t total_rows = total_rows_of(m);
        if(total_rows > ROWS_ON_SCREEN && m->scroll_offset < total_rows - ROWS_ON_SCREEN) {
            m->scroll_offset++;
        }
        return true;
    }
    return false;
}

/* NEU: this patch -- adds Left/Right as a one-page jump. */
static bool neu_input_cb(Model* m, Key key) {
    if(key == KeyUp) {
        if(m->scroll_offset > 0) m->scroll_offset--;
        return true;
    }
    if(key == KeyDown) {
        uint32_t total_rows = total_rows_of(m);
        if(total_rows > ROWS_ON_SCREEN && m->scroll_offset < total_rows - ROWS_ON_SCREEN) {
            m->scroll_offset++;
        }
        return true;
    }
    if(key == KeyLeft) {
        if(m->scroll_offset > ROWS_ON_SCREEN) {
            m->scroll_offset -= ROWS_ON_SCREEN;
        } else {
            m->scroll_offset = 0;
        }
        return true;
    }
    if(key == KeyRight) {
        uint32_t total_rows = total_rows_of(m);
        if(total_rows > ROWS_ON_SCREEN) {
            uint32_t max_offset = total_rows - ROWS_ON_SCREEN;
            if(m->scroll_offset + ROWS_ON_SCREEN < max_offset) {
                m->scroll_offset += ROWS_ON_SCREEN;
            } else {
                m->scroll_offset = max_offset;
            }
        }
        return true;
    }
    return false;
}

static int checks_passed = 0;
/* Evaluates `cond` exactly once -- several call sites pass side-effecting
   expressions (the input_cb calls themselves), and the obvious
   assert(cond); if(!(cond)) {...} pattern would silently double-fire them. */
#define CHECK(cond)                                                           \
    do {                                                                      \
        bool _check_ok = (cond);                                              \
        if(!_check_ok) {                                                      \
            fprintf(stderr, "CHECK FAILED: %s (line %d)\n", #cond, __LINE__); \
            exit(1);                                                          \
        }                                                                     \
        assert(_check_ok);                                                    \
        checks_passed++;                                                      \
    } while(0)

int main(void) {
    /* 1. Regression: Up/Down identical between ALT and NEU over many data_len
          values and many presses in both directions. */
    for(uint32_t data_len = 0; data_len <= 4096; data_len += 37) {
        Model alt = {data_len, 0};
        Model neu = {data_len, 0};
        Key seq[] = {KeyDown, KeyDown, KeyDown, KeyUp, KeyDown, KeyUp, KeyUp, KeyUp, KeyDown};
        for(size_t i = 0; i < sizeof(seq) / sizeof(seq[0]); i++) {
            bool a = alt_input_cb(&alt, seq[i]);
            bool n = neu_input_cb(&neu, seq[i]);
            CHECK(a == n);
            CHECK(alt.scroll_offset == neu.scroll_offset);
        }
    }

    /* 2. Fund: ALT never reacts to Left/Right on a multi-page file
          (1024 rows from a full 4096-byte preview). */
    {
        Model alt = {4096, 500};
        bool handled_left = alt_input_cb(&alt, KeyLeft);
        uint32_t after_left = alt.scroll_offset;
        bool handled_right = alt_input_cb(&alt, KeyRight);
        uint32_t after_right = alt.scroll_offset;
        CHECK(handled_left == false);
        CHECK(handled_right == false);
        CHECK(after_left == 500);
        CHECK(after_right == 500);
    }

    /* 3. Fix: NEU's Right advances exactly one page (ROWS_ON_SCREEN rows),
          clamped to max_offset near the end. */
    {
        uint32_t total_rows = total_rows_of(&(Model){4096, 0});
        uint32_t max_offset = total_rows - ROWS_ON_SCREEN; /* 1018 */
        Model neu = {4096, 0};
        CHECK(neu_input_cb(&neu, KeyRight) == true);
        CHECK(neu.scroll_offset == ROWS_ON_SCREEN);
        neu.scroll_offset = max_offset - 2;
        neu_input_cb(&neu, KeyRight);
        CHECK(neu.scroll_offset == max_offset); /* clamped, not overshot */
        neu_input_cb(&neu, KeyRight);
        CHECK(neu.scroll_offset == max_offset); /* stays clamped */
    }

    /* 4. Fix: NEU's Left retreats exactly one page, clamped to 0. */
    {
        Model neu = {4096, 1018};
        neu_input_cb(&neu, KeyLeft);
        CHECK(neu.scroll_offset == 1012);
        neu.scroll_offset = 4;
        neu_input_cb(&neu, KeyLeft);
        CHECK(neu.scroll_offset == 0); /* clamped, not underflowed */
        neu_input_cb(&neu, KeyLeft);
        CHECK(neu.scroll_offset == 0);
    }

    /* 5. Edge: file fits on one screen (total_rows <= ROWS_ON_SCREEN) ->
          Left/Right handled (return true, same as Up/Down) but are no-ops. */
    for(uint32_t data_len = 0; data_len <= BYTES_PER_ROW * ROWS_ON_SCREEN; data_len++) {
        Model neu = {data_len, 0};
        bool r = neu_input_cb(&neu, KeyRight);
        CHECK(r == true);
        CHECK(neu.scroll_offset == 0);
        bool l = neu_input_cb(&neu, KeyLeft);
        CHECK(l == true);
        CHECK(neu.scroll_offset == 0);
    }

    /* 6. Edge: walking the full 4096-byte preview with repeated Right
          presses lands exactly on max_offset and never exceeds it. */
    {
        Model neu = {4096, 0};
        uint32_t total_rows = total_rows_of(&neu);
        uint32_t max_offset = total_rows - ROWS_ON_SCREEN;
        for(int i = 0; i < 1000; i++) {
            neu_input_cb(&neu, KeyRight);
            CHECK(neu.scroll_offset <= max_offset);
        }
        CHECK(neu.scroll_offset == max_offset);
    }

    /* 7. Equivalence: unhandled keys (Ok/Back) are rejected by both ALT and
          NEU identically -- the fix only adds Left/Right, nothing else. */
    for(uint32_t data_len = 0; data_len <= 4096; data_len += 211) {
        Model alt = {data_len, 0};
        Model neu = {data_len, 0};
        CHECK(alt_input_cb(&alt, KeyOk) == neu_input_cb(&neu, KeyOk));
        CHECK(alt_input_cb(&alt, KeyBack) == neu_input_cb(&neu, KeyBack));
    }

    printf("checks_passed=%d\n", checks_passed);
    printf("ALLE CHECKS BESTANDEN\n");
    return 0;
}
