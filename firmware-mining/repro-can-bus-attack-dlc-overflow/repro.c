// Host-Repro: applications/external/can_bus_attack/libraries/mcp_can_2515.c (RogueMaster-Bundle,
// Branch "420"), read_frame() -- liest einen per MCP2515 empfangenen CAN-Frame per SPI aus den
// RXBn-Registern des Controllers.
//
// Original (1:1 uebernommen, nur die fuer diesen Fund relevante Stelle):
//
//     furi_hal_spi_bus_rx(spi, &data_ctrl, 1, TIMEOUT);
//     frame->data_lenght = data_ctrl & MCP_DLC_MASK;      // MCP_DLC_MASK == 0x0F, 4 Bit -> 0..15
//     frame->req = (data_ctrl & MCP_RTR_MASK) ? 1 : 0;
//     for(uint8_t i = 0; i < frame->data_lenght; i++) {
//         furi_hal_spi_bus_rx(spi, &frame->buffer[i], 1, TIMEOUT);  // frame->buffer ist uint8_t[8]
//     }
//
// Das DLC-Feld eines CAN-Rahmens ist im Protokoll 4 Bit breit (Werte 0-15); gueltige CAN-2.0-Nutzlast
// ist auf 8 Byte begrenzt, aber weder das Protokoll selbst noch der MCP2515-Chip erzwingen das
// hardwareseitig beim Empfang -- ein manipulierter oder fehlerhafter Sender auf dem Bus kann ein
// DLC-Feld von 9-15 uebertragen, das der Controller unverändert im RXBnDLC-Register ablegt. Die
// hier gemaskte `data_ctrl & MCP_DLC_MASK` liefert dann einen Wert > 8, mit dem die anschliessende
// Schleife bis zu 15 Byte in das nur 8 Byte grosse `frame->buffer` schreibt -- ein Schreibzugriff
// ausserhalb des deklarierten Arrays, da `read_frame()` an allen drei Aufrufstellen
// (SnifferOption.c:194, pid_library.c:86/109, uds_library.c:55) mit einem lokalen
// `CANFRAME frame;` auf dem Stack aufgerufen wird. Auf dem hier verwendeten Host/ABI liegt
// `buffer` am Ende des 16 Byte grossen `CANFRAME` (siehe sizeof/offsetof-Ausgabe des Programms)
// mit genau 1 Byte eigener Tail-Padding dahinter -- DLC=9 schreibt dadurch noch innerhalb des
// 16-Byte-Stack-Objekts (von ASan nicht erkannt, aber bereits ausserhalb des Array-Vertrags,
// siehe Fundfall 1), ab DLC=10 liegt der Schreibzugriff nachweislich ausserhalb des Objekts
// (von ASan als stack-buffer-overflow erkannt, Fundfall 2/3). Dieselbe fehlende Pruefung
// existiert in der byteidentischen Kopie dieser Datei im noch offenen `canbus`-Kandidaten
// desselben Clusters.
//
// Die etablierte, verbreitete MCP2515-Host-Bibliothek, von der dieser Treiber abstammt
// (coryjfowler/Seeed MCP_CAN, `mcp_can.cpp::readMsg()`), begrenzt genau an dieser Stelle explizit
// auf 8 ("if (dlc > 8) dlc = 8;") -- dieser Port hat die Begrenzung beim Uebertragen verloren.
//
// NEU fuegt dieselbe Begrenzung als zusaetzliche Zeile direkt nach der Maskierung ein:
// "if(frame->data_lenght > MAX_LEN) frame->data_lenght = MAX_LEN;" (identischer Patch wie
// firmware-mining/can-bus-attack-app-port/mcp_can_2515_c.patch).
//
// Jeder potenziell ueberlaufende Testfall laeuft isoliert in einem fork()-Kindprozess (ein
// ASan-Abbruch eines Falls darf den Testlauf selbst nicht beenden).
//
//   gcc -std=c11 -O0 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -o repro_o0 repro.c && ./repro_o0
//   gcc -std=c11 -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -o repro_o1 repro.c && ./repro_o1

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_LEN 8

// Nachbildung von CANFRAME (mcp_can_2515.h), identische Feldreihenfolge/-groessen.
typedef struct {
    uint32_t canId;
    uint8_t ext;
    uint8_t req;
    uint8_t data_lenght;
    uint8_t buffer[MAX_LEN];
} CANFRAME;

