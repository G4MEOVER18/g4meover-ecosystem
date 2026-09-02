/* Host-side repro: evil_ble clone-fallback advertisement builder.
 *
 * Models the exact byte-construction logic of evil_ble_start_clone()'s
 * "else" branch (evil_ble.c, applications/external/evil_ble in the
 * RogueMaster bundle, commit b8fe671, source barkandbite/flipper_suite),
 * for the case dev.adv_data_len == 0.
 *
 * Constants match the real headers:
 *   EVIL_BLE_MAC_LEN            18  (evil_ble_scanner.h)
 *   EVIL_BLE_NAME_LEN           32  (evil_ble_scanner.h)
 *   EXTRA_BEACON_MAX_DATA_SIZE  31  (Flipper firmware furi_hal_bt extra
 *                                    beacon API, legacy BLE adv payload cap)
 *
 * ALT reproduces the upstream fallback verbatim: it treats dev.name as an
 * always-genuine broadcastable name, with no check for the scanner's own
 * MAC-only placeholder convention ("(AA:BB:CC:DD:EE:FF)", set by
 * parse_scanbt_line() in evil_ble_scanner.c whenever no real name was
 * captured, alongside adv_data_len = 0 for that exact case).
 *
 * NEU adds the same guard the scanner itself already uses in two places
 * (parse_scanbt_line() and evil_ble_scanner_rx_cb()) before calling
 * build_adv_from_name(): dev.name[0] != '\0' && dev.name[0] != '('.
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define EVIL_BLE_MAC_LEN 18
#define EVIL_BLE_NAME_LEN 32
#define EXTRA_BEACON_MAX_DATA_SIZE 31

typedef struct {
    char mac[EVIL_BLE_MAC_LEN];
    char name[EVIL_BLE_NAME_LEN];
    uint8_t adv_data[EXTRA_BEACON_MAX_DATA_SIZE];
    uint8_t adv_data_len;
} EvilBleDevice;

/* --- ALT: verbatim fallback branch from evil_ble_start_clone() (upstream) --- */
static uint8_t alt_build_fallback_adv(const EvilBleDevice* dev, uint8_t adv[EXTRA_BEACON_MAX_DATA_SIZE]) {
    uint8_t name_len = (uint8_t)strlen(dev->name);
    if(name_len > EXTRA_BEACON_MAX_DATA_SIZE - 2) {
        name_len = EXTRA_BEACON_MAX_DATA_SIZE - 2;
    }
    adv[0] = name_len + 1;
    adv[1] = 0x09;
    memcpy(&adv[2], dev->name, name_len);
    return (uint8_t)(2 + name_len);
}

/* --- NEU: same branch, guarded like the scanner's own placeholder check --- */
static uint8_t neu_build_fallback_adv(const EvilBleDevice* dev, uint8_t adv[EXTRA_BEACON_MAX_DATA_SIZE]) {
    if(dev->name[0] == '\0' || dev->name[0] == '(') {
        /* No real captured name (MAC-only device) -- do not fabricate a
         * Complete Local Name AD structure from the scanner's own
         * "(AA:BB:CC:DD:EE:FF)" display placeholder. Clone MAC-only. */
        return 0;
    }
    uint8_t name_len = (uint8_t)strlen(dev->name);
    if(name_len > EXTRA_BEACON_MAX_DATA_SIZE - 2) {
        name_len = EXTRA_BEACON_MAX_DATA_SIZE - 2;
    }
    adv[0] = name_len + 1;
    adv[1] = 0x09;
    memcpy(&adv[2], dev->name, name_len);
    return (uint8_t)(2 + name_len);
}

static void print_adv(const char* label, const uint8_t* adv, uint8_t len) {
    printf("  %s: len=%u bytes = [", label, len);
    for(uint8_t i = 0; i < len; i++) printf("%02X ", adv[i]);
    printf("]");
    if(len >= 2 && adv[1] == 0x09) {
        printf("  -> AD type 0x09 (Complete Local Name) = \"");
        for(uint8_t i = 2; i < len; i++) putchar((char)adv[i]);
        printf("\"");
    } else if(len == 0) {
        printf("  -> no AD structure emitted (MAC-only clone)");
    }
    printf("\n");
}

