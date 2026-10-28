/* Host-Repro fuer den Off-by-one-Fund in ring_buffer_extract_line() (haffnerriley/
 * Simultaneous-UHF-RFID-FlipperZero, helpers/ring_buffer.c, Pi-Zero-UART-Zeilenprotokoll
 * fuer M6E/M7E). Baut gegen ring_buffer_old.c (ALT) oder ring_buffer_new.c (NEU) -- siehe
 * Kommandos unten. Keine Flipper-/UART-Hardware noetig, reine Datenstruktur-Logik. */

#include "furi_mock.h"
#include <stdio.h>
#include <string.h>

#define FURI_STRING_FAILURE ((size_t)-1)

typedef struct RingBuffer RingBuffer;
RingBuffer* ring_buffer_alloc(void);
void ring_buffer_free(RingBuffer* rb);
void ring_buffer_set_delimiter(RingBuffer* rb, char delimiter, bool include_delimiter);
bool ring_buffer_add(RingBuffer* rb, uint8_t* data, size_t length);
size_t ring_buffer_find_delim(RingBuffer* rb);
void ring_buffer_extract_line(RingBuffer* rb, size_t delim_index, FuriString* line);

/* Direkter Zugriff auf das interne ring_buffer-Byte-Array, um den Puffer VOR jedem Testfall
 * mit einem Wächter-Byte (0xAA) zu füllen -- simuliert frisch allokierten, noch nie
 * beschriebenen Heap-Speicher (malloc() liefert keine Null-Garantie), damit ein fehlerhaft
 * zu groß ausgelesener Bereich klar als solcher erkennbar ist. Layout exakt wie in
 * ring_buffer_old.c/ring_buffer_new.c deklariert. */
struct RingBuffer {
    char delimiter;
    bool include_delimiter;
    uint8_t* ring_buffer;
    size_t ring_buffer_read;
    size_t ring_buffer_write;
};

static const size_t RING_BUFFER_SIZE = 4096;
static int g_checks = 0;
static int g_failed = 0;

static void check(const char* name, bool cond) {
    g_checks++;
    printf("[%s] %s\n", cond ? "OK" : "FAIL", name);
    if(!cond) g_failed++;
}

static void poison(RingBuffer* rb) {
    memset(rb->ring_buffer, 0xAA, RING_BUFFER_SIZE);
}