#define MCP_SIDH 0
#define MCP_SIDL 1
#define MCP_EID8 2
#define MCP_EID0 3
#define MCP_TXB_EXIDE_M 0x08
#define MCP_DLC_MASK    0x0F
#define MCP_RTR_MASK    0x40
#define TIMEOUT      100
#define TIMEOUT_SPI  100

// furi_hal-freie Nachbildung des SPI-Busses: liest aus einem global gesetzten "Draht"-Puffer,
// genau die Byte-fuer-Byte-Reihenfolge, in der read_frame() sie per furi_hal_spi_bus_rx() abholt.
static const uint8_t* g_wire;
static size_t g_wire_len;
static size_t g_wire_pos;

static void mock_spi_bus_rx(uint8_t* dst, size_t len) {
    for(size_t i = 0; i < len; i++) {
        dst[i] = (g_wire_pos < g_wire_len) ? g_wire[g_wire_pos] : 0;
        g_wire_pos++;
    }
}

// ALT: read_frame() -- 1:1 Original-Logik (mcp_can_2515.c, Branch "420"), ohne Begrenzung von
// frame->data_lenght auf MAX_LEN.
static void read_frame_orig(CANFRAME* frame) {
    uint8_t data[4];
    uint8_t data_ctrl = 0;

    mock_spi_bus_rx(data, sizeof(data));

    uint32_t id = ((uint32_t)data[MCP_SIDH] << 3) + (data[MCP_SIDL] >> 5);
    uint8_t ext = 0;

    if((data[MCP_SIDL] & MCP_TXB_EXIDE_M) == MCP_TXB_EXIDE_M) {
        id = (id << 2) + (data[MCP_SIDL] & 0x03);
        id = (id << 8) + data[MCP_EID8];
        id = (id << 8) + data[MCP_EID0];
        ext = 1;
    }

    frame->canId = id;
    frame->ext = ext;

    mock_spi_bus_rx(&data_ctrl, 1);

    frame->data_lenght = data_ctrl & MCP_DLC_MASK; // <-- ALT: keine Begrenzung auf MAX_LEN
    frame->req = (data_ctrl & MCP_RTR_MASK) ? 1 : 0;

    for(uint8_t i = 0; i < frame->data_lenght; i++) {
        mock_spi_bus_rx(&frame->buffer[i], 1); // <-- schreibt bei data_lenght > 8 ueber buffer[8..] hinaus
    }
}

// NEU: dieselbe Logik, plus die additive Begrenzung direkt nach der Maskierung (identischer
// Patch wie firmware-mining/can-bus-attack-app-port/mcp_can_2515_c.patch).
static void read_frame_fixed(CANFRAME* frame) {
    uint8_t data[4];
    uint8_t data_ctrl = 0;

    mock_spi_bus_rx(data, sizeof(data));

    uint32_t id = ((uint32_t)data[MCP_SIDH] << 3) + (data[MCP_SIDL] >> 5);
    uint8_t ext = 0;

    if((data[MCP_SIDL] & MCP_TXB_EXIDE_M) == MCP_TXB_EXIDE_M) {
        id = (id << 2) + (data[MCP_SIDL] & 0x03);
        id = (id << 8) + data[MCP_EID8];
        id = (id << 8) + data[MCP_EID0];
        ext = 1;
    }

    frame->canId = id;
    frame->ext = ext;

    mock_spi_bus_rx(&data_ctrl, 1);

    frame->data_lenght = data_ctrl & MCP_DLC_MASK;
    if(frame->data_lenght > MAX_LEN) frame->data_lenght = MAX_LEN; // <-- NEU
    frame->req = (data_ctrl & MCP_RTR_MASK) ? 1 : 0;

    for(uint8_t i = 0; i < frame->data_lenght; i++) {
        mock_spi_bus_rx(&frame->buffer[i], 1);
    }
}

typedef void (*ReadFn)(CANFRAME*);

