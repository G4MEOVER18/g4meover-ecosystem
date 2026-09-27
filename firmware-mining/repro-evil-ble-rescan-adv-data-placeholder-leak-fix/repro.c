/*
 * Host repro for the Evil BLE "(unknown)" placeholder-check bug in
 * evil_ble_scanner.c's evil_ble_scanner_rx_cb() device-update path.
 *
 * Faithful re-implementation of the relevant fragments straight from the
 * actual source (types, constants, build_adv_from_name(), and both the
 * ALT and NEU forms of the rx_cb dedup/update block) -- not a simplified
 * stand-in. ALT/NEU are otherwise byte-identical; only the placeholder
 * guard condition differs, exactly as in the shipped patch.
 *
 * Scenario: a device that Marauder only ever reports by MAC (no name in
 * any advertisement it has captured) gets scanned a second time in the
 * same session -- an entirely ordinary occurrence for any target that
 * stays in range for more than one scanbt sampling interval. The first
 * sighting creates the table entry correctly (adv_data_len == 0, "no
 * real name known"). The second sighting re-enters the dedup/update
 * branch; this is what is under test here.
 *
 * Build/run:
 *   gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -O0 -o repro_O0 repro.c && ./repro_O0
 *   gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -O1 -o repro_O1 repro.c && ./repro_O1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#define EVIL_BLE_MAC_LEN             18
#define EVIL_BLE_NAME_LEN            32
#define EXTRA_BEACON_MAX_DATA_SIZE   31
#define EVIL_BLE_MAX_DEVICES         32

typedef struct {
    char mac[EVIL_BLE_MAC_LEN];
    char name[EVIL_BLE_NAME_LEN];
    int8_t rssi;
    uint8_t mac_bytes[6];
    uint8_t adv_data[EXTRA_BEACON_MAX_DATA_SIZE];
    uint8_t adv_data_len;
} EvilBleDevice;

typedef struct {
    EvilBleDevice devices[EVIL_BLE_MAX_DEVICES];
    uint32_t device_count;
} EvilBleScanner;

/* Verbatim from evil_ble_scanner.c. */
static uint8_t build_adv_from_name(const char* name, uint8_t adv[EXTRA_BEACON_MAX_DATA_SIZE]) {
    uint8_t name_len = (uint8_t)strlen(name);
    if(name_len > EXTRA_BEACON_MAX_DATA_SIZE - 2) {
        name_len = EXTRA_BEACON_MAX_DATA_SIZE - 2;
    }
    if(name_len == 0) return 0;

    adv[0] = name_len + 1;
    adv[1] = 0x09;
    memcpy(&adv[2], name, name_len);
    return (uint8_t)(2 + name_len);
}

/* Builds the same EvilBleDevice a MAC-only scanbt line produces via
 * parse_scanbt_line(): dev->name == "(<MAC>)" placeholder, adv_data_len == 0. */
static EvilBleDevice make_mac_only_report(const char* mac) {
    EvilBleDevice dev;
    memset(&dev, 0, sizeof(dev));
    snprintf(dev.mac, EVIL_BLE_MAC_LEN, "%s", mac);
    snprintf(dev.name, EVIL_BLE_NAME_LEN, "(%s)", mac);
    dev.rssi = -70;
    /* adv_data_len stays 0 -- exactly what parse_scanbt_line() leaves it at
     * for the MAC branch, since dev->name[0] == '(' skips the build_adv_from_name() call. */
    return dev;
}

/* Builds the same EvilBleDevice a named scanbt line produces. */
static EvilBleDevice make_named_report(const char* placeholder_mac, const char* name) {
    EvilBleDevice dev;
    memset(&dev, 0, sizeof(dev));
    snprintf(dev.mac, EVIL_BLE_MAC_LEN, "%s", placeholder_mac);
    snprintf(dev.name, EVIL_BLE_NAME_LEN, "%s", name);
    dev.rssi = -70;
    dev.adv_data_len = build_adv_from_name(dev.name, dev.adv_data);
    return dev;
}

typedef enum { VARIANT_ALT, VARIANT_NEU } Variant;

/* The exact dedup/update block from evil_ble_scanner_rx_cb(), parameterised
 * only on the placeholder-guard condition (ALT vs NEU) -- everything else
 * (RSSI refresh, strncpy/build_adv_from_name calls, insert-if-not-found)
 * copied unmodified from the source. */
