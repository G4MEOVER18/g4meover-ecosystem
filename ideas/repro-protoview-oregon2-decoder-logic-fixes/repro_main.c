#include <stdio.h>
#include <string.h>
#include <math.h>
#include "mocks.h"
#include "oregon2_extract.h"

static int checks_ok = 0, checks_total = 0;
#define CHECK(desc, cond) do { \
    checks_total++; \
    if (cond) { checks_ok++; printf("[OK]   %s\n", desc); } \
    else      { printf("[FAIL] %s\n", desc); } \
} while (0)

static int feq(float a, float b) { return fabsf(a-b) < 0.0001f; }

/* Builds a buffer encoding device-id bytes (id0,id1) and three BCD
 * temperature digits (tens,ones,dec) plus two BCD humidity digits
 * (tens,ones), at exactly the nibble indexes decode()'s switch
 * statement reads them from (0,1 -> deviceid[0]; 2,3 -> deviceid[1];
 * 10,9,8 -> temp[0..2]; 13,12 -> hum[0..1]). All other nibbles are left
 * at 0 (unused by the code under test). */
static void build_buffer(uint8_t *buffer, uint8_t id0, uint8_t id1,
                          uint8_t temp_tens, uint8_t temp_ones, uint8_t temp_dec,
                          uint8_t hum_tens, uint8_t hum_ones)
{
    memset(buffer,0,8);
    set_nibble(buffer,0,(id0>>4)&0xF);
    set_nibble(buffer,1,id0&0xF);
    set_nibble(buffer,2,(id1>>4)&0xF);
    set_nibble(buffer,3,id1&0xF);
    set_nibble(buffer,10,temp_tens);
    set_nibble(buffer,9,temp_ones);
    set_nibble(buffer,8,temp_dec);
    set_nibble(buffer,13,hum_tens);
    set_nibble(buffer,12,hum_ones);
}

/* Reference formulas computed independently of both ALT and NEU, from
 * the raw BCD digits -- this is the ground truth, not a copy of either
 * implementation under test. */
static float ref_temp_correct(int tens, int ones, int dec) {
    return tens*10 + ones + dec*0.1f;
}
static int ref_hum_correct(int tens, int ones) {
    return tens*10 + ones;
}
/* What ALT actually computes if you double-apply the ASCII-digit
 * offset: each raw digit value d is turned into (d-'0') first. */
static float ref_temp_alt(int tens, int ones, int dec) {
    return (tens-'0')*10 + (ones-'0') + ((float)(dec-'0'))*0.1f;
}
static int ref_hum_alt(int tens, int ones) {
    return (tens-'0')*10 + (ones-'0');
}

static void run_scenario(const char *label,
                          uint8_t id0, uint8_t id1,
                          int temp_tens, int temp_ones, int temp_dec,
                          int hum_tens, int hum_ones)
{
    printf("\n-- Scenario: %s (real reading: id=%02X%02X temp=%d%d.%d hum=%d%d) --\n",
           label, id0, id1, temp_tens, temp_ones, temp_dec, hum_tens, hum_ones);

    uint8_t buffer[8];
    build_buffer(buffer,id0,id1,temp_tens,temp_ones,temp_dec,hum_tens,hum_ones);

    Oregon2Result alt, neu;
    oregon2_extract_ALT(buffer,&alt);
    oregon2_extract_NEU(buffer,&neu);

    float real_temp = ref_temp_correct(temp_tens,temp_ones,temp_dec);
    int real_hum = ref_hum_correct(hum_tens,hum_ones);
    float alt_expected_temp = ref_temp_alt(temp_tens,temp_ones,temp_dec);
    int alt_expected_hum = ref_hum_alt(hum_tens,hum_ones);

    char desc[160];

    snprintf(desc,sizeof(desc),"NEU deviceid == real id (0x%02X%02X)",id0,id1);
    CHECK(desc, neu.deviceid[0]==id0 && neu.deviceid[1]==id1);

    snprintf(desc,sizeof(desc),"NEU temperature == real reading (%.1f)",(double)real_temp);
    CHECK(desc, feq(neu.tempval,real_temp));

    snprintf(desc,sizeof(desc),"NEU humidity == real reading (%d)",real_hum);
    CHECK(desc, neu.humval==real_hum);

    snprintf(desc,sizeof(desc),
        "ALT temperature matches the double-offset formula (predicted %.1f, got %.1f)",
        (double)alt_expected_temp,(double)alt.tempval);
    CHECK(desc, feq(alt.tempval,alt_expected_temp));

    snprintf(desc,sizeof(desc),
        "ALT humidity matches the double-offset formula (predicted %d, got %d)",
        alt_expected_hum,alt.humval);
    CHECK(desc, alt.humval==alt_expected_hum);

    snprintf(desc,sizeof(desc),
        "ALT temperature output (%.1f) does NOT match the real reading (%.1f) -- bug reproduced",
        (double)alt.tempval,(double)real_temp);
    CHECK(desc, !feq(alt.tempval,real_temp));

    snprintf(desc,sizeof(desc),
        "ALT humidity output (%d) does NOT match the real reading (%d) -- bug reproduced",
        alt.humval,real_hum);
    CHECK(desc, alt.humval != real_hum);
}

int main(void) {
    run_scenario("23.4 deg C / 55%% RH",0xAB,0xCD, 2,3,4, 5,5);
    run_scenario("0.0 deg C / 0%% RH (all-zero digits)",0x12,0x34, 0,0,0, 0,0);
    run_scenario("9.9 deg C / 99%% RH (max BCD digits)",0xFF,0x00, 9,9,9, 9,9);

    printf("\n%d/%d Pruefungen bestanden\n", checks_ok, checks_total);
    if (checks_ok == checks_total) {
        printf("ALL CHECKS PASSED\n");
        return 0;
    }
    printf("CHECKS FAILED\n");
    return 1;
}
