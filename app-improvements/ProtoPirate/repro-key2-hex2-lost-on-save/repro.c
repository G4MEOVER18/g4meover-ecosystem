// Host-repro: protopirate_storage_copy_key_2() in helpers/protopirate_storage.c versucht beim
// Speichern eines Captures (Verlauf UND normale "Save"-Funktion, beide laufen ueber
// protopirate_storage_write_capture_data()) das Feld "Key_2" nur in drei Formaten zu lesen:
// 8-Byte-Hex (ford_v1.c), String (psa.c) und u32 (kia_v6.c/honda_v1.c). chrysler_v0.c schreibt
// "Key_2" jedoch als 2-Byte-Hex -- dieses vierte reale Format wird von keinem der drei Versuche
// erfasst, der Rueckgabewert bleibt "true" (kein Fehler, das Feld gilt als "nicht vorhanden"),
// und das Feld fehlt in der gespeicherten Datei komplett.
//
// Wirkung: subghz_protocol_encoder_chrysler_v0_deserialize() (protocols/chrysler_v0.c:371)
// verlangt "Key_2" zwingend und bricht ohne dieses Feld sofort mit SubGhzProtocolStatusError
// ab -- jedes ueber ProtoPirate gespeicherte (Verlauf-Bookmark oder manuelles "Save") Chrysler-
// V0-Capture ist dadurch NICHT erneut ladbar/emulierbar, obwohl das Capture selbst korrekt war.
//
// ALT/NEU unten sind protopirate_storage_copy_hex_fixed()/copy_string_optional()/
// copy_u32_optional()/copy_key_2() 1:1 aus helpers/protopirate_storage.c uebernommen (nur
// FlipperFormat* durch ein minimales getyptes Feld-Modell ersetzt, das exakt die vier real
// vorkommenden "Key_2"-Schreibformen abbildet: chrysler_v0.c hex/2, ford_v1.c hex/8, psa.c
// string, kia_v6.c/honda_v1.c u32). Modell-Read-Funktionen liefern nur dann true, wenn Typ UND
// angefragte Laenge zum gespeicherten Feld passen -- exakt das Verhalten von
// flipper_format_read_hex()/read_string()/read_uint32() bei Typ-/Laengen-Mismatch (Ablehnung,
// kein Crash).
//
// Build & run:
//   gcc -O1 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro
//
// Erwartete Ausgabe: fuer alle vier Formate zeigt ALT, ob "Key_2" uebernommen wird; nur der
// chrysler_v0-Fall (hex/2) schlaegt unter ALT fehl. Unter NEU gelingt die Uebernahme in allen
// vier Faellen.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ---- Minimales Feld-Modell: bildet genau die vier real vorkommenden Formen ab, in denen
//      "Key_2" von ProtoPirate-Protokollen geschrieben wird ----

typedef enum { FIELD_NONE, FIELD_HEX, FIELD_STRING, FIELD_U32 } FieldType;

typedef struct {
    FieldType type;
    uint8_t hex_data[8];
    size_t hex_len;
    char string_data[32];
    uint32_t u32_data;
} FakeField;

typedef struct {
    FakeField key_2_src; // Quelle: so, wie das jeweilige Protokoll "Key_2" geschrieben hat
    FakeField key_2_dst; // Ziel: was protopirate_storage_write_capture_data() tatsaechlich speichert
} FakeFormat;

static bool fake_read_hex(FakeField* src, uint8_t* out, size_t len) {
    if(src->type != FIELD_HEX || src->hex_len != len) return false;
    memcpy(out, src->hex_data, len);
    return true;
}

static bool fake_read_string(FakeField* src, char* out, size_t out_size) {
    if(src->type != FIELD_STRING) return false;
    snprintf(out, out_size, "%s", src->string_data);
    return true;
}

static bool fake_read_u32(FakeField* src, uint32_t* out) {
    if(src->type != FIELD_U32) return false;
    *out = src->u32_data;
    return true;
}

static void fake_write_hex(FakeField* dst, const uint8_t* data, size_t len) {
    dst->type = FIELD_HEX;
    dst->hex_len = len;
    memcpy(dst->hex_data, data, len);
}

static void fake_write_string(FakeField* dst, const char* str) {
    dst->type = FIELD_STRING;
    snprintf(dst->string_data, sizeof(dst->string_data), "%s", str);
}

static void fake_write_u32(FakeField* dst, uint32_t value) {
    dst->type = FIELD_U32;
    dst->u32_data = value;
}

// ---- ALT: helpers/protopirate_storage.c, Stand vor diesem Fix (unveraendert in der Logik) ----

static bool old_copy_hex_fixed(FakeFormat* ff, size_t len, bool* copied) {
    uint8_t data[8];
    if(copied) *copied = false;
    if(!fake_read_hex(&ff->key_2_src, data, len)) return true; // "nicht vorhanden", kein Fehler
    if(copied) *copied = true;
    fake_write_hex(&ff->key_2_dst, data, len);
    return true;
}

