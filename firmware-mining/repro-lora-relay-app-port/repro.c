/*
 * Host-Repro: LoRa Relay (ElectronicCats/flipper-SX1262-LoRa, applications_user/lora_app/
 * lora_relay.c, lora_view_sniffer_draw_callback()) -- stack-buffer-overflow write beim
 * Aufbau der JSON-Log-Zeile ("Recording"-Modus).
 *
 * Die betroffene Funktion baut final_string[400] per
 *     snprintf(final_string, 666, "...%s...", ..., asciiBuff);
 * -- die 666 ist ein Literal, keine sizeof(final_string) (die waere 400). asciiBuff ist die
 * Hex-Darstellung des zuletzt per LoRa empfangenen, vollstaendig fremdbestimmten Pakets (bis zu
 * 255 Byte Nutzlast -> bis zu 510 Hex-Zeichen). snprintf() selbst haelt sich an die angegebene
 * Groesse (666), schreibt aber bei einer hinreichend grossen Nutzlast weit ueber das tatsaechlich
 * nur 400 Byte grosse Stack-Array hinaus.
 *
 * Dieses Repro transkribiert final_string[400] + den exakten snprintf()-Aufruf 1:1 (Konstanten,
 * Formatstring, Feldreihenfolge) und ersetzt nur die drei Firmware-Aufrufe
 * (furi_hal_rtc_get_datetime/getRSSI/furi_string_get_cstr) durch feste, realistische
 * Platzhalterwerte -- die eigentliche Pufferlogik ist unveraendert.
 *
 * Fuenf Szenarien, jedes in einem eigenen Kindprozess (fork-isoliert, damit ein ASan-Abort in
 * einem Szenario die anderen nicht verhindert):
 *   1) Regression:   kleines Paket (20 Byte),  ORIGINAL (666)       -> darf NICHT ueberlaufen
 *   2) Fund:         Paket ab Schwelle (140 Byte), ORIGINAL (666)   -> MUSS ueberlaufen (ASan)
 *   3) Fund (worst case): maximales LoRa-Paket (255 Byte), ORIGINAL -> MUSS ueberlaufen (ASan)
 *   4) Fix:          maximales LoRa-Paket (255 Byte), FIX (sizeof)  -> darf NICHT ueberlaufen
 *   5) Aequivalenztest: kleines Paket (20 Byte), ORIGINAL vs. FIX   -> identischer String
 *
 * Kompiliert/getestet unter:
 *   gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined \
 *       -O0 -o repro_O0 repro.c && ./repro_O0
 *   (identisch bei -O1)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* Verbatim aus lora_relay.c */
#define TIME_LEN 12
#define DATE_LEN 14
#define CLOCK_TIME_FORMAT     "%.2d:%.2d:%.2d"
#define CLOCK_ISO_DATE_FORMAT "%.4d-%.2d-%.2d"

static char asciiBuff[512];

static void bytesToAsciiHex(const unsigned char* buffer, unsigned char length) {
    unsigned i;
    for(i = 0; i < length; ++i) {
        asciiBuff[i * 2] = "0123456789ABCDEF"[buffer[i] >> 4];
        asciiBuff[i * 2 + 1] = "0123456789ABCDEF"[buffer[i] & 0x0F];
    }
    asciiBuff[length * 2] = '\0';
}

/* 1:1 Transkription von lora_view_sniffer_draw_callback()s JSON-Zeilenaufbau.
 * out muss mindestens 700 Byte gross sein (dient hier nur dazu, das Ergebnis fuer den
 * Aequivalenztest herauszureichen -- im Original gibt es diesen Ausgabeparameter nicht,
 * final_string bleibt dort lokal). */
/* Die FIXED-Variante begrenzt absichtlich auf sizeof(final_string) -- GCC meldet dafuer
 * -Wformat-truncation, weil asciiBuff theoretisch laenger als der Puffer sein kann. Genau
 * dieses kontrollierte Abschneiden statt eines Pufferueberlaufs ist der gewuenschte Fix, daher
 * hier bewusst unterdrueckt statt "behoben". */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
static void build_final_string(const char* freq_str, const char* bw_name, const char* sf_name,
                                int rssi, int use_fix, char* out, size_t out_cap) {
    char time_string[TIME_LEN];
    char date_string[DATE_LEN];
    snprintf(time_string, TIME_LEN, CLOCK_TIME_FORMAT, 12, 34, 56);
    snprintf(date_string, DATE_LEN, CLOCK_ISO_DATE_FORMAT, 2026, 10, 13);

    char final_string[400]; /* exakte Groesse wie im Original */

    if(!use_fix) {
        /* ORIGINAL (Fund): Literal 666 statt sizeof(final_string) (== 400) */
        snprintf(
            final_string,
            666,
            "{\"date\":\"%s\", \"time\":\"%s\", \"frequency\":\"%s\", \"bw\":\"%s\", \"sf\":\"%s\", \"RSSI\":\"%d\", \"payload\":\"%s\"}",
            date_string,
            time_string,
            freq_str,
            bw_name,
            sf_name,
            rssi,
            asciiBuff);
    } else {
        /* FIX: an die tatsaechliche Puffergroesse gebunden */
        snprintf(
            final_string,
            sizeof(final_string),
            "{\"date\":\"%s\", \"time\":\"%s\", \"frequency\":\"%s\", \"bw\":\"%s\", \"sf\":\"%s\", \"RSSI\":\"%d\", \"payload\":\"%s\"}",
            date_string,
            time_string,
            freq_str,
            bw_name,
            sf_name,
            rssi,
            asciiBuff);
    }

    if(out) {
        size_t n = strlen(final_string);
        if(n >= out_cap) n = out_cap - 1;
        memcpy(out, final_string, n);
        out[n] = '\0';
    }
}
#pragma GCC diagnostic pop

