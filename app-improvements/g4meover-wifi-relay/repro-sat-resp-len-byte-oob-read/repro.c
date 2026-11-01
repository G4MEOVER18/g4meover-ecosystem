// Host-Repro: g4meover-wifi-relay::on_espnow_recv() vertraut dem LEN-Byte der
// ESP-NOW-Nutzlast, statt es gegen die tatsaechlich empfangene Puffergroesse (`len`,
// vom ESP-NOW-Treiber geliefert) zu pruefen. Dieses Repro bindet die UNVERAENDERTE
// ukfe_rf.c/.h (identisch zu src/ukfe_rf.c im Repo) und reproduziert exakt die beiden
// Codepfade aus main.cpp -- vor und nach dem Fix -- ohne deren Logik nachzubilden.
//
// Build:
//   gcc -std=c17 -Wall -Wextra -O0 -g -fsanitize=address,undefined
//       -fno-sanitize-recover=undefined repro.c ukfe_rf.c -o repro_asan
//   ./repro_asan
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include "ukfe_rf.h"

static const uint8_t SECRET[UKFE_RF_SECRET_LEN] = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00,
};

// ---- Codepfad VOR dem Fix: identisch zur ursprünglichen on_espnow_recv()-Zeile
// ("ukfe_rf_parse_frame(..., (size_t)data[0] + 1, ..., NULL)") -- `len` (die
// tatsaechlich von ESP-NOW zugestellte und damit die einzig fuer `data` gueltige
// Byteanzahl) wird dabei komplett ignoriert.
static bool vulnerable_on_espnow_recv_core(const uint8_t* data, int len, UkfeRfMessage* out) {
    if (len <= 0 || len > (int)UKFE_RF_MAX_FRAME) return false;
    return ukfe_rf_parse_frame(SECRET, data, (size_t)data[0] + 1, out, NULL);
}

// ---- Codepfad NACH dem Fix: LEN-Byte muss der tatsaechlich empfangenen Laenge
// entsprechen, bevor ukfe_rf_parse_frame() ueberhaupt mit `data` aufgerufen wird.
static bool fixed_on_espnow_recv_core(const uint8_t* data, int len, UkfeRfMessage* out) {
    if (len <= 0 || len > (int)UKFE_RF_MAX_FRAME) return false;
    if ((size_t)data[0] + 1 != (size_t)len) return false;
    return ukfe_rf_parse_frame(SECRET, data, (size_t)len, out, NULL);
}

// Ein 1-Byte-"ESP-NOW-Frame": der einzige empfangene Byte behauptet, LEN=54 zu sein
// (ergo eine volle 55-Byte-Nutzlast mit 40 Arg-Bytes) -- genau das Muster, mit dem ein
// gestutztes/gestoertes Funkpaket oder ein praepariertes Frame ankommen kann.
static uint8_t* make_len_lie_buffer(size_t real_len, uint8_t claimed_len_byte) {
    uint8_t* buf = (uint8_t*)malloc(real_len);
    assert(buf);
    memset(buf, 0, real_len);
    buf[0] = claimed_len_byte;
    return buf;
}

// Läuft in einem isolierten Kindprozess: wenn der Lesezugriff tatsächlich über die
// `real_len` Bytes der Allokation hinausgeht, meldet ASan einen
// heap-buffer-overflow und bricht den Prozess ab (SIGABRT) -- der Elternprozess
// wertet nur den Exit-Status aus, läuft selbst unter ASan weiter.
static int run_isolated(bool (*fn)(const uint8_t*, int, UkfeRfMessage*),
                         const uint8_t* data, int len) {
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        UkfeRfMessage msg;
        bool ok = fn(data, len, &msg);
        (void)ok;
        _exit(0); // nur erreicht, wenn kein Sanitizer-Abbruch auftrat
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return status;
}

