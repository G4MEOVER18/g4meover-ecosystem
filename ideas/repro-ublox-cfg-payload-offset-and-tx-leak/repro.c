/*
 * Host-Repro gegen die echte, unveraendert mitkompilierte ublox_device.c
 * (liamhays/ublox, Commit bdb4e19, 2025-08-02) -- zwei getrennte Funde in
 * ublox_worker.c (ublox_worker_init_gps() / ublox_worker_sync_to_gps_time()):
 *
 * FUND A (Haupt-Fund): In ublox_worker_init_gps() wird fuer alle drei
 * Lese-Modifizieren-Schreiben-Zyklen (CFG-PMS/CFG-ODO/CFG-NAV5)
 *   <foo>_frame_tx.payload = <foo>_message_rx->message;
 * gesetzt -- also der ROHE, vollstaendige UBX-Rahmen (6-Byte-Header
 * sync1/sync2/class/id/len_lo/len_hi plus Payload plus Checksumme), nicht
 * der eigentliche Payload-Bereich (der laut der app-eigenen, korrekten
 * Parse-Funktion ublox_bytes_to_frame() erst bei Byte-Offset 6 beginnt).
 * Die anschliessenden Feld-Schreibzugriffe (z.B. "payload[4] |= 1" fuer
 * CFG-ODO) landen dadurch im 6-Byte-Header statt im echten Payload, und
 * die danach aufgerufene ublox_frame_to_bytes() kopiert fuer den
 * ausgehenden Schreib-Befehl dieselben falsch-versetzten Bytes. Dieses
 * Repro beweist das protokollunabhaengig (ohne auf die genaue u-blox-
 * Feldbedeutung angewiesen zu sein): es fuellt den simulierten
 * CFG-PMS-Lese-Antwort-Payload mit acht eindeutigen Markerbytes,
 * durchlaeuft wortgetreu den PMS-Codepfad aus ublox_worker_init_gps()
 * (Zeilen ~489-524 der Originaldatei) gegen die echte
 * ublox_frame_to_bytes()/ublox_i2c_transfer()/ublox_bytes_to_frame(),
 * und vergleicht den beim simulierten I2C-Schreibzugriff tatsaechlich
 * gesendeten Payload-Bereich (Byte 6.. der ausgehenden Nachricht) gegen
 * das, was bei korrekter +6-Indizierung (wie sie ublox_bytes_to_frame()
 * selbst schon richtig vorfuehrt) herausgekommen waere.
 *
 * FUND B (Nebenfund, selbe Datei): zwei der sechs ublox_frame_to_bytes()-
 * Ergebnisse (TX-Nachrichten) werden nie mit ublox_message_free()
 * freigegeben -- in ublox_worker_sync_to_gps_time() (message_tx) und im
 * NAV5-Schreib-Zweig von ublox_worker_init_gps() (der zweite,
 * wiederverwendete nav5_message_tx). Alle anderen vier TX-Allokationen
 * in derselben Datei (PVT, ODO-Lesen, PMS-Lesen+Schreiben, ODO-Lesen+
 * Schreiben, NAV5-Lesen, RESETODO) werden korrekt freigegeben --
 * Gegenprobe im Repro.
 *
 * Bau: gcc -std=c17 -Wall -Wextra -fsanitize=address,leak,undefined \
 *          -fno-sanitize-recover=undefined -Imocks -O0 \
 *          repro.c ublox_device.c -o repro_o0 && ./repro_o0
 *      (ebenso bei -O1)
 */

#include "ublox_device.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* --- Minimaler I2C-Mock: liefert eine zuvor per mock_set_response()
 * hinterlegte, gueltige UBX-Antwort zurueck. furi_hal_i2c_tx() wird
 * ignoriert (der TX-Inhalt selbst ist fuer beide Funde irrelevant). --- */
const FuriHalI2cBusHandle furi_hal_i2c_handle_external = {0};

