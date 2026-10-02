// Host-Repro fuer firmware-mining/faac-slh-genius-manufacture-field-port.md
//
// Transkribiert 1:1 aus lib/subghz/protocols/faac_slh.c (gepatchte Fassung):
//   - subghz_protocol_faac_slh_get_key() (neuer Helfer)
//   - die display_name-Auswahl aus subghz_protocol_decoder_faac_slh_get_string()
// Der Mock-Keystore ersetzt M_EACH()/SubGhzKeyArray_t durch ein schlichtes Array,
// die Iterationsreihenfolge und Abbruchlogik sind Zeile fuer Zeile identisch.
//
// Baut und laeuft ohne Flipper-SDK: g++ -std=c++17 -Wall -Wextra
//   -fsanitize=address,undefined -O0 repro.c -o repro && ./repro
// (ebenso mit -O1 ausgefuehrt)

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

typedef enum { KEELOQ_LEARNING_FAAC, KEELOQ_LEARNING_OTHER } KeeloqLearningType;

struct ManufactureCode {
    const char* name;
    KeeloqLearningType type;
    uint64_t key;
};

struct Keystore {
    const ManufactureCode* entries;
    size_t count;
};

// --- 1:1 aus faac_slh.c (gepatcht) ---------------------------------------

static uint64_t subghz_protocol_faac_slh_get_key(
    const Keystore* keystore,
    const char* wanted,
    const char** manufacture_name) {
    uint64_t faac_key = 0;
    const char* faac_name = nullptr;
    if(!wanted || (*wanted == '\0')) wanted = "FAAC_SLH";

    for(size_t i = 0; i < keystore->count; i++) {
        const ManufactureCode* manufacture_code = &keystore->entries[i];
        if(manufacture_code->type != KEELOQ_LEARNING_FAAC) continue;
        const char* name = manufacture_code->name;
        if(strcmp(name, wanted) == 0) {
            *manufacture_name = name;
            return manufacture_code->key;
        }
        // Fall back to Faac when the file names something this keystore lacks
        if(strcmp(name, "FAAC_SLH") == 0) {
            faac_key = manufacture_code->key;
            faac_name = name;
        }
    }
    if(faac_name) *manufacture_name = faac_name;
    return faac_key;
}

// Alte, fehlerhafte Fassung (unveraendert vor diesem Patch): iteriert alle
// KEELOQ_LEARNING_FAAC-Eintraege unbedingt durch, *manufacture_name wird bei
// jedem Treffer ueberschrieben -- der LETZTE Eintrag in Keystore-Reihenfolge
// gewinnt, unabhaengig vom Dateiwunsch (der nicht einmal geprueft wird).
static uint64_t subghz_protocol_faac_slh_get_key_OLD_BUGGY(
    const Keystore* keystore,
    const char** manufacture_name) {
    uint64_t decrypt_key = 0;
    for(size_t i = 0; i < keystore->count; i++) {
        const ManufactureCode* manufacture_code = &keystore->entries[i];
        switch(manufacture_code->type) {
        case KEELOQ_LEARNING_FAAC:
            decrypt_key = manufacture_code->key;
            *manufacture_name = manufacture_code->name;
            break;
        default:
            break;
        }
    }
    return decrypt_key;
}

static const char*
    display_name_for(const char* protocol_name, const char* manufacture_name) {
    const char* display_name = protocol_name;
    if(manufacture_name && (strcmp(manufacture_name, "FAAC_SLH") != 0)) {
        display_name = manufacture_name;
    }
    return display_name;
}

// --- Testaufbau ------------------------------------------------------------

static int checks_run = 0;
static int checks_failed = 0;

#define CHECK(cond, msg)                                                 \
    do {                                                                 \
        checks_run++;                                                   \
        if(!(cond)) {                                                   \
            checks_failed++;                                            \
            fprintf(stderr, "FAIL (%s:%d): %s\n", __FILE__, __LINE__, msg); \
        }                                                                \
    } while(0)

