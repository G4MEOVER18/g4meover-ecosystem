/* Byteidentisch zu helpers/ring_buffer.c aus haffnerriley/Simultaneous-UHF-RFID-FlipperZero
 * (Commit siehe ideas/simultaneous-rfid-reader-ring-buffer-empty-line-fix.md), nur das
 * Firmware-Include durch den Host-Mock ersetzt. Unveraenderte ("ALT") Fassung. */

#include "furi_mock.h"
#define FURI_STRING_FAILURE ((size_t)-1)

static const uint32_t ring_buffer_size = 4096;

typedef struct {
    char delimiter;
    bool include_delimiter;
    uint8_t* ring_buffer;
    size_t ring_buffer_read;
    size_t ring_buffer_write;
} RingBuffer;

RingBuffer* ring_buffer_alloc() {
    RingBuffer* buffer = malloc(sizeof(RingBuffer));
    buffer->ring_buffer = malloc(ring_buffer_size);
    buffer->ring_buffer_read = 0;
    buffer->ring_buffer_write = 0;
    buffer->delimiter = '\n';
    buffer->include_delimiter = false;
    return buffer;
}

void ring_buffer_free(RingBuffer* buffer) {
    free(buffer->ring_buffer);
    free(buffer);
}

void ring_buffer_set_delimiter(RingBuffer* rb, char delimiter, bool include_delimiter) {
    rb->delimiter = delimiter;
    rb->include_delimiter = include_delimiter;
}

size_t ring_buffer_available(RingBuffer* rb) {
    size_t available;
    if(rb->ring_buffer_write == rb->ring_buffer_read) {
        available = ring_buffer_size - 1;
    } else if(rb->ring_buffer_read > rb->ring_buffer_write) {
        available = rb->ring_buffer_read - rb->ring_buffer_write;
    } else {
        available = (ring_buffer_size - rb->ring_buffer_write) + rb->ring_buffer_read;
    }
    return available;
}

bool ring_buffer_add(RingBuffer* rb, uint8_t* data, size_t length) {
    bool hasDelim = false;

    for(size_t i = 0; i < length; i++) {
        rb->ring_buffer[rb->ring_buffer_write] = data[i];

        if(data[i] == (uint8_t)rb->delimiter) {
            hasDelim = true;
        }

        if(++rb->ring_buffer_write >= ring_buffer_size) {
            rb->ring_buffer_write = 0;
        }

        if(rb->ring_buffer_write == rb->ring_buffer_read) {
            if(++rb->ring_buffer_read >= ring_buffer_size) {
                rb->ring_buffer_read = 0;
            }
        }
    }

    return hasDelim;
}

size_t ring_buffer_find_delim(RingBuffer* rb) {
    size_t index = FURI_STRING_FAILURE;

    size_t i = rb->ring_buffer_read;

    while(i != rb->ring_buffer_write) {
        if(rb->ring_buffer[i] == (uint8_t)rb->delimiter) {
            index = i;
            break;
        }

        if(++i >= ring_buffer_size) {
            i = 0;
        }
    }

    return index;
}

void ring_buffer_extract_line(RingBuffer* rb, size_t delim_index, FuriString* line) {
    if(delim_index >= rb->ring_buffer_read) {
        /* line is in one chunk */
        furi_string_set_strn(
            line,
            (char*)&rb->ring_buffer[rb->ring_buffer_read],
            delim_index - rb->ring_buffer_read + (rb->include_delimiter ? 1 : 0));
    } else {
        /* line is split across the buffer wrap, so we need to copy it in two chunks */
        furi_string_set_strn(
            line, (char*)&rb->ring_buffer[rb->ring_buffer_read], ring_buffer_size - rb->ring_buffer_read);

        for(size_t i = 0; i < delim_index; i++) {
            furi_string_push_back(line, (char)rb->ring_buffer[i]);
        }

        if(rb->include_delimiter) {
            furi_string_push_back(line, (char)rb->ring_buffer[delim_index]);
        }
    }

    rb->ring_buffer_read = delim_index + 1;
    if(rb->ring_buffer_read >= ring_buffer_size) {
        rb->ring_buffer_read = 0;
    }
}

void ring_buffer_clear(RingBuffer* rb) {
    rb->ring_buffer_read = 0;
    rb->ring_buffer_write = 0;
}
