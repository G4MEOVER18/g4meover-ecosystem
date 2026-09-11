#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// OLD: byte-for-byte copy of nfc_tools_ndef.c's url_encode() as found upstream.
static void url_encode_old(const char* src, char* dest, size_t dest_size) {
    static const char hex[] = "0123456789ABCDEF";
    size_t j = 0;
    for(size_t i = 0; src[i] != '\0' && j + 4 < dest_size; i++) {
        unsigned char c = (unsigned char)src[i];
        if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.' || c == '~') {
            dest[j++] = (char)c;
        } else {
            dest[j++] = '%';
            dest[j++] = hex[(c >> 4) & 0x0F];
            dest[j++] = hex[c & 0x0F];
        }
    }
    dest[j] = '\0';
}

// NEW: one-character fix (`<` -> `<=`), same structure otherwise.
static void url_encode_new(const char* src, char* dest, size_t dest_size) {
    static const char hex[] = "0123456789ABCDEF";
    size_t j = 0;
    for(size_t i = 0; src[i] != '\0' && j + 4 <= dest_size; i++) {
        unsigned char c = (unsigned char)src[i];
        if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.' || c == '~') {
            dest[j++] = (char)c;
        } else {
            dest[j++] = '%';
            dest[j++] = hex[(c >> 4) & 0x0F];
            dest[j++] = hex[c & 0x0F];
        }
    }
    dest[j] = '\0';
}

// Independent reference: standard RFC 3986 unreserved-set percent-encoder,
// written without reusing any of the loop-bound logic under test.
static void url_encode_reference(const char* src, char* out) {
    static const char hex[] = "0123456789ABCDEF";
    size_t j = 0;
    for(size_t i = 0; src[i]; i++) {
        unsigned char c = (unsigned char)src[i];
        if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out[j++] = (char)c;
        } else {
            out[j++] = '%';
            out[j++] = hex[(c >> 4) & 0x0F];
            out[j++] = hex[c & 0x0F];
        }
    }
    out[j] = '\0';
}

static void run_case(const char* label, const char* input) {
    size_t raw_len = strlen(input);
    size_t dest_size = raw_len * 3 + 1; // exactly how both call sites in nfc_tools_ndef.c size the buffer

    char* ref = malloc(raw_len * 3 + 1);
    url_encode_reference(input, ref);

    char* old_buf = malloc(dest_size);
    memset(old_buf, 0xAA, dest_size);
    url_encode_old(input, old_buf, dest_size);

    char* new_buf = malloc(dest_size);
    memset(new_buf, 0xAA, dest_size);
    url_encode_new(input, new_buf, dest_size);

    int old_matches = (strcmp(old_buf, ref) == 0);
    int new_matches = (strcmp(new_buf, ref) == 0);

    printf("[%-28s] input=%-14s ref=%-20s ALT=%-20s (%s)  NEU=%-20s (%s)\n",
           label, input, ref, old_buf, old_matches ? "match" : "TRUNCATED",
           new_buf, new_matches ? "match" : "MISMATCH");

    free(ref); free(old_buf); free(new_buf);
}

int main(void) {
    // Regression anchors: pure alnum input never needs escaping, must be
    // byte-identical before/after in every case.
    run_case("regression: empty",        "");
    run_case("regression: alnum only",   "SatoshiNakamoto123");
    run_case("regression: single alnum", "A");

    // Bug-triggering cases: any '/'/':'/'?'/'&'/space/'!' etc. is escaped
    // to 3 bytes, exactly the worst case the buffer is sized for.
    run_case("BUG: single escaped char", "!");
    run_case("BUG: two escaped chars",   "!!");
    run_case("BUG: escaped + alnum tail","!A");     // mixed case: even the *unescaped* tail char is dropped
    run_case("BUG: bitcoin message",     "for coffee!"); // realistic ?message= value (space + '!')
    run_case("BUG: search keyword",      "C&A shop"); // realistic search keyword ('&' + space)
    run_case("BUG: all escaped, longer", "!!!!!");

    return 0;
}
