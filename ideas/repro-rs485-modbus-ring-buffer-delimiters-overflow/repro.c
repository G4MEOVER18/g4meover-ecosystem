// Host-Repro fuer den RingBuffer-delimiters[32]-Fund in
// ElectronicCats/flipper-rs485modbus (Commit ff50246, ModbusApp/modbus_ring_buffer/
// modbus_ring_buffer.c + Modbus.h + scenes/msgs_buffer_scene.c).
//
// Struct und writeRingBuffer() sind 1:1 aus Modbus.h / modbus_ring_buffer.c
// transkribiert (nur furi.h-Abhaengigkeiten entfernt). BuildCMDList()/
// OnItemEnterCB() aus msgs_buffer_scene.c werden als reine Index-Rekonstruktions-
// Logik nachgebaut (ohne GUI/submenu-Aufrufe), um zu zeigen, dass die Praxis-
// Konsequenz (falsche Nachricht beim Anzeigen/Senden) tatsaechlich eintritt.
//
// Fund: delimiters[32] hat 32 Slots, delimiterIdx (uint8_t) wird bei JEDEM
// Aufruf von writeRingBuffer() (= jede geparste Modbus-Nachricht) um 1 erhoeht,
// aber nur zurueckgesetzt, wenn writeIdx über 255 wraps -- nicht wenn
// delimiterIdx selbst 32 erreicht. Bei kurzen Nachrichten (z. B. 4-6 Byte
// Standard-RTU-Antworten) sind nach 32 Nachrichten erst 128-192 von 255 Bytes
// belegt, writeIdx wrapt also noch nicht -- delimiterIdx laeuft trotzdem ueber
// die eigentliche Grenze hinaus und schreibt in den Speicherbereich von
// ringBuffer[] (im Struct direkt nach delimiters[] platziert) hinein.
//
// gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#define Ring_Buf_Size 255

// 1:1 aus Modbus.h
typedef struct {
    uint8_t delimiters[32];
    uint8_t ringBuffer[Ring_Buf_Size];
    uint16_t writeIdx;
    uint8_t delimiterIdx;
    uint8_t readIdx;
} RingBuffer;

// 1:1 aus modbus_ring_buffer.c
RingBuffer* ring_buffer_alloc(void) {
    RingBuffer* buffer = malloc(sizeof(RingBuffer));
    buffer->writeIdx = 0;
    buffer->delimiterIdx = 0;
    for(uint8_t i = 0; i < 32; i++)
        buffer->delimiters[i] = 255;
    return buffer;
}
void ring_buffer_free(RingBuffer* buffer) {
    free(buffer);
}
void writeRingBuffer(RingBuffer* rb, uint8_t* buf, size_t len) {
    for(size_t i = 0; i < len; i++) {
        rb->ringBuffer[rb->writeIdx] = buf[i];
        if(i == len - 1) rb->delimiters[rb->delimiterIdx] = rb->writeIdx;
        if(++rb->writeIdx > 255) {
            rb->delimiterIdx = 0;
            rb->writeIdx = 0;
        }
    }
    rb->delimiterIdx++;
}

// Nachbau der Rekonstruktionslogik aus scenes/msgs_buffer_scene.c::OnItemEnterCB()
// (GUI-/scene_manager-Aufrufe entfernt, reine Index-/Kopierlogik 1:1 erhalten).
static size_t reconstruct_message(RingBuffer* rb, uint8_t index, uint8_t* out) {
    uint8_t start = index ? (uint8_t)(rb->delimiters[index - 1] + 1) : 0;
    size_t n = 0;
    for(uint8_t i = start; i <= rb->delimiters[index]; i++) {
        out[i - start] = rb->ringBuffer[i];
        n = (size_t)(i - start) + 1;
        if(i == 255) break; // Schutz gegen Endlosschleife im Repro, falls delimiters[index] < start
    }
    return n;
}

