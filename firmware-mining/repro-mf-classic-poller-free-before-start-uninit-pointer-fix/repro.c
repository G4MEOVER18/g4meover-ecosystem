// Host-Modell: mf_classic_poller_free() liest MfClassicPollerModeContext ungeprueft als
// MfClassicPollerDictAttackContext, obwohl das Union-Feld erst in mf_classic_poller_handler_start()
// (State Start) per memset genullt und fuer Read/Write mit eigenen, kleineren Kontextstrukturen
// befuellt wird - und ueberhaupt nicht, solange der Poller noch im vorgelagerten State
// MfClassicPollerStateDetectType steckt. mf_classic_poller_alloc() nutzt malloc() (kein calloc()),
// mode_ctx ist also bis zum ersten Start-Durchlauf uninitialisierter Heap-Inhalt.
//
// Realer Ablauf, der das ausloest (siehe lib/nfc/nfc_poller.c:
// nfc_poller_list_free() -> poller_api->free() unbedingt, unabhaengig vom internen State):
//   1. App startet MfClassic-Read/Write/Dict-Attack -> mf_classic_poller_alloc() (malloc,
//      mode_ctx uninitialisiert).
//   2. Poller haengt im State DetectType (Kartentyp wird noch bestimmt) - mode_ctx wurde noch
//      KEIN einziges Mal geschrieben.
//   3. Karte wird weggezogen / Nutzer bricht ab -> NFC-Schicht ruft nfc_poller_free() ->
//      mf_classic_poller_free(), voellig unabhaengig vom aktuellen internen State.
//   4. ALT liest instance->mode_ctx.dict_attack_ctx.{mf_classic_system_dict,
//      mf_classic_user_dict, nested_nonce.nonces} aus diesem nie geschriebenen Speicher und
//      ruft bei jedem != NULL wirkenden Wert keys_dict_free()/free() darauf auf - ein
//      Free auf einen Zeiger, den der Poller nie besessen hat.
//
// Dieses Modell bildet exakt die beiden betroffenen Funktionen nach (Feldreihenfolge/-typen
// 1:1 aus mf_classic_poller_i.h uebernommen) und vergleicht ALT (mein bisheriger,
// unveraenderter Stand) gegen NEU (mit instance->mode-Gate, siehe
// mf-classic-poller-free-before-start-uninit-pointer-fix.md).
//
// gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#define MF_CLASSIC_BLOCK_SIZE (16)
#define MF_CLASSIC_KEY_SIZE   (6)

typedef struct {
    uint8_t data[MF_CLASSIC_BLOCK_SIZE];
} MfClassicBlock;
typedef enum { MfClassicKeyTypeA, MfClassicKeyTypeB } MfClassicKeyType;
typedef struct {
    uint8_t data[MF_CLASSIC_KEY_SIZE];
} MfClassicKey;
typedef struct {
    uint8_t data[4];
} MfClassicAccessBits;
typedef union {
    MfClassicBlock block;
    struct {
        MfClassicKey key_a;
        MfClassicAccessBits access_bits;
        MfClassicKey key_b;
    };
} MfClassicSectorTrailer;

typedef struct {
    uint8_t current_sector;
    MfClassicSectorTrailer sec_tr;
    uint16_t current_block;
    bool is_value_block;
    MfClassicKeyType key_type_read;
    MfClassicKeyType key_type_write;
    bool need_halt_before_write;
    MfClassicBlock tag_block;
} MfClassicPollerWriteContext;

