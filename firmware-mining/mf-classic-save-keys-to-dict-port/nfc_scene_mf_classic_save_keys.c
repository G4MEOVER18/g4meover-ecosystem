#include "../nfc_app_i.h"

#include <toolbox/keys_dict.h>

#define TAG "NfcMfClassicSaveKeys"

// A 4K card has 40 sectors, key A and key B each -- the most distinct keys one card can hand
// over. Bounds the two fixed-size arrays below without pulling in a protocol header just for
// MF_CLASSIC_TOTAL_SECTORS_MAX.
#define NFC_MF_CLASSIC_SAVE_KEYS_MAX (80)

// Append `key` unless `keys[0..key_count)` already holds it. Sectors commonly share a key
// (default/factory keys above all), and a duplicate offered twice would otherwise cost the
// dictionary compare below twice for no benefit.
static size_t nfc_mf_classic_save_keys_push_unique(
    MfClassicKey* keys,
    size_t key_count,
    const MfClassicKey* key) {
    if(key_count == NFC_MF_CLASSIC_SAVE_KEYS_MAX) return key_count;

    for(size_t i = 0; i < key_count; i++) {
        if(memcmp(keys[i].data, key->data, sizeof(MfClassicKey)) == 0) return key_count;
    }

    keys[key_count] = *key;
    return key_count + 1;
}

// Collects every key the card actually gave up, from the found masks rather than from whichever
// attack filled them -- one path this way covers the CUID dictionary, nested, MFKey32, a
// hand-entered key or an imported dump alike. An unrecovered slot holds zero fill, not a key, so
// gating on the mask bit (not on the bytes) is required: an all-zero key is a real key with its
// mask bit set, same as any other.
static size_t nfc_mf_classic_save_keys_collect(const MfClassicData* data, MfClassicKey* keys) {
    uint8_t num_sectors = mf_classic_get_total_sectors_num(data->type);
    size_t key_count = 0;

    for(uint8_t sector = 0; sector < num_sectors; sector++) {
        const MfClassicSectorTrailer* sec_tr =
            mf_classic_get_sector_trailer_by_sector(data, sector);

        if(FURI_BIT(data->key_a_mask, sector)) {
            key_count = nfc_mf_classic_save_keys_push_unique(keys, key_count, &sec_tr->key_a);
        }
        if(FURI_BIT(data->key_b_mask, sector)) {
            key_count = nfc_mf_classic_save_keys_push_unique(keys, key_count, &sec_tr->key_b);
        }
    }

    return key_count;
}

// Marks every key in `keys` that `dict` already holds, in one pass over the file instead of one
// keys_dict_is_key_present() scan per key (quadratic for a large dictionary and up to 80
// candidates). Comparing the parsed bytes rather than keys_dict_is_key_present()'s formatted
// strings also means a hand-edited lowercase-hex line in the dictionary still counts as present.
static void nfc_mf_classic_save_keys_mark_known(
    KeysDict* dict,
    const MfClassicKey* keys,
    size_t key_count,
    bool* known,
    size_t* known_count) {
    // A missing or empty dictionary holds nothing to compare against -- keys_dict_get_total_keys()
    // is 0 either way, and keys_dict_alloc() already leaves a missing file's stream closed.
    if(keys_dict_get_total_keys(dict) == 0) return;

    MfClassicKey dict_key;
    while(keys_dict_get_next_key(dict, dict_key.data, sizeof(MfClassicKey))) {
        for(size_t i = 0; i < key_count; i++) {
            if(known[i]) continue;
            if(memcmp(keys[i].data, dict_key.data, sizeof(MfClassicKey)) != 0) continue;

            known[i] = true;
            (*known_count)++;
            break; // Candidates are deduplicated, so one match settles this dictionary line.
        }
    }
}

static void
    nfc_scene_mf_classic_save_keys_callback(GuiButtonType button, InputType type, void* context) {
    NfcApp* instance = context;
    if(button == GuiButtonTypeLeft && type == InputTypeShort) {
        scene_manager_previous_scene(instance->scene_manager);
    }
}

void nfc_scene_mf_classic_save_keys_on_enter(void* context) {
    NfcApp* instance = context;
    const MfClassicData* data =
        nfc_device_get_data(instance->nfc_device, NfcProtocolMfClassic);

    MfClassicKey keys[NFC_MF_CLASSIC_SAVE_KEYS_MAX];
    size_t key_count = nfc_mf_classic_save_keys_collect(data, keys);

    bool known[NFC_MF_CLASSIC_SAVE_KEYS_MAX] = {};
    size_t known_count = 0;
    size_t added_count = 0;
    bool write_failed = false;

    if(key_count > 0) {
        // Filtering against the system dictionary is what stops a default-keyed card from
        // doubling the user dictionary with keys nobody needed to save. A missing system
        // dictionary is a broken install, not a reason to skip the filter silently -- every
        // factory key on the card would otherwise look new.
        if(!keys_dict_check_presence(NFC_APP_MF_CLASSIC_DICT_SYSTEM_PATH)) {
            FURI_LOG_W(
                TAG, "System dictionary %s is missing", NFC_APP_MF_CLASSIC_DICT_SYSTEM_PATH);
        }
        KeysDict* system_dict = keys_dict_alloc(
            NFC_APP_MF_CLASSIC_DICT_SYSTEM_PATH, KeysDictModeOpenExisting, sizeof(MfClassicKey));
        nfc_mf_classic_save_keys_mark_known(system_dict, keys, key_count, known, &known_count);
        keys_dict_free(system_dict);

        // One handle for both the read and the append below, so the user dictionary is not
        // walked a third time.
        KeysDict* user_dict = keys_dict_alloc(
            NFC_APP_MF_CLASSIC_DICT_USER_PATH, KeysDictModeOpenAlways, sizeof(MfClassicKey));
        nfc_mf_classic_save_keys_mark_known(user_dict, keys, key_count, known, &known_count);

        for(size_t i = 0; i < key_count; i++) {
            if(known[i]) continue;

            if(!keys_dict_add_key(user_dict, keys[i].data, sizeof(MfClassicKey))) {
                // Stop at the first failure: the cause (full SD, read-only card) applies to the
                // rest as well, and the count already written stays accurate.
                FURI_LOG_E(TAG, "Failed to add a key to the user dictionary");
                write_failed = true;
                break;
            }
            added_count++;
        }

        keys_dict_free(user_dict);
    }

    furi_string_reset(instance->text_box_store);
    if(write_failed) {
        furi_string_cat_printf(
            instance->text_box_store,
            "\e#Save Failed\nSaved %zu of %zu\nSD card full or\nread-only",
            added_count,
            key_count - known_count);
    } else {
        furi_string_cat_printf(
            instance->text_box_store,
            "\e#%s\nNew keys: %zu\nAlready known: %zu",
            added_count > 0 ? "Keys Saved" : "Nothing to Add",
            added_count,
            known_count);
    }

    if(added_count > 0) dolphin_deed(DolphinDeedNfcKeyAdd);

    widget_add_text_scroll_element(
        instance->widget, 2, 2, 124, 60, furi_string_get_cstr(instance->text_box_store));
    widget_add_button_element(
        instance->widget,
        GuiButtonTypeLeft,
        "Back",
        nfc_scene_mf_classic_save_keys_callback,
        instance);
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcViewWidget);
}

bool nfc_scene_mf_classic_save_keys_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void nfc_scene_mf_classic_save_keys_on_exit(void* context) {
    NfcApp* instance = context;
    widget_reset(instance->widget);
    furi_string_reset(instance->text_box_store);
}