static bool old_copy_string_optional(FakeFormat* ff) {
    char value[32];
    if(!fake_read_string(&ff->key_2_src, value, sizeof(value))) return true;
    fake_write_string(&ff->key_2_dst, value);
    return true;
}

static bool old_copy_u32_optional(FakeFormat* ff) {
    uint32_t value = 0;
    if(!fake_read_u32(&ff->key_2_src, &value)) return true;
    fake_write_u32(&ff->key_2_dst, value);
    return true;
}

// protopirate_storage_copy_key_2(), ALT
static bool old_copy_key_2(FakeFormat* ff) {
    bool copied = false;
    if(!old_copy_hex_fixed(ff, 8, &copied)) return false;
    if(copied) return true;
    return old_copy_string_optional(ff) && old_copy_u32_optional(ff);
}

// ---- NEU: mit hex/2-Versuch zwischen hex/8 und string/u32 eingefuegt ----

static bool new_copy_key_2(FakeFormat* ff) {
    bool copied = false;
    if(!old_copy_hex_fixed(ff, 8, &copied)) return false;
    if(copied) return true;
    if(!old_copy_hex_fixed(ff, 2, &copied)) return false;
    if(copied) return true;
    return old_copy_string_optional(ff) && old_copy_u32_optional(ff);
}

// ---- Testfaelle: die vier real vorkommenden "Key_2"-Schreibformen ----

typedef struct {
    const char* label;
    void (*prepare)(FakeField* src);
} Case;

static void prep_chrysler_v0(FakeField* src) {
    // protocols/chrysler_v0.c:371/472 -- 2-Byte-Hex
    src->type = FIELD_HEX;
    src->hex_len = 2;
    src->hex_data[0] = 0xAB;
    src->hex_data[1] = 0xCD;
}

static void prep_ford_v1(FakeField* src) {
    // protocols/ford_v1.c:781 -- 8-Byte-Hex
    src->type = FIELD_HEX;
    src->hex_len = 8;
    for(size_t i = 0; i < 8; i++) src->hex_data[i] = (uint8_t)(0x10 + i);
}

static void prep_psa(FakeField* src) {
    // protocols/psa.c:1301 -- String
    src->type = FIELD_STRING;
    snprintf(src->string_data, sizeof(src->string_data), "1122334455667788");
}

static void prep_kia_v6(FakeField* src) {
    // protocols/kia_v6.c:711 -- u32
    src->type = FIELD_U32;
    src->u32_data = 0xDEADBEEFu;
}

static bool field_equal(const FakeField* a, const FakeField* b) {
    if(a->type != b->type) return false;
    switch(a->type) {
    case FIELD_HEX:
        return a->hex_len == b->hex_len && memcmp(a->hex_data, b->hex_data, a->hex_len) == 0;
    case FIELD_STRING:
        return strcmp(a->string_data, b->string_data) == 0;
    case FIELD_U32:
        return a->u32_data == b->u32_data;
    default:
        return true;
    }
}

int main(void) {
    const Case cases[] = {
        {"chrysler_v0 (hex/2)", prep_chrysler_v0},
        {"ford_v1 (hex/8)", prep_ford_v1},
        {"psa (string)", prep_psa},
        {"kia_v6/honda_v1 (u32)", prep_kia_v6},
    };
    const size_t n = sizeof(cases) / sizeof(cases[0]);

    int failures = 0;

    printf("=== ALT (protopirate_storage_copy_key_2, vor dem Fix) ===\n");
    for(size_t i = 0; i < n; i++) {
        FakeFormat ff = {0};
        cases[i].prepare(&ff.key_2_src);
        old_copy_key_2(&ff);
        bool preserved = field_equal(&ff.key_2_src, &ff.key_2_dst);
        printf(
            "  %-24s -> Key_2 %s\n",
            cases[i].label,
            preserved ? "uebernommen" : "VERLOREN (dst.type == FIELD_NONE)");
        if(strcmp(cases[i].label, "chrysler_v0 (hex/2)") == 0 && preserved) failures++;
        if(strcmp(cases[i].label, "chrysler_v0 (hex/2)") != 0 && !preserved) failures++;
    }

    printf("\n=== NEU (mit hex/2-Versuch ergaenzt) ===\n");
    for(size_t i = 0; i < n; i++) {
        FakeFormat ff = {0};
        cases[i].prepare(&ff.key_2_src);
        new_copy_key_2(&ff);
        bool preserved = field_equal(&ff.key_2_src, &ff.key_2_dst);
        printf("  %-24s -> Key_2 %s\n", cases[i].label, preserved ? "uebernommen" : "VERLOREN");
        if(!preserved) failures++;
    }

    printf(
        "\n%s\n",
        failures == 0 ?
            "Bestaetigt: ALT verliert Key_2 nur im chrysler_v0-Fall (hex/2), alle anderen drei "
            "Formate bleiben bereits unter ALT erhalten. NEU erhaelt Key_2 in allen vier "
            "Faellen, ohne die drei bereits funktionierenden Faelle zu veraendern." :
            "FEHLER: Ergebnis weicht vom erwarteten Verhalten ab.");

    return failures == 0 ? 0 : 1;
}