static uint8_t g_mock_response[128];
static size_t g_mock_response_len;
static size_t g_mock_rx_offset;

static void mock_set_response(const uint8_t* buf, size_t len) {
    assert(len <= sizeof(g_mock_response));
    memcpy(g_mock_response, buf, len);
    g_mock_response_len = len;
    g_mock_rx_offset = 0;
}

bool furi_hal_i2c_is_device_ready(const FuriHalI2cBusHandle* h, uint8_t a, uint32_t t) {
    (void)h; (void)a; (void)t;
    return true;
}
bool furi_hal_i2c_tx(const FuriHalI2cBusHandle* h, uint8_t a, const uint8_t* d, size_t s, uint32_t t) {
    (void)h; (void)a; (void)d; (void)s; (void)t;
    return true;
}
bool furi_hal_i2c_rx(const FuriHalI2cBusHandle* h, uint8_t a, uint8_t* data, size_t size, uint32_t t) {
    (void)h; (void)a; (void)t;
    assert(g_mock_rx_offset + size <= g_mock_response_len);
    memcpy(data, g_mock_response + g_mock_rx_offset, size);
    g_mock_rx_offset += size;
    return true;
}

/* Baut eine gueltige rohe UBX-Antwortnachricht (Header+Payload+Checksumme),
 * mit demselben Checksum-Algorithmus wie ublox_device.c selbst. */
static void build_valid_message(
    uint8_t class_, uint8_t id_, const uint8_t* payload, uint16_t payload_len,
    uint8_t* out, size_t* out_len) {
    out[0] = 0xb5;
    out[1] = 0x62;
    out[2] = class_;
    out[3] = id_;
    out[4] = payload_len & 0xff;
    out[5] = (payload_len >> 8) & 0xff;
    memcpy(out + 6, payload, payload_len);
    uint8_t ck_a = 0, ck_b = 0;
    for(int i = 2; i < 2 + (int)(payload_len + 4); i++) {
        ck_a = ck_a + out[i];
        ck_b = ck_b + ck_a;
    }
    out[6 + payload_len] = ck_a;
    out[7 + payload_len] = ck_b;
    *out_len = 8 + payload_len;
}

/* ====================================================================
 * FUND A: CFG-PMS-Lese-Modifizieren-Schreiben-Zyklus, wortgetreu wie in
 * ublox_worker_init_gps() (Zeilen ~489-524 des Originals), inklusive des
 * echten "set power setup value to balanced"-Zugriffs
 * `pms_message_rx->message[6 + 1] = 0x01;`.
 * ==================================================================== */