int main() {
    const ManufactureCode faac_only[] = {
        {"FAAC_SLH", KEELOQ_LEARNING_FAAC, 0x1111111111111111ULL},
    };
    const Keystore ks_faac_only = {faac_only, 1};

    const ManufactureCode faac_then_genius[] = {
        {"FAAC_SLH", KEELOQ_LEARNING_FAAC, 0x1111111111111111ULL},
        {"Genius", KEELOQ_LEARNING_FAAC, 0x2222222222222222ULL},
    };
    const Keystore ks_faac_then_genius = {faac_then_genius, 2};

    const ManufactureCode genius_then_faac[] = {
        {"Genius", KEELOQ_LEARNING_FAAC, 0x2222222222222222ULL},
        {"FAAC_SLH", KEELOQ_LEARNING_FAAC, 0x1111111111111111ULL},
    };
    const Keystore ks_genius_then_faac = {genius_then_faac, 2};

    const ManufactureCode three_faac_learning[] = {
        {"FAAC_SLH", KEELOQ_LEARNING_FAAC, 0x1111111111111111ULL},
        {"Genius", KEELOQ_LEARNING_FAAC, 0x2222222222222222ULL},
        {"SomeOtherFaacVariant", KEELOQ_LEARNING_FAAC, 0x3333333333333333ULL},
    };
    const Keystore ks_three = {three_faac_learning, 3};

    const ManufactureCode mixed_types[] = {
        {"NotFaac", KEELOQ_LEARNING_OTHER, 0x9999999999999999ULL},
        {"FAAC_SLH", KEELOQ_LEARNING_FAAC, 0x1111111111111111ULL},
        {"NotFaac2", KEELOQ_LEARNING_OTHER, 0x8888888888888888ULL},
    };
    const Keystore ks_mixed = {mixed_types, 3};

    const Keystore ks_empty = {nullptr, 0};

    // Szenario 1: Regression -- ein Eintrag, kein Dateiwunsch (wie jede vor
    // Genius geschriebene .sub-Datei) -> FAAC_SLH, unveraendertes Verhalten.
    {
        const char* used = nullptr;
        uint64_t key = subghz_protocol_faac_slh_get_key(&ks_faac_only, nullptr, &used);
        CHECK(key == 0x1111111111111111ULL, "Szenario 1: falscher Schluessel");
        CHECK(used != nullptr && strcmp(used, "FAAC_SLH") == 0, "Szenario 1: falscher Name");
    }

    // Szenario 2: der eigentliche Fund -- FAAC_SLH + Genius im Keystore,
    // Datei nennt keinen Hersteller (leerer String, wie es flipper_format_read_string
    // bei fehlendem "Manufacture"-Feld liefert). ALT waehlt hier den LETZTEN
    // Eintrag (Genius) -- ein bereits funktionierendes FAAC-Signal wuerde
    // ploetzlich mit dem falschen Schluessel ver-/entschluesselt. NEU muss
    // bei fehlendem Dateiwunsch weiterhin FAAC_SLH waehlen.
    {
        const char* used_new = nullptr;
        uint64_t key_new =
            subghz_protocol_faac_slh_get_key(&ks_faac_then_genius, "", &used_new);
        CHECK(key_new == 0x1111111111111111ULL, "Szenario 2 NEU: falscher Schluessel");
        CHECK(
            used_new != nullptr && strcmp(used_new, "FAAC_SLH") == 0,
            "Szenario 2 NEU: falscher Name");

        const char* used_old = nullptr;
        uint64_t key_old = subghz_protocol_faac_slh_get_key_OLD_BUGGY(
            &ks_faac_then_genius, &used_old);
        CHECK(
            key_old == 0x2222222222222222ULL,
            "Szenario 2 ALT: Bug nicht reproduziert (sollte Genius-Schluessel waehlen)");
        CHECK(
            used_old != nullptr && strcmp(used_old, "Genius") == 0,
            "Szenario 2 ALT: Bug nicht reproduziert (sollte 'Genius' melden)");
    }

    // Szenario 3: Datei nennt "Genius" -- muss den Genius-Eintrag waehlen,
    // unabhaengig von der Position im Keystore (zwei Reihenfolgen geprueft).
    {
        const char* used = nullptr;
        uint64_t key =
            subghz_protocol_faac_slh_get_key(&ks_faac_then_genius, "Genius", &used);
        CHECK(key == 0x2222222222222222ULL, "Szenario 3a: falscher Schluessel");
        CHECK(used != nullptr && strcmp(used, "Genius") == 0, "Szenario 3a: falscher Name");

        const char* used2 = nullptr;
        uint64_t key2 =
            subghz_protocol_faac_slh_get_key(&ks_genius_then_faac, "Genius", &used2);
        CHECK(key2 == 0x2222222222222222ULL, "Szenario 3b: falscher Schluessel");
        CHECK(used2 != nullptr && strcmp(used2, "Genius") == 0, "Szenario 3b: falscher Name");
    }

    // Szenario 4: Datei nennt "Genius", aber dieser Keystore hat (noch) keinen
    // Genius-Eintrag -- deckt genau die im Port-Vorschlag offen benannte
    // Unsicherheit ab, ob mein eigener Keystore bereits einen Genius-Eintrag
    // enthaelt. Erwartung: sauberer Fallback auf FAAC_SLH statt Schluessel 0
    // oder Absturz.
    {
        const char* used = nullptr;
        uint64_t key = subghz_protocol_faac_slh_get_key(&ks_faac_only, "Genius", &used);
        CHECK(key == 0x1111111111111111ULL, "Szenario 4: kein Fallback auf FAAC_SLH");
        CHECK(
            used != nullptr && strcmp(used, "FAAC_SLH") == 0,
            "Szenario 4: falscher gemeldeter Name beim Fallback");
    }

    // Szenario 5: komplett leerer Keystore (keine FAAC-Learning-Eintraege) --
    // darf nicht abstuerzen, muss Schluessel 0 liefern, *manufacture_name
    // bleibt unberuehrt (so wie es der Aufrufer bereits vor diesem Patch
    // erwartet: decrypt bleibt 0 wenn key==0).
    {
        const char* used = (const char*)0xDEADBEEF; // Wächterwert, muss unveraendert bleiben
        uint64_t key = subghz_protocol_faac_slh_get_key(&ks_empty, nullptr, &used);
        CHECK(key == 0, "Szenario 5: Schluessel sollte 0 sein");
        CHECK(used == (const char*)0xDEADBEEF, "Szenario 5: manufacture_name wurde unerwartet veraendert");
    }

    // Szenario 6: drei FAAC-Learning-Eintraege, Dateiwunsch trifft den
    // mittleren -- bestaetigt, dass die Auswahl wirklich namensbasiert ist
    // und nicht zufaellig durch Position/Reihenfolge funktioniert.
    {
        const char* used = nullptr;
        uint64_t key = subghz_protocol_faac_slh_get_key(&ks_three, "Genius", &used);
        CHECK(key == 0x2222222222222222ULL, "Szenario 6: falscher Schluessel");
        CHECK(used != nullptr && strcmp(used, "Genius") == 0, "Szenario 6: falscher Name");
    }

    // Szenario 7: Mix aus FAAC- und Nicht-FAAC-Lerntypen -- Nicht-FAAC-Eintraege
    // muessen uebersprungen werden (continue-Zweig), unabhaengig von ihrer
    // Position vor/nach dem eigentlichen FAAC_SLH-Eintrag.
    {
        const char* used = nullptr;
        uint64_t key = subghz_protocol_faac_slh_get_key(&ks_mixed, nullptr, &used);
        CHECK(key == 0x1111111111111111ULL, "Szenario 7: falscher Schluessel");
        CHECK(used != nullptr && strcmp(used, "FAAC_SLH") == 0, "Szenario 7: falscher Name");
    }

    // Szenario 8: display_name-Logik aus get_string(). FAAC_SLH zeigt
    // weiterhin den Protokollnamen, jeder andere Hersteller (Genius) seinen
    // eigenen Namen, kein Treffer (NULL) faellt sicher auf den Protokollnamen
    // zurueck (kein Null-Pointer-Dereferenzierungs-Risiko).
    {
        CHECK(
            strcmp(display_name_for("Faac SLH", "FAAC_SLH"), "Faac SLH") == 0,
            "Szenario 8a: FAAC_SLH sollte Protokollnamen zeigen");
        CHECK(
            strcmp(display_name_for("Faac SLH", "Genius"), "Genius") == 0,
            "Szenario 8b: Genius sollte eigenen Namen zeigen");
        CHECK(
            strcmp(display_name_for("Faac SLH", nullptr), "Faac SLH") == 0,
            "Szenario 8c: kein Treffer sollte auf Protokollnamen zurueckfallen");
    }

    printf("Einzelpruefungen: %d, fehlgeschlagen: %d\n", checks_run, checks_failed);
    if(checks_failed == 0) {
        printf("ALLE CHECKS BESTANDEN\n");
    }
    return checks_failed == 0 ? 0 : 1;
}