// Baut die Byte-Sequenz, die read_frame() vom MCP2515-Controller abholt: 4 Byte Kopf
// (SIDH/SIDL/EID8/EID0, hier Standard-ID 0x123, nicht extended), 1 Steuerbyte (DLC-Nibble in den
// unteren 4 Bit, RTR=0), gefolgt von "raw_dlc" Datenbyte (0xAA, 0xAB, ... als Marker).
static void build_wire(uint8_t* out, uint8_t raw_dlc) {
    uint16_t std_id = 0x123;
    out[MCP_SIDH] = (uint8_t)(std_id >> 3);
    out[MCP_SIDL] = (uint8_t)((std_id & 0x07) << 5); // kein EXIDE-Bit -> Standard-ID
    out[MCP_EID8] = 0;
    out[MCP_EID0] = 0;
    out[4] = raw_dlc & MCP_DLC_MASK; // Steuerbyte: DLC-Nibble, RTR=0
    for(uint8_t i = 0; i < raw_dlc; i++) {
        out[5 + i] = (uint8_t)(0xA0 + i);
    }
}

// Ruft read() mit dem gegebenen "Draht"-Inhalt isoliert auf; frame liegt als lokale (Stack-)
// Variable in derselben Funktion, exakt wie "CANFRAME frame = app->can_frame;" in
// SnifferOption.c::worker_sniffing() bzw. die lokalen CANFRAME-Variablen in pid_library.c/
// uds_library.c.
static void run_read(ReadFn read, const uint8_t* wire, size_t wire_len) {
    g_wire = wire;
    g_wire_len = wire_len;
    g_wire_pos = 0;
    CANFRAME frame;
    memset(&frame, 0, sizeof(frame));
    read(&frame);
    printf(
        "    canId=0x%lx ext=%u req=%u data_lenght=%u\n",
        (unsigned long)frame.canId,
        frame.ext,
        frame.req,
        frame.data_lenght);
}