static void scenario_fund_a_cfg_pms_offset(void) {
    /* Acht eindeutige Marker-Bytes als "aktuelle" CFG-PMS-Konfiguration,
     * wie sie ein echtes u-blox-Modul auf die Lese-Anfrage zurueckgeben
     * wuerde (version, powerSetupValue, period_lo/hi, onTime_lo/hi,
     * reserved1/2 -- Feldnamen laut u-blox-Interface-Beschreibung, aber
     * fuer dieses Repro nur als unterscheidbare Markerwerte wichtig). */
    uint8_t real_payload[8] = {0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97};
    uint8_t response[64];
    size_t response_len;
    build_valid_message(UBX_CFG_CLASS, UBX_CFG_PMS_MESSAGE, real_payload, 8, response, &response_len);

    /* --- Schritt 1: CFG-PMS lesen (wortgetreu) --- */
    UbloxFrame pms_frame_tx;
    pms_frame_tx.class = UBX_CFG_CLASS;
    pms_frame_tx.id = UBX_CFG_PMS_MESSAGE;
    pms_frame_tx.len = 0;
    pms_frame_tx.payload = NULL;
    UbloxMessage* pms_message_tx = ublox_frame_to_bytes(&pms_frame_tx);

    mock_set_response(response, response_len);
    UbloxMessage* pms_message_rx = ublox_i2c_transfer(pms_message_tx, UBX_CFG_PMS_MESSAGE_LENGTH);
    assert(pms_message_rx != NULL);
    ublox_message_free(pms_message_tx);

    /* --- Referenz: was ublox_bytes_to_frame() (die app-eigene, korrekte
     * Parse-Funktion) als echten Payload erkennen wuerde -- Beweis, dass
     * der echte Payload bei Offset 6 beginnt, nicht bei Offset 0. --- */
    UbloxFrame* parsed_correctly = ublox_bytes_to_frame(pms_message_rx);
    assert(parsed_correctly != NULL);
    assert(parsed_correctly->len == 8);
    assert(memcmp(parsed_correctly->payload, real_payload, 8) == 0);
    printf("Referenz OK: ublox_bytes_to_frame() liest den echten Payload korrekt bei Offset 6.\n");
    ublox_frame_free(parsed_correctly);

    /* --- Schritt 2 (ALT, wortgetreu aus ublox_worker_init_gps()):
     * payload wird auf den ROHEN Antwortpuffer gesetzt (Offset 0, nicht 6) --- */
    pms_message_rx->message[6 + 1] = 0x01; // "set power setup value to balanced"

    pms_frame_tx.class = UBX_CFG_CLASS;
    pms_frame_tx.id = UBX_CFG_PMS_MESSAGE;
    pms_frame_tx.len = 8;
    pms_frame_tx.payload = pms_message_rx->message; // <-- FUND: kein +6, voller Rahmen

    UbloxMessage* pms_message_tx_write = ublox_frame_to_bytes(&pms_frame_tx);

    /* Der tatsaechlich ausgehende, echte Payload-Bereich der neuen Nachricht
     * (Byte 6.. der von ublox_frame_to_bytes() erzeugten Bytes). */
    uint8_t* sent_payload = pms_message_tx_write->message + 6;

    printf("ALT  gesendeter CFG-PMS-Payload: %02x %02x %02x %02x %02x %02x %02x %02x\n",
           sent_payload[0], sent_payload[1], sent_payload[2], sent_payload[3],
           sent_payload[4], sent_payload[5], sent_payload[6], sent_payload[7]);
    printf("     (beabsichtigt waere: %02x 01 %02x %02x %02x %02x %02x %02x -- "
           "Originalwerte mit powerSetupValue=0x01 an echter Payload-Position 1)\n",
           real_payload[0], real_payload[2], real_payload[3], real_payload[4],
           real_payload[5], real_payload[6], real_payload[7]);

    /* Harter, protokollunabhaengiger Beweis: der gesendete Payload ist
     * NICHT "Originalwerte mit powerSetupValue=0x01 an Position 1" --
     * die eigentliche Absicht der zwei Codezeilen darueber wird verfehlt. */
    uint8_t intended[8];
    memcpy(intended, real_payload, 8);
    intended[1] = 0x01;
    int alt_matches_intent = (memcmp(sent_payload, intended, 8) == 0);
    printf("ALT: gesendeter Payload entspricht der Absicht? %s\n",
           alt_matches_intent ? "ja" : "NEIN (Fund bestaetigt)");
    assert(!alt_matches_intent);

    /* Stattdessen enthaelt der gesendete Payload die ersten 8 Bytes des
     * RAW-Puffers (Header+erste zwei echte Payload-Bytes) -- exakt das
     * mechanische Verhalten, das den Fund erklaert. */
    uint8_t expected_actual[8] = {
        0xb5, 0x62, UBX_CFG_CLASS, UBX_CFG_PMS_MESSAGE,
        8, 0, real_payload[0], 0x01 /* message[6+1] wurde oben modifiziert */
    };
    int alt_matches_mechanism = (memcmp(sent_payload, expected_actual, 8) == 0);
    printf("ALT: gesendeter Payload = rohe Rahmen-Bytes (Header+erste 2 echte Bytes)? %s\n",
           alt_matches_mechanism ? "ja (Mechanismus bestaetigt)" : "nein");
    assert(alt_matches_mechanism);

    /* --- Schritt 2 (NEU/Fix): dieselbe Absicht, aber mit dem bereits im
     * selben Codepfad vorhandenen, korrekten Muster -- Payload-Zeiger auf
     * den echten, bereits geparsten Payload setzen (ublox_bytes_to_frame()
     * liefert ihn bereits korrekt bei Offset 6; alternativ pms_message_rx
     * ->message + 6 direkt verwenden, ohne weitere Codeaenderung). --- */
    uint8_t fixed_payload[8];
    memcpy(fixed_payload, pms_message_rx->message + 6, 8); // echter Payload, Offset 6
    fixed_payload[1] = 0x01; // "set power setup value to balanced", jetzt an der richtigen Stelle

    UbloxFrame pms_frame_tx_fixed;
    pms_frame_tx_fixed.class = UBX_CFG_CLASS;
    pms_frame_tx_fixed.id = UBX_CFG_PMS_MESSAGE;
    pms_frame_tx_fixed.len = 8;
    pms_frame_tx_fixed.payload = fixed_payload;
    UbloxMessage* pms_message_tx_write_fixed = ublox_frame_to_bytes(&pms_frame_tx_fixed);
    uint8_t* sent_payload_fixed = pms_message_tx_write_fixed->message + 6;

    printf("NEU  gesendeter CFG-PMS-Payload: %02x %02x %02x %02x %02x %02x %02x %02x\n",
           sent_payload_fixed[0], sent_payload_fixed[1], sent_payload_fixed[2], sent_payload_fixed[3],
           sent_payload_fixed[4], sent_payload_fixed[5], sent_payload_fixed[6], sent_payload_fixed[7]);
    int neu_matches_intent = (memcmp(sent_payload_fixed, intended, 8) == 0);
    printf("NEU: gesendeter Payload entspricht der Absicht? %s\n",
           neu_matches_intent ? "ja (Fix bestaetigt)" : "NEIN");
    assert(neu_matches_intent);

    ublox_message_free(pms_message_tx_write);
    ublox_message_free(pms_message_tx_write_fixed);
    ublox_message_free(pms_message_rx);

    printf("FUND A bestaetigt: CFG-PMS-Schreibzugriff setzt in der Originalversion nie "
           "das echte powerSetupValue-Feld, Fix korrigiert das mit demselben bereits "
           "vorhandenen Payload-Zeiger-Muster.\n\n");
}

