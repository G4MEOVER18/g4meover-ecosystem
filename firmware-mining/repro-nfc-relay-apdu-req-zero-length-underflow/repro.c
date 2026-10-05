// Host-Mini-Repro: as_reader_worker_task() (applications/external/nfc_relay, Quelle
// leommxj/nfc_relay, gebuendelt in RogueMaster/flipperzero-firmware-wPlugins Branch 420,
// applications/external/nfc_relay/as_reader_worker.c) behandelt einen ueber die
// UART-Relaisverbindung empfangenen NfcRelayPacketApduReq-Paket wie folgt (ALT, Zeilen
// 294-309):
//
//   recv_packet = comm_wait_packet(as_reader_worker->comm, NfcRelayPacketApduReq);
//   if(!recv_packet) { continue; }
//   // drop PCB
//   bit_buffer_copy_bytes(
//       as_reader_worker->bitbuffer_tx, &recv_packet->buf[1], recv_packet->len - 1);
//
// `recv_packet->len` ist ein von der Gegenseite der Relaisverbindung (zweiter Flipper,
// laut eigenem README der App wahlweise per direktem UART-Draht ODER ueber zwei
// ESP-AT-WLAN-Module durch einen Cloud-Relay-Server aus dem Internet gebrueckt) frei
// gewaehlter uint8_t-Wert -- comm_wait_packet() (comm.c) prueft nur den Pakettyp, nicht
// die Laenge. uart_worker_task() (uart_worker.c) erzeugt fuer ein Paket mit len==0 einen
// vollkommen gueltigen Null-Byte-Payload-Frame (eigener "if(len == 0)"-Zweig, malloc() nur
// fuer den Header, kein zusaetzliches buf[]).
//
// Trifft ein solches Paket (typ=NfcRelayPacketApduReq, len=0) ein, wertet der Aufrufer
// `recv_packet->len - 1` aus: `len` ist uint8_t, die Subtraktion promotet nach `int`
// (0 - 1 = -1), und dieser Wert wird anschliessend an den `size_t size_bytes`-Parameter von
// bit_buffer_copy_bytes() (lib/toolbox/bit_buffer.c) uebergeben -- auf der 32-Bit-
// Zielarchitektur (STM32WB55/Cortex-M4, size_t == uint32_t) wird daraus 0xFFFFFFFF.
// bit_buffer_copy_bytes() selbst prueft vorab `furi_check(buf->capacity_bytes >=
// size_bytes)`; bitbuffer_tx ist mit bit_buffer_alloc(256) allokiert (256 Byte Kapazitaet),
// die Pruefung schlaegt also fehl -- furi_check() stuerzt den Prozess ab (im echten
// Geraet: furi_crash(), Geraeteabsturz/-neustart). Der Absturz passiert VOR dem eigentlichen
// memcpy(), es handelt sich also um einen sauberen, aber aus der Ferne ueber die
// Relaisverbindung auslösbaren Denial-of-Service, nicht um eine Speicherkorruption.
//
// furi_check() wird hier als assert-artiger Prozessabbruch nachgebildet (reale Kette:
// furi_check -> furi_crash() -> Geraeteanhalt); bit_buffer_copy_bytes()/BitBuffer bilden
// nur den fuer den Bug relevanten Kontrollfluss nach (Kapazitaetspruefung + memcpy, kein
// vollstaendiger BitBuffer-Nachbau noetig). size_t wird hier explizit als uint32_t
// nachgebildet, um die 32-Bit-Zielarchitektur (nicht den ggf. 64-Bit-Host) abzubilden --
// auf einem 64-Bit-Host waere der Unterlauf ebenso real (size_t waere dort 0xFFFFFFFF...FF),
// nur der genaue Zahlenwert unterscheidet sich.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef uint32_t target_size_t; // size_t auf STM32WB55 (Cortex-M4, 32-Bit)

// Nachbildung von furi_check(): bricht den Prozess ab, wenn cond falsch ist.
#define FURI_CHECK(cond)     \
    do {                     \
        if(!(cond)) abort(); \
    } while(0)

