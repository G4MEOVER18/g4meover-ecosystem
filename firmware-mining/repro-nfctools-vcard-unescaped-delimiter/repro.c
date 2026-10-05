/*
 * Host repro for the nfctools "Write Contact" vCard 3.0 delimiter-escaping
 * bug (applications/external/nfctools/core/nfc_tools_ndef.c,
 * nfc_tools_ndef_build(), case NdefTypeContact).
 *
 * RFC 6350 §3.4 requires that a vCard TEXT value backslash-escape '\\', ';'
 * and ',', and encode an embedded newline as "\n" -- because ';' and ','
 * are themselves the subfield/list separators inside a vCard line. The N
 * and ADR properties this app writes are built from exactly five and
 * seven semicolon-separated subfields:
 *   N:;<name>;;;                              (family;given;additional;prefix;suffix)
 *   ADR:;;<address>;;;;                       (POBox;ExtAddr;Street;Locality;Region;PostalCode;Country)
 * `nfc_tools_ndef_build()` wrote the user's free-text Name/Org/Address
 * straight into those lines with plain "%s" -- a Company field like
 * "Acme; Inc." or a contact name like "Doe;Jane" lands as a *literal*
 * delimiter, re-slicing the subfields on every vCard-reading device (the
 * whole point of a Flipper-written NFC business-card tag).
 *
 * ALT reproduces the unescaped vCard builder verbatim (the pre-fix
 * behaviour). NEU reproduces the fixed builder -- which adds exactly one
 * new static helper, `vcard_escape()`, transcribed 1:1 from the applied
 * patch (firmware-mining/nfctools-vcard-unescaped-delimiter-fix/nfc_tools_ndef_c.patch)
 * -- and calls it for all six free-text fields (Name/Org/Address/Phone/
 * Email/URL) before embedding them.
 *
 * Both builders are transcribed 1:1 from the real (resp. patched)
 * `case NdefTypeContact:` block; only the growable-string plumbing differs
 * (a plain heap buffer here instead of Flipper's FuriString, since this
 * runs on the host) -- the field order, the separators and the vcard_escape()
 * body itself are unchanged.
 *
 * A small RFC 6350-compliant splitter (`vcard_split_n_field` /
 * `vcard_split_adr_field`) parses the N:/ADR: line of the generated vCard
 * back into its subfields, honouring backslash-escaping -- this is what any
 * real vCard-reading phone/app does, and is what demonstrates the
 * corruption (ALT) vs. correct round-trip (NEU).
 *
 * Build:  gcc -std=c11 -O0 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
 * Run:    ./repro
 */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ───────────────────────── growable string (host stand-in for FuriString's
// cat_str/cat_printf used by the real code; behaviourally equivalent for
// this repro's purposes) ────────────────────────────────────────────────
typedef struct {
    char* buf;
    size_t len;
    size_t cap;
} GStr;

static void gstr_init(GStr* s) {
    s->cap = 64;
    s->buf = malloc(s->cap);
    s->buf[0] = '\0';
    s->len = 0;
}

static void gstr_cat(GStr* s, const char* add) {
    size_t add_len = strlen(add);
    if(s->len + add_len + 1 > s->cap) {
        while(s->len + add_len + 1 > s->cap) s->cap *= 2;
        s->buf = realloc(s->buf, s->cap);
    }
    memcpy(s->buf + s->len, add, add_len + 1);
    s->len += add_len;
}

static void gstr_cat_printf(GStr* s, const char* fmt, const char* arg) {
    size_t needed = strlen(fmt) + strlen(arg) + 1;
    char* tmp = malloc(needed);
    snprintf(tmp, needed, fmt, arg);
    gstr_cat(s, tmp);
    free(tmp);
}

// ───────────────────────── NEU: vcard_escape(), transcribed verbatim from
// the applied patch ───────────────────────────────────────────────────────
static void vcard_escape(const char* src, char* dest, size_t dest_size) {
    size_t j = 0;
    for(size_t i = 0; src[i] != '\0' && j + 3 < dest_size; i++) {
        char c = src[i];
        if(c == '\\' || c == ';' || c == ',') {
            dest[j++] = '\\';
            dest[j++] = c;
        } else if(c == '\n') {
            dest[j++] = '\\';
            dest[j++] = 'n';
        } else if(c == '\r') {
            continue;
        } else {
            dest[j++] = c;
        }
    }
    dest[j] = '\0';
}

// ───────────────────────── ALT: case NdefTypeContact, pre-fix (unescaped) ─
static char* build_vcard_old(
    const char* buf1_name,
    const char* buf2_org,
    const char* buf3_adr,
    const char* buf4_tel,
    const char* buf5_email,
    const char* buf6_url) {
    GStr vc;
    gstr_init(&vc);
    gstr_cat(&vc, "BEGIN:VCARD\r\nVERSION:3.0\r\n");
    gstr_cat_printf(&vc, "FN:%s\r\n", buf1_name);
    gstr_cat_printf(&vc, "N:;%s;;;\r\n", buf1_name);
    if(buf2_org[0]) gstr_cat_printf(&vc, "ORG:%s\r\n", buf2_org);
    if(buf3_adr[0]) gstr_cat_printf(&vc, "ADR:;;%s;;;;\r\n", buf3_adr);
    if(buf4_tel[0] && !(buf4_tel[0] == '+' && buf4_tel[1] == '\0'))
        gstr_cat_printf(&vc, "TEL:%s\r\n", buf4_tel);
    if(buf5_email[0]) gstr_cat_printf(&vc, "EMAIL:%s\r\n", buf5_email);
    if(buf6_url[0] && strcmp(buf6_url, "https://") != 0)
        gstr_cat_printf(&vc, "URL:%s\r\n", buf6_url);
    gstr_cat(&vc, "END:VCARD\r\n");
    char* out = vc.buf; // caller frees
    return out;
}

// ───────────────────────── NEU: case NdefTypeContact, patched ─────────────
static char* build_vcard_new(
    const char* buf1_name,
    const char* buf2_org,
    const char* buf3_adr,
    const char* buf4_tel,
    const char* buf5_email,
    const char* buf6_url) {
    GStr vc;
    gstr_init(&vc);
    gstr_cat(&vc, "BEGIN:VCARD\r\nVERSION:3.0\r\n");

    size_t name_esc_size = strlen(buf1_name) * 2 + 1;
    char* name_esc = malloc(name_esc_size);
    vcard_escape(buf1_name, name_esc, name_esc_size);
    gstr_cat_printf(&vc, "FN:%s\r\n", name_esc);
    gstr_cat_printf(&vc, "N:;%s;;;\r\n", name_esc);
    free(name_esc);

    if(buf2_org[0]) {
        size_t esc_size = strlen(buf2_org) * 2 + 1;
        char* esc = malloc(esc_size);
        vcard_escape(buf2_org, esc, esc_size);
        gstr_cat_printf(&vc, "ORG:%s\r\n", esc);
        free(esc);
    }
    if(buf3_adr[0]) {
        size_t esc_size = strlen(buf3_adr) * 2 + 1;
        char* esc = malloc(esc_size);
        vcard_escape(buf3_adr, esc, esc_size);
        gstr_cat_printf(&vc, "ADR:;;%s;;;;\r\n", esc);
        free(esc);
    }
    if(buf4_tel[0] && !(buf4_tel[0] == '+' && buf4_tel[1] == '\0')) {
        size_t esc_size = strlen(buf4_tel) * 2 + 1;
        char* esc = malloc(esc_size);
        vcard_escape(buf4_tel, esc, esc_size);
        gstr_cat_printf(&vc, "TEL:%s\r\n", esc);
        free(esc);
    }
    if(buf5_email[0]) {
        size_t esc_size = strlen(buf5_email) * 2 + 1;
        char* esc = malloc(esc_size);
        vcard_escape(buf5_email, esc, esc_size);
        gstr_cat_printf(&vc, "EMAIL:%s\r\n", esc);
        free(esc);
    }
    if(buf6_url[0] && strcmp(buf6_url, "https://") != 0) {
        size_t esc_size = strlen(buf6_url) * 2 + 1;
        char* esc = malloc(esc_size);
        vcard_escape(buf6_url, esc, esc_size);
        gstr_cat_printf(&vc, "URL:%s\r\n", esc);
        free(esc);
    }
    gstr_cat(&vc, "END:VCARD\r\n");
    char* out = vc.buf; // caller frees
    return out;
}

// ───────────────────────── RFC 6350-compliant field splitter ──────────────
// Finds the line starting with `prop` (e.g. "N:" or "ADR:") in `vcard`,
// splits the rest of that line (up to "\r\n") into up to `max_fields`
// subfields on *unescaped* ';', and unescapes each subfield in place
// ("\;" -> ";", "\," -> ",", "\\\\" -> "\\", "\n" -> newline). This is the
// behaviour any compliant vCard parser applies -- it is what turns the
// bug (a literal ';' acting as a real separator) into visible corruption.
static int vcard_split_prop(
    const char* vcard,
    const char* prop,
    char fields[][256],
    int max_fields) {
    // Anchor the match to the start of a line: "N:" is also a substring of
    // "FN:", so a bare strstr() would find the tail of the FN: line first.
    const char* search_from = vcard;
    const char* line = NULL;
    while((line = strstr(search_from, prop)) != NULL) {
        if(line == vcard || line[-1] == '\n') break;
        search_from = line + 1;
    }
    assert(line);
    const char* p = line + strlen(prop);
    const char* line_end = strstr(p, "\r\n");
    assert(line_end);

    int field_idx = 0;
    size_t w = 0;
    fields[0][0] = '\0';
    while(p < line_end && field_idx < max_fields - 1) {
        if(*p == '\\' && p + 1 < line_end) {
            // escaped char: unescape it into the current field
            char next = p[1];
            fields[field_idx][w++] = (next == 'n') ? '\n' : next;
            p += 2;
        } else if(*p == ';') {
            fields[field_idx][w] = '\0';
            field_idx++;
            w = 0;
            fields[field_idx][0] = '\0';
            p++;
        } else {
            fields[field_idx][w++] = *p++;
        }
    }
    fields[field_idx][w] = '\0';
    return field_idx + 1; // number of subfields actually produced
}

static void test_regression_no_special_chars(void) {
    char* old_vc = build_vcard_old("John Doe", "Acme Corp", "1 Main St", "+15550001", "a@b.c", "https://x.test");
    char* new_vc = build_vcard_new("John Doe", "Acme Corp", "1 Main St", "+15550001", "a@b.c", "https://x.test");
    assert(strcmp(old_vc, new_vc) == 0); // no behaviour change without special chars
    char fields[8][256];
    // N: template is ";%s;;;" -- always 4 literal separators -> 5 subfields
    // (family;given;additional;prefix;suffix) when the name itself has none.
    int n = vcard_split_prop(new_vc, "N:", fields, 8);
    assert(n == 5);
    assert(strcmp(fields[0], "") == 0); // family
    assert(strcmp(fields[1], "John Doe") == 0); // given
    assert(strcmp(fields[2], "") == 0 && strcmp(fields[3], "") == 0 && strcmp(fields[4], "") == 0);
    free(old_vc);
    free(new_vc);
    printf("OK:   Regression ohne Sonderzeichen: ALT und NEU erzeugen byte-identisches vCard, N: parst zu 5 Feldern family=\"\" given=\"John Doe\" additional/prefix/suffix=\"\".\n");
}

static void test_name_semicolon_corrupts_old_fixes_new(void) {
    const char* name = "Doe;Jane";
    char* old_vc = build_vcard_old(name, "", "", "", "", "");
    char* new_vc = build_vcard_new(name, "", "", "", "", "");

    char old_fields[8][256];
    int old_n = vcard_split_prop(old_vc, "N:", old_fields, 8);
    // CORRUPTED: the literal ';' in the name adds a 6th subfield instead of
    // the standard 5 -- "Doe" lands in "given", "Jane" leaks into "additional".
    assert(old_n == 6);
    assert(strcmp(old_fields[1], "Doe") == 0);
    assert(strcmp(old_fields[2], "Jane") == 0);

    char new_fields[8][256];
    int new_n = vcard_split_prop(new_vc, "N:", new_fields, 8);
    assert(new_n == 5); // standard 5 subfields, given="Doe;Jane" intact -- correct round-trip
    assert(strcmp(new_fields[1], name) == 0);
    assert(strcmp(new_fields[2], "") == 0); // additional stays empty, not shifted into

    free(old_vc);
    free(new_vc);
    printf("OK:   Name mit ';' (\"Doe;Jane\"): ALT zerlegt N: in 6 statt 5 Feldern (given=\"Doe\" additional=\"Jane\", Name verstuemmelt) -- NEU erhaelt den Namen unversehrt als einziges given-Feld, 5 Felder wie vom Standard vorgesehen.\n");
}

static void test_name_comma_and_backslash_roundtrip(void) {
    const char* name = "O'Brien, Patrick \\Pat\\";
    char* new_vc = build_vcard_new(name, "", "", "", "", "");
    char fields[8][256];
    int n = vcard_split_prop(new_vc, "N:", fields, 8);
    assert(n == 5); // comma and backslash are escaped, not treated as separators
    assert(strcmp(fields[1], name) == 0);
    free(new_vc);
    printf("OK:   Name mit ',' und '\\\\' (\"O'Brien, Patrick \\\\Pat\\\\\"): NEU liefert ihn nach dem Rundlauf unveraendert zurueck, weiterhin 5 N:-Felder.\n");
}

static void test_adr_semicolon_shifts_subfields_old_fixes_new(void) {
    const char* street = "123 Main St; Suite 4";
    char* old_vc = build_vcard_old("X", "", street, "", "", "");
    char* new_vc = build_vcard_new("X", "", street, "", "", "");

    char old_fields[8][256];
    int old_n = vcard_split_prop(old_vc, "ADR:", old_fields, 8);
    // ADR has 7 subfields (POBox;Ext;Street;Locality;Region;PostalCode;Country).
    // The unescaped ';' inside the street value injects an 8th subfield and
    // shifts Locality/Region/PostalCode/Country one slot to the right.
    assert(old_n == 8);
    assert(strcmp(old_fields[2], "123 Main St") == 0);
    assert(strcmp(old_fields[3], " Suite 4") == 0); // leaked into "Locality"

    char new_fields[8][256];
    int new_n = vcard_split_prop(new_vc, "ADR:", new_fields, 8);
    assert(new_n == 7); // exactly POBox;Ext;Street;Locality;Region;PostalCode;Country
    assert(strcmp(new_fields[2], street) == 0); // whole street value intact in its own slot
    assert(strcmp(new_fields[3], "") == 0); // Locality slot still empty, not shifted into

    free(old_vc);
    free(new_vc);
    printf("OK:   Adresse mit ';' (\"123 Main St; Suite 4\"): ALT erzeugt 8 statt 7 ADR-Teilfelder (Ort-Feld wird mit \" Suite 4\" ueberschrieben) -- NEU haelt exakt 7 Teilfelder, Strasse unversehrt in ihrem eigenen Slot.\n");
}

static void test_org_semicolon_escaped(void) {
    const char* org = "Acme; Inc.";
    char* new_vc = build_vcard_new("X", org, "", "", "", "");
    assert(strstr(new_vc, "ORG:Acme\\; Inc.\r\n") != NULL);
    char* old_vc = build_vcard_old("X", org, "", "", "", "");
    assert(strstr(old_vc, "ORG:Acme; Inc.\r\n") != NULL); // literal, unescaped (pre-fix)
    free(new_vc);
    free(old_vc);
    printf("OK:   Firma mit ';' (\"Acme; Inc.\"): ALT schreibt sie unescaped, NEU escaped sie als \"Acme\\\\; Inc.\".\n");
}

static void test_optional_fields_still_omitted_when_empty(void) {
    char* new_vc = build_vcard_new("X", "", "", "", "", "");
    assert(strstr(new_vc, "ORG:") == NULL);
    assert(strstr(new_vc, "ADR:") == NULL);
    assert(strstr(new_vc, "TEL:") == NULL);
    assert(strstr(new_vc, "EMAIL:") == NULL);
    assert(strstr(new_vc, "URL:") == NULL);
    free(new_vc);
    printf("OK:   Leere optionale Felder (Org/Adresse/Telefon/E-Mail/URL) bleiben unveraendert ausgelassen.\n");
}

static void test_phone_plus_sentinel_and_url_https_sentinel_suppressed(void) {
    char* new_vc = build_vcard_new("X", "", "", "+", "", "https://");
    assert(strstr(new_vc, "TEL:") == NULL); // bare "+" sentinel (untouched phone field)
    assert(strstr(new_vc, "URL:") == NULL); // bare "https://" sentinel (untouched url field)
    free(new_vc);
    printf("OK:   Unveraenderte Sentinel-Werte (\"+\" bei Telefon, \"https://\" bei URL) werden weiterhin unterdrueckt.\n");
}

static void test_tel_email_url_also_escaped(void) {
    // TEL/EMAIL/URL rarely contain delimiter characters in practice, but the
    // fix escapes them uniformly for RFC 6350 correctness and symmetry.
    char* new_vc = build_vcard_new("X", "", "", "+1;555", "a;b@c.com", "https://x;y.test");
    assert(strstr(new_vc, "TEL:+1\\;555\r\n") != NULL);
    assert(strstr(new_vc, "EMAIL:a\\;b@c.com\r\n") != NULL);
    assert(strstr(new_vc, "URL:https://x\\;y.test\r\n") != NULL);
    free(new_vc);
    printf("OK:   Telefon/E-Mail/URL mit ';' werden ebenfalls escaped (Symmetrie, RFC 6350).\n");
}

static void test_empty_name_edge_case(void) {
    // write_contact_name's keyboard scene enforces minimum_length=1, so an
    // empty name cannot reach this code in the real app -- checked anyway
    // as a defensive edge case for vcard_escape() itself (malloc(0*2+1)==malloc(1)).
    char* old_vc = build_vcard_old("", "", "", "", "", "");
    char* new_vc = build_vcard_new("", "", "", "", "", "");
    assert(strcmp(old_vc, new_vc) == 0);
    assert(strstr(new_vc, "FN:\r\n") != NULL);
    assert(strstr(new_vc, "N:;;;;\r\n") != NULL);
    free(old_vc);
    free(new_vc);
    printf("OK:   Randfall leerer Name (in der echten App durch minimum_length=1 ausgeschlossen): ALT und NEU bleiben byte-identisch.\n");
}

int main(void) {
    test_regression_no_special_chars();
    test_name_semicolon_corrupts_old_fixes_new();
    test_name_comma_and_backslash_roundtrip();
    test_adr_semicolon_shifts_subfields_old_fixes_new();
    test_org_semicolon_escaped();
    test_optional_fields_still_omitted_when_empty();
    test_phone_plus_sentinel_and_url_https_sentinel_suppressed();
    test_tel_email_url_also_escaped();
    test_empty_name_edge_case();
    printf("Alle Szenarien bestaetigt (exit code 0)\n");
    return 0;
}