// Fuehrt fn(wire) in einem Kindprozess aus; gibt zurueck, ob der Kindprozess ungewoehnlich
// beendet wurde (Signal, insbesondere SIGABRT ueber ASan).
static bool run_isolated(ReadFn read, const uint8_t* wire, size_t wire_len) {
    pid_t pid = fork();
    if(pid == 0) {
        run_read(read, wire, wire_len);
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFSIGNALED(status) || (WIFEXITED(status) && WEXITSTATUS(status) != 0);
}

static void print_case(const char* name, bool crashed) {
    printf("    %s: %s\n", name, crashed ? "ABSTURZ (von ASan erkannt)" : "kein Absturz");
}

int main(void) {
    printf(
        "sizeof(CANFRAME) == %zu, offsetof(buffer) == %zu, sizeof(buffer) == %d "
        "(Original, unveraendert)\n\n",
        sizeof(CANFRAME),
        offsetof(CANFRAME, buffer),
        MAX_LEN);

    printf("== Regressionsfall: gueltiger Standard-Frame, DLC=8 (voller, zulaessiger Rahmen) ==\n");
    {
        uint8_t wire[4 + 1 + 8];
        build_wire(wire, 8);
        printf("  ALT:\n");
        run_read(read_frame_orig, wire, sizeof(wire));
        printf("  NEU:\n");
        run_read(read_frame_fixed, wire, sizeof(wire));
        printf("  (erwartet: data_lenght=8 bei beiden, identisches Verhalten)\n");
    }

    printf(
        "\n== Fundfall 1: manipuliertes/fehlerhaftes DLC-Nibble = 9 (1 Byte ausserhalb des "
        "deklarierten 8-Byte-Arrays) ==\n");
    {
        uint8_t wire[4 + 1 + 9];
        build_wire(wire, 9);
        print_case(
            "ALT (schreibt 9. Byte in frame->buffer[8] -- auf diesem Host faellt das noch in "
            "die eigene Tail-Padding des CANFRAME-Stack-Objekts, siehe sizeof/offsetof oben, "
            "daher hier kein ASan-Fund; auf einem Compiler/ABI ohne diese Padding-Reserve, z. B. "
            "wenn CANFRAME um ein weiteres Feld erweitert wird, waere dies ein echter "
            "Speicherfehler)",
            run_isolated(read_frame_orig, wire, sizeof(wire)));
        print_case("NEU (begrenzt data_lenght auf 8, schreibt gar nicht erst ueber buffer[7] hinaus)",
                    run_isolated(read_frame_fixed, wire, sizeof(wire)));
        printf("  ALT direkt aufgerufen:\n");
        run_read(read_frame_orig, wire, sizeof(wire));
        printf("  NEU direkt aufgerufen:\n");
        run_read(read_frame_fixed, wire, sizeof(wire));
        printf("  (erwartet: ALT data_lenght=9 -- bereits ausserhalb des Array-Vertrags --, NEU data_lenght=8)\n");
    }

    printf(
        "\n== Fundfall 2: DLC-Nibble = 10 (minimaler auf diesem Host von ASan erkannter "
        "Ueberlauf, 2 Byte) ==\n");
    {
        uint8_t wire[4 + 1 + 10];
        build_wire(wire, 10);
        print_case(
            "ALT (schreibt 10 Byte in frame->buffer[8..9], 1 Byte ueber die Tail-Padding hinaus)",
            run_isolated(read_frame_orig, wire, sizeof(wire)));
        print_case("NEU (begrenzt data_lenght auf 8, kein Overflow)",
                    run_isolated(read_frame_fixed, wire, sizeof(wire)));
    }

    printf("\n== Fundfall 3: maximales DLC-Nibble = 15 (maximaler Ueberlauf, 7 Byte) ==\n");
    {
        uint8_t wire[4 + 1 + 15];
        build_wire(wire, 15);
        print_case(
            "ALT (schreibt 15 Byte in frame->buffer[8..14], 6 Byte ueber die Tail-Padding hinaus)",
            run_isolated(read_frame_orig, wire, sizeof(wire)));
        print_case("NEU (begrenzt data_lenght auf 8, kein Overflow)",
                    run_isolated(read_frame_fixed, wire, sizeof(wire)));
    }

    printf("\n== Vollstaendiger Sweep ueber alle 16 moeglichen DLC-Nibble-Werte (0..15) ==\n");
    {
        int neu_correct = 0;
        int alt_crashes_over_8 = 0;
        int over_8_count = 0;
        for(unsigned raw_dlc = 0; raw_dlc <= 15; raw_dlc++) {
            uint8_t wire[4 + 1 + 15];
            build_wire(wire, (uint8_t)raw_dlc);
            size_t wire_len = 5 + raw_dlc;

            if(raw_dlc <= 8) {
                // Gueltiger Bereich: ALT und NEU muessen sich direkt (ohne Isolation) identisch
                // und sicher verhalten.
                g_wire = wire;
                g_wire_len = wire_len;
                g_wire_pos = 0;
                CANFRAME f_old;
                memset(&f_old, 0, sizeof(f_old));
                read_frame_orig(&f_old);

                g_wire_pos = 0;
                CANFRAME f_new;
                memset(&f_new, 0, sizeof(f_new));
                read_frame_fixed(&f_new);

                if(memcmp(&f_old, &f_new, sizeof(f_old)) == 0 && f_new.data_lenght == raw_dlc) {
                    neu_correct++;
                }
            } else {
                over_8_count++;
                bool alt_crashed = run_isolated(read_frame_orig, wire, wire_len);
                if(alt_crashed) alt_crashes_over_8++;

                g_wire = wire;
                g_wire_len = wire_len;
                g_wire_pos = 0;
                CANFRAME f_new;
                memset(&f_new, 0, sizeof(f_new));
                read_frame_fixed(&f_new);
                if(f_new.data_lenght == MAX_LEN) neu_correct++;
            }
        }
        printf(
            "  DLC 0..8 (9 Werte) + DLC 9..15 (%d Werte): NEU liefert in %d/16 Faellen das "
            "erwartete, sichere Ergebnis\n",
            over_8_count,
            neu_correct);
        printf(
            "  DLC 9..15 (%d Werte): ALT loest in %d/%d Faellen einen von ASan erkannten "
            "Stack-Overflow aus\n",
            over_8_count,
            alt_crashes_over_8,
            over_8_count);
    }

    printf(
        "\nOK: Fuer DLC 0..8 (gueltiger Bereich) verhalten sich ALT und NEU identisch und sicher.\n"
        "Fuer ein manipuliertes/fehlerhaftes DLC-Nibble von 9..15 schreibt ALT bereits ausserhalb\n"
        "des deklarierten 8-Byte-Arrays (frame->buffer[8..14]) -- auf diesem Host faellt nur der\n"
        "erste ueberschuessige Byte (DLC=9) noch in die eigene Tail-Padding des CANFRAME-Objekts\n"
        "und bleibt dadurch von ASan unbemerkt; ab DLC=10 erkennt AddressSanitizer den Schreibzugriff\n"
        "zuverlaessig als stack-buffer-overflow. NEU begrenzt data_lenght kontrolliert auf 8, ohne\n"
        "das Verhalten im gueltigen Bereich (DLC 0..8) zu aendern.\n");
    return 0;
}