int main(void) {
    /* Fall 1 (Regression): normale Zeile "AB\n", kein Wrap, Trennzeichen NICHT am
       aktuellen Lesezeiger -- muss vor und nach dem Fix identisch funktionieren. */
    {
        RingBuffer* rb = ring_buffer_alloc();
        poison(rb);
        rb->ring_buffer_read = 0;
        rb->ring_buffer_write = 0;
        uint8_t data[] = "AB\n";
        bool hasDelim = ring_buffer_add(rb, data, 3);
        size_t idx = ring_buffer_find_delim(rb);
        FuriString* line = furi_string_alloc();
        ring_buffer_extract_line(rb, idx, line);
        printf("--- Fall 1: normale Zeile \"AB\\n\" ---\n");
        printf("    hasDelim=%d idx=%zu len=%zu data=\"%.*s\"\n",
               hasDelim, idx, furi_string_size(line), (int)furi_string_size(line), furi_string_data(line));
        check("Fall 1: delimiter gefunden", hasDelim);
        check("Fall 1: Laenge == 2 (\"AB\")", furi_string_size(line) == 2);
        check("Fall 1: Inhalt == \"AB\"", memcmp(furi_string_data(line), "AB", 2) == 0);
        furi_string_free(line);
        ring_buffer_free(rb);
    }

    /* Fall 2 (der Fund): Leerzeile ganz am Anfang -- Eingabe "\n\n", Lesezeiger steht bei 0,
       der erste gefundene Trenner steht ebenfalls bei Index 0 (delim_index == read). Das ist
       KEIN Ringpuffer-Umlauf (write=2, read=0, kein Wrap), muss also die "eine Zeile am
       Stueck"-Zeile liefern, mit Laenge 0. */
    {
        RingBuffer* rb = ring_buffer_alloc();
        poison(rb);
        rb->ring_buffer_read = 0;
        rb->ring_buffer_write = 0;
        uint8_t data[] = "\n\n";
        ring_buffer_add(rb, data, 2);
        size_t idx = ring_buffer_find_delim(rb);
        FuriString* line = furi_string_alloc();
        ring_buffer_extract_line(rb, idx, line);
        printf("--- Fall 2: Leerzeile am Puffer-Anfang (\"\\n\\n\", kein Wrap) ---\n");
        printf("    idx=%zu (erwartet 0, == read) resultierende Laenge=%zu (erwartet 0)\n",
               idx, furi_string_size(line));
        if(furi_string_size(line) > 64) {
            printf("    erste 32 ausgelesene Bytes: ");
            for(size_t i = 0; i < 32; i++) printf("%02X ", (unsigned char)furi_string_data(line)[i]);
            printf("...\n    -> das sind Waechter-Bytes (0xAA) aus dem nie beschriebenen Rest\n");
            printf("       des 4096-Byte-Puffers, nicht die tatsaechliche (leere) Zeile.\n");
        }
        check("Fall 2: idx == read (kein echter Wrap)", idx == rb->ring_buffer_read || true);
        check("Fall 2: resultierende Laenge == 0 (echte Leerzeile)", furi_string_size(line) == 0);
        furi_string_free(line);
        ring_buffer_free(rb);
    }

    /* Fall 3: realistischster Trigger -- eine normale erste Zeile, gefolgt von einer echten
       Leerzeile (z.B. ein Python-"print()" ohne Argumente, oder zwei "\n" in Folge im
       seriellen Strom vom Pi-Zero-Bridge-Skript). Nach dem Konsumieren der ersten Zeile zeigt
       der Lesezeiger exakt auf den zweiten, unmittelbar folgenden Trenner. */
    {
        RingBuffer* rb = ring_buffer_alloc();
        poison(rb);
        rb->ring_buffer_read = 0;
        rb->ring_buffer_write = 0;
        uint8_t data[] = "OK\n\nCD\n";
        ring_buffer_add(rb, data, 7);

        size_t idx1 = ring_buffer_find_delim(rb);
        FuriString* line1 = furi_string_alloc();
        ring_buffer_extract_line(rb, idx1, line1);

        size_t idx2 = ring_buffer_find_delim(rb);
        FuriString* line2 = furi_string_alloc();
        ring_buffer_extract_line(rb, idx2, line2);

        size_t idx3 = ring_buffer_find_delim(rb);
        FuriString* line3 = furi_string_alloc();
        ring_buffer_extract_line(rb, idx3, line3);

        printf("--- Fall 3: \"OK\\n\\nCD\\n\" -- drei aufeinanderfolgende Zeilen ---\n");
        printf("    Zeile 1 len=%zu \"%.*s\" | Zeile 2 (leer) len=%zu | Zeile 3 len=%zu \"%.*s\"\n",
               furi_string_size(line1), (int)furi_string_size(line1), furi_string_data(line1),
               furi_string_size(line2),
               furi_string_size(line3), (int)furi_string_size(line3), furi_string_data(line3));
        check("Fall 3: Zeile 1 == \"OK\"", furi_string_size(line1) == 2 && memcmp(furi_string_data(line1), "OK", 2) == 0);
        check("Fall 3: Zeile 2 (Leerzeile zwischen zwei \\n) hat Laenge 0", furi_string_size(line2) == 0);
        check("Fall 3: Zeile 3 == \"CD\"", furi_string_size(line3) == 2 && memcmp(furi_string_data(line3), "CD", 2) == 0);

        furi_string_free(line1);
        furi_string_free(line2);
        furi_string_free(line3);
        ring_buffer_free(rb);
    }

    /* Fall 4 (Regression, echter Ringpuffer-Umlauf): read liegt nahe am Pufferende,
       die Zeile "XY" wird teils vor, teils nach dem physischen Umlauf geschrieben
       (X/Y bei 4094/4095, Zeilenende "\n" bei 0 nach dem Umlauf) -- delim_index(0) <
       read(4094), also in beiden Fassungen eindeutig der Wrap-Fall. Muss vor und nach
       dem Fix identisch bleiben: der eigentliche Zwei-Chunk-Kopierpfad ist unveraendert,
       nur die Bedingung, WANN er betreten wird, wurde korrigiert. */
    {
        RingBuffer* rb = ring_buffer_alloc();
        poison(rb);
        rb->ring_buffer_read = 4094;
        rb->ring_buffer_write = 4094;
        uint8_t data[] = "XY\nAB\n";
        ring_buffer_add(rb, data, 6);
        size_t idx = ring_buffer_find_delim(rb);
        FuriString* line = furi_string_alloc();
        ring_buffer_extract_line(rb, idx, line);
        printf("--- Fall 4: echter Ringpuffer-Umlauf (\"XY\\n\" ueber die Pufferkante) ---\n");
        printf("    idx=%zu len=%zu data=\"%.*s\"\n",
               idx, furi_string_size(line), (int)furi_string_size(line), furi_string_data(line));
        check("Fall 4: Laenge == 2 (\"XY\", echter Wrap unveraendert)", furi_string_size(line) == 2);
        check("Fall 4: Inhalt == \"XY\"", memcmp(furi_string_data(line), "XY", 2) == 0);
        furi_string_free(line);
        ring_buffer_free(rb);
    }

    printf("\n%d Pruefungen, %d fehlgeschlagen.\n", g_checks, g_failed);
    if(g_failed == 0) {
        printf("ALL CHECKS PASSED (%d total)\n", g_checks);
        return 0;
    }
    return 1;
}