// ---- Nachbildung von packet.h::NfcRelayPacket ----
typedef struct {
    uint8_t typ;
    uint8_t len;
    uint8_t buf[]; // bei len==0: 0 Byte allokiert (siehe uart_worker_task(), len==0-Zweig)
} NfcRelayPacket;

static NfcRelayPacket* make_packet(uint8_t len, const uint8_t* payload) {
    NfcRelayPacket* p = malloc(sizeof(NfcRelayPacket) + len);
    p->typ = 0x63; // NfcRelayPacketApduReq
    p->len = len;
    if(len > 0) memcpy(p->buf, payload, len);
    return p;
}

// ---- Nachbildung von toolbox/bit_buffer.h::BitBuffer + bit_buffer_copy_bytes() ----
typedef struct {
    uint8_t* data;
    target_size_t capacity_bytes;
    target_size_t size_bits;
    uint8_t copied[256]; // Wachposten-Kopie zum Nachweis des tatsaechlich kopierten Inhalts
} BitBuffer;

static BitBuffer* bitbuffer_alloc(target_size_t capacity_bytes) {
    BitBuffer* b = malloc(sizeof(BitBuffer));
    b->data = b->copied;
    b->capacity_bytes = capacity_bytes;
    b->size_bits = 0;
    memset(b->copied, 0xAA, sizeof(b->copied)); // Wachposten-Muster
    return b;
}

// 1:1 aus lib/toolbox/bit_buffer.c::bit_buffer_copy_bytes() transkribiert.
static void bit_buffer_copy_bytes_mock(BitBuffer* buf, const uint8_t* data, target_size_t size_bytes) {
    FURI_CHECK(buf);
    FURI_CHECK(data);
    FURI_CHECK(buf->capacity_bytes >= size_bytes);

    memcpy(buf->data, data, size_bytes);
    buf->size_bits = size_bytes * 8;
}

// ---- ALT: as_reader_worker.c Zeilen 294-309 (1:1 der fuer den Bug relevante Ausschnitt) ----
static void apdu_req_old(NfcRelayPacket* recv_packet, BitBuffer* bitbuffer_tx) {
    // drop PCB
    bit_buffer_copy_bytes_mock(bitbuffer_tx, &recv_packet->buf[1], (target_size_t)(recv_packet->len - 1));
}

// ---- NEU: additive Laengenpruefung vor der Subtraktion ----
static int apdu_req_new(NfcRelayPacket* recv_packet, BitBuffer* bitbuffer_tx) {
    if(recv_packet->len == 0) {
        return 0; // Paket wird verworfen, kein apdu_ready, kein Unterlauf
    }
    bit_buffer_copy_bytes_mock(bitbuffer_tx, &recv_packet->buf[1], (target_size_t)(recv_packet->len - 1));
    return 1;
}

typedef void (*old_fn_t)(NfcRelayPacket*, BitBuffer*);
typedef int (*new_fn_t)(NfcRelayPacket*, BitBuffer*);