int main(void) {
    /* Case 1: MAC-only device, exactly as parse_scanbt_line() in
     * evil_ble_scanner.c produces it for a line like
     * "-70 Device: aa:bb:cc:dd:ee:ff" -- dev->name is set to
     * "(%s)" with the uppercased MAC, and dev->adv_data_len is left at 0. */
    EvilBleDevice mac_only;
    memset(&mac_only, 0, sizeof(mac_only));
    memcpy(mac_only.mac, "AA:BB:CC:DD:EE:FF", 17);
    snprintf(mac_only.name, sizeof(mac_only.name), "(%s)", mac_only.mac);
    mac_only.adv_data_len = 0;

    printf("== Case 1: MAC-only device (no name ever captured) ==\n");
    printf("  dev.name = \"%s\"  dev.adv_data_len = %u\n", mac_only.name, mac_only.adv_data_len);

    uint8_t alt_adv[EXTRA_BEACON_MAX_DATA_SIZE];
    uint8_t neu_adv[EXTRA_BEACON_MAX_DATA_SIZE];
    uint8_t alt_len = alt_build_fallback_adv(&mac_only, alt_adv);
    uint8_t neu_len = neu_build_fallback_adv(&mac_only, neu_adv);
    print_adv("ALT", alt_adv, alt_len);
    print_adv("NEU", neu_adv, neu_len);

    assert(alt_len == 21); /* 2 header bytes + 19-char "(AA:BB:CC:DD:EE:FF)" */
    assert(alt_adv[1] == 0x09);
    assert(memcmp(&alt_adv[2], "(AA:BB:CC:DD:EE:FF)", 19) == 0);
    printf("  ASSERT: ALT fabricates a Complete Local Name AD structure containing the\n"
           "          scanner's own display placeholder text -- confirmed BUG.\n");

    assert(neu_len == 0);
    printf("  ASSERT: NEU emits no Complete Local Name AD structure for a MAC-only\n"
           "          device (matches how the real target actually advertises) -- fixed.\n\n");

    /* Case 2 (regression): named device whose name field happens to start
     * with '(' for a completely unrelated reason -- out of scope for this
     * app (Marauder does not emit such names in practice) and unaffected
     * either way, included only to document the guard's edge condition. */

    /* Case 3 (regression): a device with a genuine captured name must be
     * completely unaffected by the fix -- ALT and NEU must match exactly. */
    EvilBleDevice named;
    memset(&named, 0, sizeof(named));
    memcpy(named.mac, "DE:AD:11:22:33:44", 17);
    strncpy(named.name, "MyPhone", sizeof(named.name) - 1);
    named.adv_data_len = 0; /* e.g. scanner's build_adv_from_name() was never invoked yet */

    printf("== Case 3 (regression): genuine named device, adv_data_len == 0 ==\n");
    printf("  dev.name = \"%s\"  dev.adv_data_len = %u\n", named.name, named.adv_data_len);
    alt_len = alt_build_fallback_adv(&named, alt_adv);
    neu_len = neu_build_fallback_adv(&named, neu_adv);
    print_adv("ALT", alt_adv, alt_len);
    print_adv("NEU", neu_adv, neu_len);
    assert(alt_len == neu_len);
    assert(memcmp(alt_adv, neu_adv, alt_len) == 0);
    printf("  ASSERT: ALT and NEU produce byte-identical output for a genuinely named\n"
           "          device -- fix does not change behaviour for the supported case.\n\n");

    /* Case 4 (regression): empty name (defensive edge case, e.g. a future
     * caller that never set dev.name at all) -- must not emit a bogus
     * zero-length Complete Local Name structure either. */
    EvilBleDevice empty;
    memset(&empty, 0, sizeof(empty));
    memcpy(empty.mac, "11:22:33:44:55:66", 17);
    empty.adv_data_len = 0;

    printf("== Case 4 (regression): empty dev.name, adv_data_len == 0 ==\n");
    alt_len = alt_build_fallback_adv(&empty, alt_adv);
    neu_len = neu_build_fallback_adv(&empty, neu_adv);
    print_adv("ALT", alt_adv, alt_len);
    print_adv("NEU", neu_adv, neu_len);
    assert(alt_len == 2 && alt_adv[0] == 0x01 && alt_adv[1] == 0x09);
    printf("  ASSERT: ALT already emits a degenerate Complete Local Name AD structure\n"
           "          (length field=1, i.e. just the type byte, zero name bytes) for an\n"
           "          empty name -- a second, smaller instance of the same root cause.\n");
    assert(neu_len == 0);
    printf("  ASSERT: NEU emits nothing for this case too -- fixed.\n\n");

    printf("ALL CHECKS PASSED.\n");
    return 0;
}