typedef enum {
    MfClassicBackdoorUnknown,
    MfClassicBackdoorNone,
    MfClassicBackdoorAuth1,
    MfClassicBackdoorAuth2,
    MfClassicBackdoorAuth3
} MfClassicBackdoor;
typedef enum {
    MfClassicPollerModeRead,
    MfClassicPollerModeWrite,
    MfClassicPollerModeDictAttackStandard,
    MfClassicPollerModeDictAttackCUID,
    MfClassicPollerModeDictAttackEnhanced
} MfClassicPollerMode;
typedef enum {
    MfClassicNestedPhaseNone,
    MfClassicNestedPhaseAnalyzePRNG,
    MfClassicNestedPhaseDictAttack,
    MfClassicNestedPhaseDictAttackVerify,
    MfClassicNestedPhaseDictAttackResume,
    MfClassicNestedPhaseCalibrate,
    MfClassicNestedPhaseRecalibrate,
    MfClassicNestedPhaseCollectNtEnc,
    MfClassicNestedPhaseFinished
} MfClassicNestedPhase;
typedef struct {
    uint32_t cuid;
    uint8_t key_idx;
    uint32_t nt;
    uint32_t nt_enc;
    uint8_t par;
    uint16_t dist;
} MfClassicNestedNonce;
typedef struct {
    MfClassicNestedNonce* nonces;
    size_t count;
} MfClassicNestedNonceArray;
typedef enum {
    MfClassicPrngTypeUnknown,
    MfClassicPrngTypeNoTag,
    MfClassicPrngTypeWeak,
    MfClassicPrngTypeHard
} MfClassicPrngType;
typedef struct KeysDictStub KeysDict;

typedef struct {
    uint8_t current_sector;
    MfClassicKey current_key;
    MfClassicKeyType current_key_type;
    MfClassicKeyType requested_key_type;
    bool auth_passed;
    uint16_t current_block;
    uint8_t reuse_key_sector;
    MfClassicBackdoor backdoor;
    MfClassicPollerMode mode; // internal dict-attack variant, NOT the top-level poller mode
    bool enhanced_dict;
    MfClassicNestedPhase nested_phase;
    MfClassicKey nested_known_key;
    MfClassicKeyType nested_known_key_type;
    bool current_key_checked;
    uint8_t nested_known_key_sector;
    uint16_t nested_target_key;
    MfClassicNestedNonceArray nested_nonce;
    MfClassicPrngType prng_type;
    bool static_encrypted;
    uint32_t static_encrypted_nonce;
    bool calibrated;
    uint16_t d_min;
    uint16_t d_max;
    uint8_t attempt_count;
    KeysDict* mf_classic_system_dict;
    KeysDict* mf_classic_user_dict;
    uint8_t nt_enc_msb[32];
    uint16_t msb_par_sum;
    uint16_t msb_count;
} MfClassicPollerDictAttackContext;

typedef struct {
    uint8_t current_sector;
    uint16_t current_block;
    MfClassicKeyType key_type;
    MfClassicKey key;
    bool auth_passed;
} MfClassicPollerReadContext;

typedef union {
    MfClassicPollerWriteContext write_ctx;
    MfClassicPollerDictAttackContext dict_attack_ctx;
    MfClassicPollerReadContext read_ctx;
} MfClassicPollerModeContext;

// --- Tracking-Allokator statt echtem keys_dict_free()/malloc(): registriert jeden
// "echten" Dictionary-Kauf in einer globalen Tabelle. free_tracked() bricht hart ab, wenn der
// uebergebene Zeiger nicht (mehr) in der Tabelle steht - modelliert exakt das, was ein
// BusFault/Invalid-Free auf einem nicht besessenen Zeiger waere, ohne von einem konkreten
// Allocator-/ASan-Heap-Layout abzuhaengen.
#define MAX_TRACKED 64
static void* tracked_ptrs[MAX_TRACKED];
static int tracked_count = 0;
static int invalid_frees = 0;

static void* tracked_alloc(size_t n) {
    void* p = malloc(n);
    assert(tracked_count < MAX_TRACKED);
    tracked_ptrs[tracked_count++] = p;
    return p;
}

static void free_tracked(void* p) {
    for(int i = 0; i < tracked_count; i++) {
        if(tracked_ptrs[i] == p) {
            free(p);
            tracked_ptrs[i] = tracked_ptrs[--tracked_count];
            return;
        }
    }
    // p war nie ein von uns vergebener Zeiger - genau der Fehlerfall, den ALT ausloesen kann.
    invalid_frees++;
}

typedef struct {
    // Nur die fuer diesen Fund relevanten Felder des echten MfClassicPoller.
    MfClassicPollerMode mode; // NEU: top-level Feld, ALT hat dieses Feld nicht
    MfClassicPollerModeContext mode_ctx;
} PollerModel;

static PollerModel* poller_alloc_alt(void) {
    // malloc(), kein calloc() - 1:1 wie mf_classic_poller_alloc(): mode_ctx bleibt
    // uninitialisiert bis Start ihn nullt.
    PollerModel* instance = malloc(sizeof(PollerModel));
    return instance;
}

