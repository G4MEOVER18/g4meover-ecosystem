/*
 * Host-Repro: SSID-Tag-Extraktion aus Argus-FlipperZero (ESP32-Begleitfirmware,
 * esp32/argus_esp32/argus_esp32.ino, sniffer_cb()).
 *
 * ALT und NEU sind 1:1 aus dem echten Funktionskoerper transkribiert (nur der
 * SSID-Tag-Extraktionsblock, isoliert von der restlichen Frame-Verarbeitung).
 * `body` simuliert die getaggten Parameter eines Beacon-/Probe-Response-Frames
 * ab `pkt->payload + 36`; `body_len = len - 36` ist die tatsaechlich vom
 * ESP32-Wifi-Treiber *empfangene* Restlaenge, waehrend `body[1]` (die vom
 * Sender deklarierte SSID-Tag-Laenge) unabhaengig davon beliebig sein kann --
 * ein Sender kann ein SSID-Information-Element mit deklarierter Laenge 32
 * senden, dessen Frame aber (ob durch absichtliche Fehlkonstruktion oder durch
 * normale Funkbedingungen) kuerzer endet als diese Deklaration verspricht.
 *
 * ALT prueft `sl` nur gegen die feste Obergrenze 32 (Puffergroesse von
 * `ssid[33]` auf der Zielseite), nicht gegen `body_len` -- liest damit bei
 * jeder kuerzer empfangenen Nachricht ueber das Ende des tatsaechlich
 * empfangenen `body`-Puffers hinaus. NEU klemmt zusaetzlich auf die
 * tatsaechlich verfuegbaren Bytes (`body_len - 2`).
 *
 * `body` wird hier exakt auf `body_len` Byte alloziert (wie es ein
 * DMA-Empfangspuffer mit exakter Framegroesse waere), damit ein Overread
 * unter AddressSanitizer zuverlaessig als heap-buffer-overflow sichtbar wird.
 * Jedes Szenario laeuft in einem eigenen fork()-Kindprozess, damit ein Crash
 * in einem Szenario den Rest des Testlaufs nicht abbricht.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* ---- ALT: 1:1 aus sniffer_cb(), SSID-Tag-Block ---- */
static void extract_ssid_old(const uint8_t* body, int body_len, char* ssid /* [33] */) {
    memset(ssid, 0, 33);
    if(body_len >= 2 && body[0] == 0) {
        int sl = body[1];
        if(sl > 32) sl = 32;
        for(int k = 0; k < sl; k++) {
            char c = (char)body[2 + k];
            ssid[k] = (c == ',' || c == '\n' || c == '\r' || c < 32) ? '_' : c;
        }
        ssid[sl] = '\0';
    }
}

/* ---- NEU: zusaetzliche Klemmung auf die tatsaechlich verfuegbaren Bytes ---- */
static void extract_ssid_new(const uint8_t* body, int body_len, char* ssid /* [33] */) {
    memset(ssid, 0, 33);
    if(body_len >= 2 && body[0] == 0) {
        int sl = body[1];
        if(sl > 32) sl = 32;
        int avail = body_len - 2; // Bytes, die nach dem Tag-Header tatsaechlich empfangen wurden
        if(sl > avail) sl = avail; // deklarierte Tag-Laenge kann die empfangene Framegroesse ueberschreiten
        for(int k = 0; k < sl; k++) {
            char c = (char)body[2 + k];
            ssid[k] = (c == ',' || c == '\n' || c == '\r' || c < 32) ? '_' : c;
        }
        ssid[sl] = '\0';
    }
}

typedef void (*extract_fn)(const uint8_t*, int, char*);

/* baut einen `body`-Puffer exakt auf `body_len` Byte, body[0]=0 (SSID-Tag-ID),
 * body[1]=declared_len (deklarierte Tag-Laenge), Rest mit erkennbarem Fuellmuster */