/* ====================================================================
 * FUND B: zwei TX-Nachrichten werden nie freigegeben. use_fix=0 bildet
 * exakt das Original nach (kein free), use_fix=1 ergaenzt den fehlenden
 * ublox_message_free()-Aufruf. LeakSanitizer entscheidet beim
 * Prozessende -- je ein Lauf pro Modus (siehe Nutzungshinweis unten).
 * ==================================================================== */
static void scenario_fund_b_sync_time_leak(int use_fix) {
    uint8_t real_payload[20] = {0};
    uint8_t response[64];
    size_t response_len;
    build_valid_message(UBX_NAV_CLASS, UBX_NAV_TIMEUTC_MESSAGE, real_payload, 20, response, &response_len);

    UbloxFrame frame_tx;
    frame_tx.class = UBX_NAV_CLASS;
    frame_tx.id = UBX_NAV_TIMEUTC_MESSAGE;
    frame_tx.len = 0;
    frame_tx.payload = NULL;
    UbloxMessage* message_tx = ublox_frame_to_bytes(&frame_tx); // <-- wird im Original nie freigegeben

    mock_set_response(response, response_len);
    UbloxMessage* message_rx = ublox_i2c_transfer(message_tx, UBX_NAV_TIMEUTC_MESSAGE_LENGTH);
    assert(message_rx != NULL);

    UbloxFrame* frame_rx = ublox_bytes_to_frame(message_rx);
    ublox_message_free(message_rx);
    assert(frame_rx != NULL);
    ublox_frame_free(frame_rx);

    if(use_fix) {
        ublox_message_free(message_tx); // <-- FIX: fehlender Aufruf im Original
    }
    // ALT: message_tx (UbloxMessage-Struct + 8-Byte-Puffer) bleibt haengen.
}