static PollerModel* poller_alloc_neu(void) {
    PollerModel* instance = malloc(sizeof(PollerModel));
    instance->mode = MfClassicPollerModeRead; // neue Absicherung aus dem Port
    return instance;
}

// mf_classic_poller_free() ALT: liest mode_ctx bedingungslos als dict_attack_ctx.
static void poller_free_alt(PollerModel* instance) {
    MfClassicPollerDictAttackContext* dict_attack_ctx = &instance->mode_ctx.dict_attack_ctx;
    if(dict_attack_ctx->mf_classic_system_dict) {
        free_tracked(dict_attack_ctx->mf_classic_system_dict);
    }
    if(dict_attack_ctx->mf_classic_user_dict) {
        free_tracked(dict_attack_ctx->mf_classic_user_dict);
    }
    if(dict_attack_ctx->nested_nonce.nonces) {
        free_tracked(dict_attack_ctx->nested_nonce.nonces);
    }
    free(instance);
}

// mf_classic_poller_free() NEU: Gate auf instance->mode (Port-Vorschlag).
static void poller_free_neu(PollerModel* instance) {
    if(instance->mode == MfClassicPollerModeDictAttackStandard ||
       instance->mode == MfClassicPollerModeDictAttackCUID ||
       instance->mode == MfClassicPollerModeDictAttackEnhanced) {
        MfClassicPollerDictAttackContext* dict_attack_ctx = &instance->mode_ctx.dict_attack_ctx;
        if(dict_attack_ctx->mf_classic_system_dict) {
            free_tracked(dict_attack_ctx->mf_classic_system_dict);
        }
        if(dict_attack_ctx->mf_classic_user_dict) {
            free_tracked(dict_attack_ctx->mf_classic_user_dict);
        }
        if(dict_attack_ctx->nested_nonce.nonces) {
            free_tracked(dict_attack_ctx->nested_nonce.nonces);
        }
    }
    free(instance);
}

// Fuellt mode_ctx mit einem gewaehlten "Poison"-Bytemuster - modelliert den Bereich
// realen, nicht deterministischen Heap-Inhalts, den malloc() ohne calloc() liefern kann
// (z. B. Reste einer zuvor freigegebenen, wiederverwendeten Allokation).
static void poison_mode_ctx(PollerModel* instance, uint8_t byte_pattern) {
    memset(&instance->mode_ctx, byte_pattern, sizeof(MfClassicPollerModeContext));
}

static int test_detect_type_free_alt_vs_neu(void) {
    printf("== Test 1: Poller freigegeben waehrend DetectType (mode_ctx nie geschrieben) ==\n");
    int alt_crashes = 0;
    int trials = 200;
    for(int t = 0; t < trials; t++) {
        // Realistische Poison-Bytes: nicht alles 0x00 (das waere der guenstige Sonderfall,
        // den manche Allocator/frische Seiten liefern) - Testspektrum deckt beides ab.
        uint8_t pattern = (uint8_t)(t % 256);

        PollerModel* alt = poller_alloc_alt();
        poison_mode_ctx(alt, pattern);
        tracked_count = 0;
        invalid_frees = 0;
        poller_free_alt(alt);
        if(pattern != 0x00 && invalid_frees > 0) alt_crashes++;

        // poller_alloc_neu() hat instance->mode bereits auf Read gesetzt; poison_mode_ctx()
        // ruehrt nur mode_ctx an, mode bleibt unangetastet - wie beim echten Port, wo mode
        // ein eigenes struct-Feld ist, keine Union-Faerbung von mode_ctx.
        PollerModel* neu = poller_alloc_neu();
        poison_mode_ctx(neu, pattern);
        tracked_count = 0;
        invalid_frees = 0;
        poller_free_neu(neu);
        if(invalid_frees != 0) {
            printf("  FEHLER: NEU meldet einen Invalid-Free bei Pattern 0x%02X (mode=Read)\n",
                   pattern);
            return 1;
        }
    }
    printf(
        "  ALT: %d/%d Poison-Muster (!= 0x00) fuehren zu einem Free auf einen nicht besessenen "
        "Zeiger\n",
        alt_crashes,
        trials - 1); // Pattern 0 ist der einzige ausgeschlossene, garantiert sichere Fall
    printf("  NEU: 0/%d Invalid-Frees (mode=Read blockt den gesamten Cleanup-Block)\n", trials);
    if(alt_crashes == 0) {
        printf("  FEHLER: ALT haette bei den meisten Poison-Mustern invalid frees zeigen "
               "muessen\n");
        return 1;
    }
    return 0;
}