static uint8_t* build_body(int body_len, int declared_len) {
    uint8_t* b = malloc((size_t)body_len);
    for(int i = 0; i < body_len; i++) b[i] = (uint8_t)('A' + (i % 26));
    if(body_len >= 1) b[0] = 0;
    if(body_len >= 2) b[1] = (uint8_t)declared_len;
    return b;
}

static int run_case(const char* name, extract_fn fn, int body_len, int declared_len, int expect_crash) {
    pid_t pid = fork();
    if(pid == 0) {
        uint8_t* body = build_body(body_len, declared_len);
        char ssid[33];
        fn(body, body_len, ssid);
        fprintf(stderr, "[child] %s: ssid=\"%s\" len=%zu (no crash)\n", name, ssid, strlen(ssid));
        free(body);
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    int crashed = !(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    const char* verdict = (crashed == expect_crash) ? "OK  " : "FAIL";
    printf(
        "[%s] %-28s body_len=%-3d declared_len=%-3d -> %s\n",
        verdict,
        name,
        body_len,
        declared_len,
        crashed ? "CRASHED (ASan overread)" : "survived");
    return (crashed == expect_crash) ? 0 : 1;
}

int main(void) {
    int fails = 0;

    printf("=== ALT (sniffer_cb SSID-Tag-Block, verbatim) ===\n");
    // Regression: Tag-Laenge passt exakt zur empfangenen Restlaenge -> kein Overread
    fails += run_case("ALT regression (sl==avail)", extract_ssid_old, 34, 32, 0);
    // Regression: leeres SSID (sl=0)
    fails += run_case("ALT regression (sl==0)", extract_ssid_old, 2, 0, 0);
    // Fund: deklarierte Laenge 32, aber Frame endet nach nur 8 Nutzbytes
    fails += run_case("ALT FUND (truncated, sl=32>avail=8)", extract_ssid_old, 10, 32, 1);
    // Fund-Grenzfall: nur Tag-Header empfangen, keine Nutzbytes, Laenge trotzdem deklariert
    fails += run_case("ALT FUND (header-only, avail=0)", extract_ssid_old, 2, 32, 1);

    printf("\n=== NEU (zusaetzliche avail-Klemmung) ===\n");
    fails += run_case("NEU regression (sl==avail)", extract_ssid_new, 34, 32, 0);
    fails += run_case("NEU regression (sl==0)", extract_ssid_new, 2, 0, 0);
    fails += run_case("NEU truncated (sl=32>avail=8)", extract_ssid_new, 10, 32, 0);
    fails += run_case("NEU header-only (avail=0)", extract_ssid_new, 2, 32, 0);

    /* Byte-fuer-Byte-Aequivalenz fuer alle Nicht-Fund-Faelle (ALT==NEU, wenn
     * die deklarierte Laenge innerhalb der tatsaechlich empfangenen Bytes liegt) */
    printf("\n=== Aequivalenz ALT/NEU im Normalfall (sl <= avail) ===\n");
    for(int body_len = 2; body_len <= 40; body_len++) {
        for(int declared_len = 0; declared_len <= 32; declared_len++) {
            int avail = body_len - 2;
            if(declared_len > avail) continue; // nur Normalfall, Fund-Faelle oben separat geprueft
            uint8_t* body = build_body(body_len, declared_len);
            char a[33], b[33];
            extract_ssid_old(body, body_len, a);
            extract_ssid_new(body, body_len, b);
            if(memcmp(a, b, 33) != 0) {
                printf(
                    "[FAIL] Diskrepanz bei body_len=%d declared_len=%d\n", body_len, declared_len);
                fails++;
            }
            free(body);
        }
    }
    printf("[OK  ] ALT und NEU identisch in allen Normalfaellen (sl <= avail)\n");

    printf("\n%s\n", fails == 0 ? "ALLE CHECKS BESTANDEN" : "FEHLER GEFUNDEN");
    return fails == 0 ? 0 : 1;
}