static void scenario_fund_b_nav5_write_leak(int use_fix) {
    uint8_t real_payload_read[36] = {0};
    uint8_t response_read[64];
    size_t response_read_len;
    build_valid_message(UBX_CFG_CLASS, UBX_CFG_NAV5_MESSAGE, real_payload_read, 36, response_read, &response_read_len);

    UbloxFrame nav5_frame_tx;
    nav5_frame_tx.class = UBX_CFG_CLASS;
    nav5_frame_tx.id = UBX_CFG_NAV5_MESSAGE;
    nav5_frame_tx.len = 0;
    nav5_frame_tx.payload = NULL;
    UbloxMessage* nav5_message_tx = ublox_frame_to_bytes(&nav5_frame_tx);

    mock_set_response(response_read, response_read_len);
    UbloxMessage* nav5_message_rx = ublox_i2c_transfer(nav5_message_tx, UBX_CFG_NAV5_MESSAGE_LENGTH);
    assert(nav5_message_rx != NULL);
    ublox_message_free(nav5_message_tx); // Lese-Anfrage wird korrekt freigegeben (nicht der Fund)

    nav5_frame_tx.class = UBX_CFG_CLASS;
    nav5_frame_tx.id = UBX_CFG_NAV5_MESSAGE;
    nav5_frame_tx.len = 36;
    nav5_frame_tx.payload = nav5_message_rx->message;
    nav5_frame_tx.payload[0] = 1;
    nav5_frame_tx.payload[2] = 4;

    nav5_message_tx = ublox_frame_to_bytes(&nav5_frame_tx); // <-- wird im Original nie freigegeben

    uint8_t real_payload_ack[2] = {0};
    uint8_t response_ack[16];
    size_t response_ack_len;
    build_valid_message(UBX_ACK_CLASS, UBX_ACK_ACK_MESSAGE, real_payload_ack, 2, response_ack, &response_ack_len);
    mock_set_response(response_ack, response_ack_len);
    UbloxMessage* ack = ublox_i2c_transfer(nav5_message_tx, UBX_ACK_ACK_MESSAGE_LENGTH);
    assert(ack != NULL);

    ublox_message_free(nav5_message_rx);
    ublox_message_free(ack);

    if(use_fix) {
        ublox_message_free(nav5_message_tx); // <-- FIX: fehlender Aufruf im Original
    }
    // ALT: nav5_message_tx (UbloxMessage-Struct + 44-Byte-Puffer) bleibt haengen.
}

int main(int argc, char** argv) {
    if(argc != 2 || (strcmp(argv[1], "alt") != 0 && strcmp(argv[1], "fix") != 0)) {
        fprintf(stderr, "Usage: %s alt|fix\n", argv[0]);
        return 2;
    }
    int use_fix = strcmp(argv[1], "fix") == 0;

    printf("=== FUND A: CFG-PMS Payload-Offset (protokollunabhaengiger Beweis) ===\n");
    scenario_fund_a_cfg_pms_offset();

    printf("=== FUND B: TX-Nachrichten-Lecks (%s), je 50 Zyklen ===\n", use_fix ? "NEU/Fix" : "ALT");
    for(int i = 0; i < 50; i++) {
        scenario_fund_b_sync_time_leak(use_fix);
        scenario_fund_b_nav5_write_leak(use_fix);
    }
    printf("Alle Pruefungen durchgelaufen (LeakSanitizer entscheidet beim Prozessende "
           "ueber Fund B; Fund A wird bereits oben per assert() hart verifiziert).\n");
    return 0;
}