static int test_real_dict_attack_still_freed_neu(void) {
    printf("== Test 2: Regression -- echte Dict-Attack-Session gibt weiterhin alles frei ==\n");
    PollerModel* neu = poller_alloc_neu();
    memset(&neu->mode_ctx, 0, sizeof(MfClassicPollerModeContext)); // wie handler_start() es tut
    neu->mode = MfClassicPollerModeDictAttackEnhanced; // wie handler_start() es jetzt setzt

    MfClassicPollerDictAttackContext* ctx = &neu->mode_ctx.dict_attack_ctx;
    ctx->mf_classic_system_dict = (KeysDict*)tracked_alloc(1);
    ctx->mf_classic_user_dict = (KeysDict*)tracked_alloc(1);
    ctx->nested_nonce.nonces = malloc(sizeof(MfClassicNestedNonce) * 4);
    tracked_ptrs[tracked_count++] = ctx->nested_nonce.nonces;
    ctx->nested_nonce.count = 4;

    int before = tracked_count;
    poller_free_neu(neu);
    printf(
        "  %d von %d echten Allokationen wurden freigegeben (invalid_frees=%d)\n",
        before - tracked_count,
        before,
        invalid_frees);
    if(tracked_count != 0 || invalid_frees != 0) {
        printf("  FEHLER: NEU haette alle drei Allokationen einer echten Dict-Attack-Session "
               "freigeben muessen, ohne einen Invalid-Free\n");
        return 1;
    }
    return 0;
}

static int test_all_three_dict_modes_gate_correctly(void) {
    printf("== Test 3: Alle drei Dict-Attack-Modi loesen das NEU-Gate aus, Read/Write nicht ==\n");
    MfClassicPollerMode dict_modes[3] = {
        MfClassicPollerModeDictAttackStandard,
        MfClassicPollerModeDictAttackCUID,
        MfClassicPollerModeDictAttackEnhanced};
    for(int i = 0; i < 3; i++) {
        PollerModel* neu = poller_alloc_neu();
        memset(&neu->mode_ctx, 0, sizeof(MfClassicPollerModeContext));
        neu->mode = dict_modes[i];
        neu->mode_ctx.dict_attack_ctx.mf_classic_system_dict = (KeysDict*)tracked_alloc(1);
        tracked_count = 1;
        invalid_frees = 0;
        poller_free_neu(neu);
        if(tracked_count != 0) {
            printf("  FEHLER: Dict-Modus %d hat die Dictionary-Allokation nicht freigegeben\n", i);
            return 1;
        }
    }
    MfClassicPollerMode non_dict_modes[2] = {MfClassicPollerModeRead, MfClassicPollerModeWrite};
    for(int i = 0; i < 2; i++) {
        PollerModel* neu = poller_alloc_neu();
        poison_mode_ctx(neu, 0xAB);
        neu->mode = non_dict_modes[i];
        tracked_count = 0;
        invalid_frees = 0;
        poller_free_neu(neu);
        if(invalid_frees != 0) {
            printf("  FEHLER: Nicht-Dict-Modus %d hat trotz Gate einen Invalid-Free ausgeloest\n",
                   i);
            return 1;
        }
    }
    printf("  3/3 Dict-Attack-Modi geben frei, 2/2 Read/Write-Faelle bleiben trotz Poison "
           "unangetastet\n");
    return 0;
}

int main(void) {
    int failures = 0;
    failures += test_detect_type_free_alt_vs_neu();
    failures += test_real_dict_attack_still_freed_neu();
    failures += test_all_three_dict_modes_gate_correctly();

    if(failures == 0) {
        printf("\nALLE CHECKS BESTANDEN\n");
        return 0;
    } else {
        printf("\n%d CHECK-GRUPPE(N) FEHLGESCHLAGEN\n", failures);
        return 1;
    }
}
