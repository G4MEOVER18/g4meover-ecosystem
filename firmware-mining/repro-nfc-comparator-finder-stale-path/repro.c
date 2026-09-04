/*
 * Host repro for the NFC Comparator "Digital Finder Search" stale-path bug.
 *
 * Reduces nfc_comparator_finder_searcher_worker_task() (lib/finder_searcher_worker/
 * nfc_comparator_finder_searcher_worker.c) to its control flow: every directory
 * entry is written straight into the shared compare-worker's nfc_card_path via
 * dir_walk_read(), *before* the extension/load checks run, while the match
 * results (uid/uid_length/protocol) and diff counters are only updated for
 * entries that are an ".nfc" file AND load successfully. Any trailing entry
 * that fails either check (wrong extension, or an ".nfc" file that fails to
 * load) overwrites the path but leaves the previous entry's still-live match
 * data in place -- decoupling the displayed filename from the data it
 * describes.
 *
 * Build:  gcc -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
 * Run:    ./repro
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char path[64];
    bool uid, uid_length, protocol;
    uint16_t diff_count, diff_total;
} CompareWorker;

typedef struct {
    const char* name;
    bool is_nfc_ext;
    bool load_ok;
    bool shallow_match; /* uid && uid_length && protocol, if loaded */
    uint16_t diff_count;
    uint16_t diff_total;
} DirEntry;

static void cw_reset(CompareWorker* cw) {
    cw->path[0] = '\0';
    cw->uid = cw->uid_length = cw->protocol = false;
    cw->diff_count = cw->diff_total = 0;
}

static void cw_copy(CompareWorker* dst, const CompareWorker* src) {
    *dst = *src;
}

/* ALT: verbatim control flow of nfc_comparator_finder_searcher_worker_task().
 * dir_walk_read() writes the current entry's name directly into
 * worker->compare_worker->nfc_card_path on every iteration, unconditionally. */
static void search_task_old(const DirEntry* entries, size_t n, CompareWorker* cw) {
    CompareWorker tmp;
    cw_reset(&tmp);

    for(size_t i = 0; i < n; i++) {
        /* dir_walk_read(worker->dir_walk, worker->compare_worker->nfc_card_path, NULL) */
        strncpy(cw->path, entries[i].name, sizeof(cw->path) - 1);
        cw->path[sizeof(cw->path) - 1] = '\0';

        if(!entries[i].is_nfc_ext) continue;
        if(!entries[i].load_ok) continue;

        /* nfc_comparator_compare_worker_compare_cards(cw, card1, card2) */
        cw->uid = cw->uid_length = cw->protocol = entries[i].shallow_match;

        if(cw->uid && cw->uid_length && cw->protocol) {
            cw->diff_count = entries[i].diff_count;
            cw->diff_total = entries[i].diff_total;
            if(cw->diff_count == 0) {
                break; /* perfect match */
            } else if(tmp.path[0] == '\0' || tmp.diff_count > cw->diff_count) {
                cw_copy(&tmp, cw);
            }
        } else {
            cw_reset(cw);
        }
    }

    /* final result selection */
    if(tmp.path[0] != '\0' && (cw->path[0] == '\0' || tmp.diff_count < cw->diff_count)) {
        cw_copy(cw, &tmp);
    }
}

/* NEW: dir_walk_read()'s target becomes a loop-local candidate_path; the
 * worker's own nfc_card_path is only assigned together with results/diff,
 * at the point a comparison actually runs -- so path and data can never
 * drift apart. */
static void search_task_new(const DirEntry* entries, size_t n, CompareWorker* cw) {
    CompareWorker tmp;
    cw_reset(&tmp);
    char candidate_path[64];

    for(size_t i = 0; i < n; i++) {
        /* dir_walk_read(worker->dir_walk, candidate_path, NULL) */
        strncpy(candidate_path, entries[i].name, sizeof(candidate_path) - 1);
        candidate_path[sizeof(candidate_path) - 1] = '\0';

        if(!entries[i].is_nfc_ext) continue;
        if(!entries[i].load_ok) continue;

        /* furi_string_set(cw->nfc_card_path, candidate_path); -- moved here */
        strncpy(cw->path, candidate_path, sizeof(cw->path) - 1);
        cw->path[sizeof(cw->path) - 1] = '\0';

        cw->uid = cw->uid_length = cw->protocol = entries[i].shallow_match;

        if(cw->uid && cw->uid_length && cw->protocol) {
            cw->diff_count = entries[i].diff_count;
            cw->diff_total = entries[i].diff_total;
            if(cw->diff_count == 0) {
                break;
            } else if(tmp.path[0] == '\0' || tmp.diff_count > cw->diff_count) {
                cw_copy(&tmp, cw);
            }
        } else {
            cw_reset(cw);
        }
    }

    if(tmp.path[0] != '\0' && (cw->path[0] == '\0' || tmp.diff_count < cw->diff_count)) {
        cw_copy(cw, &tmp);
    }
}