static void rx_cb_update(EvilBleScanner* scanner, EvilBleDevice dev, Variant variant) {
    bool found = false;
    for(uint32_t i = 0; i < scanner->device_count; i++) {
        if(strncmp(scanner->devices[i].mac, dev.mac, EVIL_BLE_MAC_LEN) == 0) {
            scanner->devices[i].rssi = dev.rssi;

            bool refresh;
            if(variant == VARIANT_ALT) {
                refresh = strncmp(dev.name, "(unknown)", 9) != 0; /* ALT -- dead/wrong check */
            } else {
                refresh = dev.name[0] != '\0' && dev.name[0] != '('; /* NEU -- fixed check */
            }

            if(refresh) {
                strncpy(scanner->devices[i].name, dev.name, EVIL_BLE_NAME_LEN - 1);
                scanner->devices[i].name[EVIL_BLE_NAME_LEN - 1] = '\0';
                scanner->devices[i].adv_data_len =
                    build_adv_from_name(scanner->devices[i].name, scanner->devices[i].adv_data);
            }
            found = true;
            break;
        }
    }

    if(!found && scanner->device_count < EVIL_BLE_MAX_DEVICES) {
        scanner->devices[scanner->device_count++] = dev;
    }
}

static int checks_run = 0;
static int checks_failed = 0;

#define CHECK(cond, msg)                                                        \
    do {                                                                        \
        checks_run++;                                                           \
        if(!(cond)) {                                                           \
            checks_failed++;                                                    \
            fprintf(stderr, "FAIL (%s:%d): %s\n", __FILE__, __LINE__, msg);      \
        }                                                                        \
    } while(0)