/* Simuliert ein per LoRa empfangenes Paket aus bytesRead Byte und baut final_string daraus auf,
 * exakt wie lora_view_sniffer_draw_callback() es nach einem erfolgreichen Empfang tut. */
static void run_scenario(int bytes_read, int use_fix) {
    unsigned char payload[255];
    for(int i = 0; i < bytes_read; i++) {
        payload[i] = (unsigned char)(i * 7 + 1); /* deterministischer Fuellwert */
    }
    bytesToAsciiHex(payload, (unsigned char)bytes_read);
    build_final_string("915.000000 MHz", "125.00khz", "SF8", -85, use_fix, NULL, 0);
}

/* Fuehrt ein Szenario in einem Kindprozess aus. Rueckgabe: true, wenn der Prozess sauber
 * beendet wurde (kein ASan-Abort/Signal) -- also "kein Overflow aufgetreten". */
static int child_survived(int bytes_read, int use_fix) {
    fflush(NULL);
    pid_t pid = fork();
    if(pid == 0) {
        /* Kind: still arbeiten, keine weitere Ausgabe (vermeidet doppelte stdout-Puffer) */
        run_scenario(bytes_read, use_fix);
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int main(void) {
    int failures = 0;

    /* Szenario 1: Regression -- kleines Paket, ORIGINAL-Code, darf nicht ueberlaufen */
    {
        int survived = child_survived(20, 0);
        printf(
            "[1] Regression  bytesRead=20  ORIGINAL (666): %s\n",
            survived ? "PASS (kein Overflow)" : "FAIL (unerwarteter Crash)");
        if(!survived) failures++;
    }

    /* Szenario 2: Fund an der Schwelle -- 140 Byte Nutzlast, ORIGINAL-Code, MUSS ueberlaufen */
    {
        int survived = child_survived(140, 0);
        printf(
            "[2] Fund (Schwelle) bytesRead=140 ORIGINAL (666): %s\n",
            !survived ? "PASS (ASan stack-buffer-overflow wie erwartet)" :
                        "FAIL (kein Overflow erkannt)");
        if(survived) failures++;
    }

    /* Szenario 3: Fund worst case -- maximales LoRa-Paket (255 Byte), ORIGINAL-Code */
    {
        int survived = child_survived(255, 0);
        printf(
            "[3] Fund (worst case) bytesRead=255 ORIGINAL (666): %s\n",
            !survived ? "PASS (ASan stack-buffer-overflow wie erwartet)" :
                        "FAIL (kein Overflow erkannt)");
        if(survived) failures++;
    }

    /* Szenario 4: Fix -- identisches worst-case Paket, FIX-Code, darf NICHT ueberlaufen */
    {
        int survived = child_survived(255, 1);
        printf(
            "[4] Fix bytesRead=255 FIX (sizeof): %s\n",
            survived ? "PASS (kein Overflow)" : "FAIL (Fix verhindert Overflow nicht)");
        if(!survived) failures++;
    }

    /* Szenario 5: Aequivalenztest -- im nicht-ueberlaufenden Fall liefert der Fix exakt
       denselben String wie das Original (reine Groessenangabe geaendert, kein Verhaltenswechsel
       fuer den Normalfall). */
    {
        unsigned char payload[255];
        for(int i = 0; i < 20; i++) payload[i] = (unsigned char)(i * 7 + 1);
        bytesToAsciiHex(payload, 20);

        char out_orig[700];
        char out_fix[700];
        build_final_string("915.000000 MHz", "125.00khz", "SF8", -85, 0, out_orig, sizeof(out_orig));
        build_final_string("915.000000 MHz", "125.00khz", "SF8", -85, 1, out_fix, sizeof(out_fix));

        int equal = strcmp(out_orig, out_fix) == 0;
        printf(
            "[5] Aequivalenztest bytesRead=20 ORIGINAL vs. FIX: %s\n",
            equal ? "PASS (identischer String)" : "FAIL (Verhalten geaendert)");
        if(!equal) failures++;
    }

    if(failures == 0) {
        printf("\nAlle 5 Szenarien bestanden.\n");
        return 0;
    } else {
        printf("\n%d Szenario(e) fehlgeschlagen.\n", failures);
        return 1;
    }
}