int main(void) {
    RingBuffer* rb = ring_buffer_alloc();

    // 40 kurze, klar unterscheidbare "Nachrichten" (4 Byte, wie eine minimale
    // Modbus-RTU-Antwort: Slave, Function, 2x CRC) simulieren -- Inhalt jeder
    // Nachricht k ist ausschliesslich das Byte (0x40 + k), damit jede
    // Nachricht eindeutig wiedererkennbar ist.
    const int NUM_MESSAGES = 40;
    const size_t MSG_LEN = 4;
    uint8_t expected[40][4];

    for(int k = 0; k < NUM_MESSAGES; k++) {
        uint8_t msg[4];
        for(size_t b = 0; b < MSG_LEN; b++) msg[b] = (uint8_t)(0x40 + k);
        memcpy(expected[k], msg, MSG_LEN);
        writeRingBuffer(rb, msg, MSG_LEN);
    }

    printf(
        "Nach %d Nachrichten a %zu Byte: writeIdx=%u (kein Wrap, < 255), delimiterIdx=%u "
        "(> 32, sollte laut Kommentar/Array-Groesse nie ueber 31 hinausgehen)\n",
        NUM_MESSAGES,
        MSG_LEN,
        rb->writeIdx,
        rb->delimiterIdx);
    assert(rb->writeIdx < 255); // bestaetigt: kein writeIdx-Wrap ist passiert
    assert(rb->delimiterIdx > 32); // bestaetigt: delimiterIdx ist trotzdem > 31 gelaufen

    // Nachricht 0 (erste Nachricht, belegt ringBuffer[0..3]) unveraendert lassen
    // und NACH allen 40 Captures erneut auslesen -- genau der Moment, in dem ein
    // Nutzer "Buffer Sender" oeffnet, nachdem er laenger gesniffed hat.
    uint8_t reconstructed0[4] = {0};
    size_t len0 = reconstruct_message(rb, 0, reconstructed0);

    int corrupted = (len0 != MSG_LEN) || (memcmp(reconstructed0, expected[0], MSG_LEN) != 0);

    printf("Nachricht #0 erwartet: ");
    for(size_t i = 0; i < MSG_LEN; i++) printf("%02X ", expected[0][i]);
    printf("(Laenge %zu)\n", MSG_LEN);

    printf("Nachricht #0 rekonstruiert (ueber BuildCMDList/OnItemEnterCB-Logik): ");
    for(size_t i = 0; i < len0 && i < 4; i++) printf("%02X ", reconstructed0[i]);
    printf("(Laenge %zu)\n", len0);

    if(corrupted) {
        printf(
            "BESTAETIGT: delimiters[32..]-Ueberlauf aus writeRingBuffer() hat ringBuffer[0] "
            "(Payload von Nachricht #0) mit einem writeIdx-Bookkeeping-Byte ueberschrieben -- "
            "die App wuerde beim erneuten Anzeigen/Senden dieser Nachricht falschen Inhalt "
            "liefern.\n");
    } else {
        printf("Kein Unterschied in diesem Lauf -- Parameter anpassen.\n");
    }

    // Ring-Buffer-Ansicht auf den betroffenen Speicherbereich zur Verifikation,
    // welches Byte den falschen Wert traegt und woher es stammt (delimiters[32]
    // wird beim delimiterIdx==32-Aufruf mit dem AKTUELLEN writeIdx beschrieben --
    // das ist exakt der Offset von ringBuffer[0] im Struct).
    printf(
        "\nZur Kontrolle: ringBuffer[0] jetzt = 0x%02X (erwartet fuer Nachricht #0: 0x%02X)\n",
        rb->ringBuffer[0],
        expected[0][0]);
    printf(
        "delimiters[32] wurde beim 33. writeRingBuffer()-Aufruf (delimiterIdx==32, aber "
        "delimiters[] hat nur Indizes 0..31) geschrieben -- im Struct-Layout ist das exakt "
        "das erste Byte von ringBuffer[], da delimiters[32] direkt vor ringBuffer[Ring_Buf_Size] "
        "liegt.\n");

    ring_buffer_free(rb);

    printf(corrupted ? "\nALLE PRUEFUNGEN BESTANDEN (Fund reproduziert).\n" : "\nFUND NICHT REPRODUZIERT.\n");
    return corrupted ? 0 : 1;
}