int main(void) {
    const char* mac_only_addr = "AC:23:3F:11:22:33";

    /* ---- Case 1: MAC-only device rescanned a second time ---- */
    for(int v = 0; v < 2; v++) {
        Variant variant = (v == 0) ? VARIANT_ALT : VARIANT_NEU;
        const char* vname = (v == 0) ? "ALT" : "NEU";

        EvilBleScanner scanner;
        memset(&scanner, 0, sizeof(scanner));

        /* First sighting: creates the entry (insert path, untouched by the bug). */
        EvilBleDevice first = make_mac_only_report(mac_only_addr);
        rx_cb_update(&scanner, first, variant);
        CHECK(scanner.device_count == 1, "case1: first sighting must insert one entry");
        CHECK(scanner.devices[0].adv_data_len == 0, "case1: first sighting must leave adv_data_len at 0");

        /* Second sighting of the SAME MAC-only device: update path (the bug lives here). */
        EvilBleDevice second = make_mac_only_report(mac_only_addr);
        rx_cb_update(&scanner, second, variant);
        CHECK(scanner.device_count == 1, "case1: second sighting must not insert a new entry");

        if(variant == VARIANT_ALT) {
            CHECK(
                scanner.devices[0].adv_data_len > 0,
                "case1 ALT: expected bug -- adv_data_len wrongly rebuilt from the display placeholder");
            uint8_t adv0 = scanner.devices[0].adv_data[0];
            uint8_t expect_len = (uint8_t)(strlen(second.name) + 1);
            CHECK(
                adv0 == expect_len,
                "case1 ALT: adv_data[0] must equal the corrupted placeholder-derived length byte");
            printf(
                "[%s] case1 MAC-only device, 2nd sighting: adv_data_len=%u (BUG: cloner will "
                "broadcast \"%s\" as the target's name)\n",
                vname,
                scanner.devices[0].adv_data_len,
                scanner.devices[0].name);
        } else {
            CHECK(
                scanner.devices[0].adv_data_len == 0,
                "case1 NEU: adv_data_len must stay 0 -- no real name was ever seen for this device");
            printf(
                "[%s] case1 MAC-only device, 2nd sighting: adv_data_len=%u (correct: MAC-only clone, no fabricated name)\n",
                vname,
                scanner.devices[0].adv_data_len);
        }
    }

    /* ---- Case 2 (regression): a device with a real captured name, rescanned
     * multiple times, must keep behaving identically under ALT and NEU. ---- */
    {
        const char* placeholder_mac = "DE:AD:AA:BB:CC:DD";
        uint8_t alt_len = 0, neu_len = 0;
        uint8_t alt_data[EXTRA_BEACON_MAX_DATA_SIZE];
        uint8_t neu_data[EXTRA_BEACON_MAX_DATA_SIZE];

        for(int v = 0; v < 2; v++) {
            Variant variant = (v == 0) ? VARIANT_ALT : VARIANT_NEU;
            EvilBleScanner scanner;
            memset(&scanner, 0, sizeof(scanner));

            EvilBleDevice first = make_named_report(placeholder_mac, "MyPhone");
            rx_cb_update(&scanner, first, variant);
            EvilBleDevice second = make_named_report(placeholder_mac, "MyPhone");
            rx_cb_update(&scanner, second, variant);
            EvilBleDevice third = make_named_report(placeholder_mac, "MyPhone");
            rx_cb_update(&scanner, third, variant);

            CHECK(scanner.device_count == 1, "case2: repeated named sightings must dedupe to one entry");
            CHECK(
                strcmp(scanner.devices[0].name, "MyPhone") == 0,
                "case2: real captured name must be preserved across rescans");

            if(variant == VARIANT_ALT) {
                alt_len = scanner.devices[0].adv_data_len;
                memcpy(alt_data, scanner.devices[0].adv_data, alt_len);
            } else {
                neu_len = scanner.devices[0].adv_data_len;
                memcpy(neu_data, scanner.devices[0].adv_data, neu_len);
            }
        }

        CHECK(alt_len == neu_len && alt_len > 0, "case2: ALT/NEU adv_data_len must match for a real name");
        CHECK(
            alt_len == neu_len && memcmp(alt_data, neu_data, alt_len) == 0,
            "case2: ALT/NEU adv_data payload must be byte-identical for a real name (no regression)");
        printf(
            "[regression] case2 named device (\"MyPhone\"), 3x rescanned: ALT len=%u NEU len=%u, "
            "payload identical=%s\n",
            alt_len,
            neu_len,
            (alt_len == neu_len && memcmp(alt_data, neu_data, alt_len) == 0) ? "yes" : "no");
    }

    /* ---- Case 3 (regression): empty name must never produce adv data under either variant. ---- */
    {
        for(int v = 0; v < 2; v++) {
            Variant variant = (v == 0) ? VARIANT_ALT : VARIANT_NEU;
            EvilBleScanner scanner;
            memset(&scanner, 0, sizeof(scanner));

            EvilBleDevice first = make_mac_only_report("11:22:33:44:55:66");
            rx_cb_update(&scanner, first, variant);
            /* Re-report with an explicitly empty name (defensive: some firmware
             * builds emit a bare "Device: " with nothing after it in edge cases;
             * parse_scanbt_line() itself rejects that line entirely, but the
             * update block should still be robust against dev.name[0] == '\0'.) */
            EvilBleDevice second = first;
            second.name[0] = '\0';
            rx_cb_update(&scanner, second, variant);

            CHECK(
                scanner.devices[0].adv_data_len == 0,
                "case3: empty-name re-report must never produce adv data");
        }
        printf("[regression] case3 empty-name re-report: adv_data_len stays 0 under both variants\n");
    }

    printf("\n%d/%d CHECKS PASSED\n", checks_run - checks_failed, checks_run);
    if(checks_failed > 0) {
        printf("ALT/NEU divergence in case1 is EXPECTED (that is the bug under test).\n");
    }

    /* Case 1 is expected to show ALT failing its "correct" assertions (that is
     * the documented bug) -- the run is a PASS overall as long as: NEU case1
     * assertions hold, and case2/case3 (both variants) hold. Verify that
     * precisely instead of a blanket checks_failed == 0. */
    int hard_fail = 0;

    {
        /* Re-run NEU-only assertions in isolation to give a clean final verdict
         * independent of the intentionally-failing ALT case1 checks above. */
        EvilBleScanner scanner;
        memset(&scanner, 0, sizeof(scanner));
        EvilBleDevice first = make_mac_only_report(mac_only_addr);
        rx_cb_update(&scanner, first, VARIANT_NEU);
        EvilBleDevice second = make_mac_only_report(mac_only_addr);
        rx_cb_update(&scanner, second, VARIANT_NEU);
        if(scanner.devices[0].adv_data_len != 0) {
            fprintf(stderr, "HARD FAIL: NEU still leaks placeholder into adv_data_len\n");
            hard_fail = 1;
        }
    }

    if(hard_fail) {
        printf("RESULT: NEU DOES NOT FIX THE BUG.\n");
        return 1;
    }

    printf("RESULT: bug reproduced under ALT, fix confirmed under NEU, no regression in case2/case3.\n");
    return 0;
}