static int run_old_isolated(uint8_t len, const uint8_t* payload) {
    pid_t pid = fork();
    if(pid == 0) {
        NfcRelayPacket* p = make_packet(len, payload);
        BitBuffer* b = bitbuffer_alloc(256);
        apdu_req_old(p, b);
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if(WIFSIGNALED(status)) return WTERMSIG(status);
    if(WIFEXITED(status) && WEXITSTATUS(status) != 0) return -1;
    return 0;
}

static int run_new_isolated(uint8_t len, const uint8_t* payload) {
    pid_t pid = fork();
    if(pid == 0) {
        NfcRelayPacket* p = make_packet(len, payload);
        BitBuffer* b = bitbuffer_alloc(256);
        apdu_req_new(p, b);
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if(WIFSIGNALED(status)) return WTERMSIG(status);
    if(WIFEXITED(status) && WEXITSTATUS(status) != 0) return -1;
    return 0;
}

static int check_crash_case(const char* label, uint8_t len, int expect_crash_old) {
    uint8_t payload[260];
    for(int i = 0; i < 260; i++) payload[i] = (uint8_t)(0x10 + i);

    int sig_old = run_old_isolated(len, payload);
    int sig_new = run_new_isolated(len, payload);

    printf("== %s (len=%u) ==\n", label, len);
    printf("  ALT: %s\n", sig_old != 0 ? "ABSTURZ (furi_check-Abbruch)" : "kein Absturz");
    printf("  NEU: %s\n\n", sig_new != 0 ? "ABSTURZ" : "kein Absturz");

    int old_matches = expect_crash_old ? (sig_old != 0) : (sig_old == 0);
    int new_ok = (sig_new == 0); // NEU darf in keinem Fall abstuerzen
    return old_matches && new_ok;
}

// Regression: fuer len>=1 muessen ALT und NEU byte-identischen Inhalt und identisches
// size_bits-Ergebnis liefern (NEU darf das normale Verhalten nicht veraendern).
static int check_regression_equal(uint8_t len) {
    uint8_t payload[260];
    for(int i = 0; i < 260; i++) payload[i] = (uint8_t)(0x10 + i);

    NfcRelayPacket* p_old = make_packet(len, payload);
    BitBuffer* b_old = bitbuffer_alloc(256);
    apdu_req_old(p_old, b_old);

    NfcRelayPacket* p_new = make_packet(len, payload);
    BitBuffer* b_new = bitbuffer_alloc(256);
    int accepted = apdu_req_new(p_new, b_new);

    int ok = accepted && (b_old->size_bits == b_new->size_bits) &&
             (memcmp(b_old->copied, b_new->copied, sizeof(b_old->copied)) == 0);

    free(p_old);
    free(b_old);
    free(p_new);
    free(b_new);
    return ok;
}

int main(void) {
    int all_ok = 1;

    // Fund: len==0 (gueltiger Null-Byte-Payload-Frame laut uart_worker_task()) --
    // ALT: recv_packet->len - 1 unterlaeuft zu 0xFFFFFFFF, bit_buffer_copy_bytes()s
    // Kapazitaetspruefung (256 >= 0xFFFFFFFF) schlaegt fehl -> furi_check-Abbruch.
    all_ok &= check_crash_case("Fund: leeres ApduReq-Paket (len=0)", 0, 1);

    // Randfall: len==1 (nur PCB-Byte, kein Nutzinhalt danach) -- nach Abzug des PCB-Byte
    // bleiben 0 Byte zu kopieren, 256 >= 0 ist wahr, kein Absturz in ALT oder NEU.
    all_ok &= check_crash_case("Randfall: minimales ApduReq-Paket (len=1)", 1, 0);

    // Sanity: normales ApduReq-Paket.
    all_ok &= check_crash_case("Sanity: normales ApduReq-Paket (len=5)", 5, 0);
    all_ok &= check_crash_case("Sanity: maximales ApduReq-Paket (len=255)", 255, 0);

    // Regression ueber den gesamten gueltigen Bereich: ALT und NEU muessen fuer jede
    // Laenge >=1 denselben Nutzinhalt liefern (NEU aendert das bestehende Verhalten nicht).
    int regression_ok = 1;
    for(int len = 1; len <= 255; len++) {
        regression_ok &= check_regression_equal((uint8_t)len);
    }
    printf("== Aequivalenztest ueber len=1..255 ==\n  %s\n\n",
           regression_ok ? "ALT und NEU liefern in allen 255 Faellen identischen Inhalt"
                          : "ABWEICHUNG gefunden");
    all_ok &= regression_ok;

    printf(all_ok ? "ERGEBNIS: bestaetigt -- ALT stuerzt bei len==0 ab, NEU nicht, "
                    "Verhalten fuer len>=1 unveraendert.\n"
                  : "ERGEBNIS: Abweichung von der Erwartung, siehe oben.\n");
    return all_ok ? 0 : 1;
}