static int run_case(
    const char* label,
    const DirEntry* entries,
    size_t n,
    const char* expect_path,
    uint16_t expect_diff_count) {
    CompareWorker old_cw, new_cw;
    cw_reset(&old_cw);
    cw_reset(&new_cw);

    search_task_old(entries, n, &old_cw);
    search_task_new(entries, n, &new_cw);

    printf("== %s ==\n", label);
    printf("  OLD: path=\"%s\" diff=%u/%u\n", old_cw.path, old_cw.diff_count, old_cw.diff_total);
    printf("  NEW: path=\"%s\" diff=%u/%u\n", new_cw.path, new_cw.diff_count, new_cw.diff_total);

    bool old_wrong =
        strcmp(old_cw.path, expect_path) != 0 || old_cw.diff_count != expect_diff_count;
    bool new_correct =
        strcmp(new_cw.path, expect_path) == 0 && new_cw.diff_count == expect_diff_count;

    printf("  expected: path=\"%s\" diff_count=%u -> OLD %s, NEW %s\n\n",
           expect_path,
           expect_diff_count,
           old_wrong ? "MISMATCH (bug)" : "matches",
           new_correct ? "matches" : "MISMATCH");

    return (old_wrong && new_correct) ? 1 : 0;
}

int main(void) {
    int confirmed = 0;

    /* Case 1: a genuine partial match ("access_card_A.nfc") is immediately
     * followed, later in directory-walk order, by an unrelated non-.nfc file
     * ("readme.txt"). Nothing after the match ever satisfies the extension
     * check, so nothing resets or updates results/diff again -- only the
     * path field gets clobbered. */
    DirEntry case1[] = {
        {"access_card_A.nfc", true, true, true, 5, 200},
        {"readme.txt", false, false, false, 0, 0},
    };
    confirmed += run_case("trailing non-.nfc file after a real partial match",
                          case1,
                          sizeof(case1) / sizeof(case1[0]),
                          "access_card_A.nfc",
                          5);

    /* Case 2: same, but the trailing entry has the right extension yet fails
     * to load (corrupted/foreign-format .nfc file left in the folder) --
     * dir_walk_read() still overwrites the path before the load check runs. */
    DirEntry case2[] = {
        {"access_card_B.nfc", true, true, true, 12, 200},
        {"corrupted.nfc", true, false, false, 0, 0},
    };
    confirmed += run_case("trailing unloadable .nfc file after a real partial match",
                          case2,
                          sizeof(case2) / sizeof(case2[0]),
                          "access_card_B.nfc",
                          12);

    /* Regression check: normal case, best match is also the last entry
     * examined -- OLD and NEW must agree (no behavior change on the
     * already-correct path). */
    DirEntry case3[] = {
        {"other.nfc", true, true, false, 0, 0},
        {"best_match.nfc", true, true, true, 3, 200},
    };
    CompareWorker old_cw, new_cw;
    cw_reset(&old_cw);
    cw_reset(&new_cw);
    search_task_old(case3, 2, &old_cw);
    search_task_new(case3, 2, &new_cw);
    printf("== regression: best match is the last entry examined ==\n");
    printf("  OLD: path=\"%s\" diff=%u/%u\n", old_cw.path, old_cw.diff_count, old_cw.diff_total);
    printf("  NEW: path=\"%s\" diff=%u/%u\n", new_cw.path, new_cw.diff_count, new_cw.diff_total);
    bool regression_ok = strcmp(old_cw.path, new_cw.path) == 0 &&
                          old_cw.diff_count == new_cw.diff_count &&
                          strcmp(new_cw.path, "best_match.nfc") == 0;
    printf("  OLD == NEW, both \"best_match.nfc\": %s\n\n", regression_ok ? "yes" : "NO");

    if(confirmed == 2 && regression_ok) {
        printf("CONFIRMED: OLD reports the wrong filename (last directory entry seen) "
               "for a real partial match whenever a non-matching/unloadable entry "
               "follows it in the walk order, while still showing that match's real "
               "diff count -- a misleading result on an access-card comparison. NEW "
               "keeps path and diff data in sync in all three cases.\n");
        return 0;
    }
    printf("UNEXPECTED: repro did not reproduce the expected OLD/NEW split.\n");
    return 1;
}