int main(void) {
    int checks = 0, failures = 0;

#define CHECK(cond, desc) do { \
        checks++; \
        if (!(cond)) { failures++; fprintf(stderr, "FAIL: %s\n", desc); } \
        else fprintf(stderr, "ok:   %s\n", desc); \
    } while (0)

    // 1) VOR dem Fix: 1-Byte-Puffer, LEN-Byte behauptet eine 55-Byte-Nutzlast.
    //    ukfe_rf_parse_frame() liest in[1], in[2], in[8]... weit hinter der
    //    tatsaechlichen 1-Byte-Allokation -- ASan muss das als
    //    heap-buffer-overflow erkennen und den Kindprozess per SIGABRT beenden.
    {
        uint8_t* buf = make_len_lie_buffer(1, 54);
        int status = run_isolated(vulnerable_on_espnow_recv_core, buf, 1);
        CHECK(WIFSIGNALED(status) || (WIFEXITED(status) && WEXITSTATUS(status) != 0),
              "VOR Fix: 1-Byte-ESP-NOW-Frame mit gelogenem LEN-Byte loest "
              "Sanitizer-Abbruch (OOB-Lesen) aus");
        free(buf);
    }

    // 2) NACH dem Fix: derselbe boesartige 1-Byte-Puffer -- der Laengenabgleich
    //    (data[0]+1 != len) muss VOR jedem Zugriff auf data[1..] greifen und
    //    ukfe_rf_parse_frame() darf gar nicht erst aufgerufen werden.
    {
        uint8_t* buf = make_len_lie_buffer(1, 54);
        UkfeRfMessage msg;
        bool ok = fixed_on_espnow_recv_core(buf, 1, &msg);
        CHECK(!ok, "NACH Fix: dasselbe 1-Byte-Frame wird ohne OOB-Zugriff sicher "
                   "abgelehnt");
        free(buf);
    }

    // 3) Realistischerer Fall: ein tatsaechlich gueltiges, vollstaendig signiertes
    //    0-Argument-Frame (z.B. UkfeRfRespAck-Antwort eines Satelliten), aber beim
    //    Empfang um das letzte Byte (CRC-High-Byte) gestutzt uebergeben -- simuliert
    //    ein durch Funkstoerung verkuerzt zugestelltes ESP-NOW-Paket. `len` (die
    //    echte, verkuerzte Zustellgroesse) ist hier 1 kleiner als `data[0]+1`.
    {
        UkfeRfMessage m;
        ukfe_rf_make_simple(&m, (UkfeRfCmd)UkfeRfRespAck);
        uint8_t full[UKFE_RF_MAX_FRAME];
        size_t full_len = ukfe_rf_build_frame(SECRET, &m, full, sizeof(full));
        assert(full_len == UKFE_RF_HDR_OVERHEAD); // 0 Argumente

        size_t truncated_len = full_len - 1;
        uint8_t* buf = (uint8_t*)malloc(truncated_len);
        memcpy(buf, full, truncated_len); // data[0] behauptet weiterhin full_len

        int status = run_isolated(vulnerable_on_espnow_recv_core, buf, (int)truncated_len);
        CHECK(WIFSIGNALED(status) || (WIFEXITED(status) && WEXITSTATUS(status) != 0),
              "VOR Fix: um 1 Byte gestutztes echtes Antwort-Frame loest ebenfalls "
              "OOB-Lesen aus (nicht nur bei vorsaetzlicher Praeparation)");

        UkfeRfMessage out;
        bool ok = fixed_on_espnow_recv_core(buf, (int)truncated_len, &out);
        CHECK(!ok, "NACH Fix: dasselbe gestutzte Frame wird sicher abgelehnt, kein "
                   "OOB-Zugriff");
        free(buf);
    }

    // 4) Regression: ein echtes, vollstaendiges, korrekt zugestelltes Antwort-Frame
    //    muss nach dem Fix weiterhin akzeptiert werden (Laengenabgleich darf
    //    legitime Frames nicht blockieren).
    {
        UkfeRfMessage m;
        ukfe_rf_make_simple(&m, (UkfeRfCmd)UkfeRfRespAck);
        uint8_t full[UKFE_RF_MAX_FRAME];
        size_t full_len = ukfe_rf_build_frame(SECRET, &m, full, sizeof(full));

        UkfeRfMessage out;
        bool ok = fixed_on_espnow_recv_core(full, (int)full_len, &out);
        CHECK(ok && out.cmd == UkfeRfRespAck,
              "NACH Fix: echtes, vollstaendig zugestelltes Antwort-Frame wird "
              "weiterhin korrekt angenommen (keine Regression)");

        bool ok_old = vulnerable_on_espnow_recv_core(full, (int)full_len, &out);
        CHECK(ok_old && out.cmd == UkfeRfRespAck,
              "Regression (informativ): VOR Fix funktionierte der Normalfall "
              "(len == data[0]+1) ebenfalls -- der Bug trat nur bei Abweichung "
              "zwischen beiden auf");
    }

    // 5) Regression: ein Frame mit nicht-leeren Argumenten (ALEN > 0), korrekt und
    //    vollstaendig zugestellt, muss nach dem Fix ebenfalls weiterhin passieren.
    {
        UkfeRfMessage m;
        ukfe_rf_make_wifi_deauth(&m, (const uint8_t[6]){0xAA,0xBB,0xCC,0xDD,0xEE,0xFF}, 6);
        m.cmd = UkfeRfRespAck; // Antwortrichtung, Nutzlastform irrelevant fuer den Test
        uint8_t full[UKFE_RF_MAX_FRAME];
        size_t full_len = ukfe_rf_build_frame(SECRET, &m, full, sizeof(full));

        UkfeRfMessage out;
        bool ok = fixed_on_espnow_recv_core(full, (int)full_len, &out);
        CHECK(ok && out.arg_len == m.arg_len,
              "NACH Fix: Antwort-Frame mit Argumenten (ALEN>0) weiterhin korrekt "
              "angenommen");
    }

    fprintf(stderr, "\n%d/%d Prüfungen bestanden\n", checks - failures, checks);
    return failures ? 1 : 0;
}
