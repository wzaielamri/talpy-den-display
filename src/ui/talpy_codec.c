#include "talpy_codec.h"
#include <string.h>

// Pure C on purpose: the same file is built for the board and by host gcc for the tests.

#define OP_TRANSPARENT_MAX 0x7F // 0x00..0x7F: op + 1 transparent pixels
#define OP_REPEAT_BASE     0x80 // 0x80..0xBF: op - 0x80 + 1 copies of one pixel
#define OP_LITERAL_BASE    0xC0 // 0xC0..0xFF: op - 0xC0 + 1 literal pixels
#define PIXEL_BYTES        3    // rgb565 low, rgb565 high, alpha

bool talpy_decode(const uint8_t *src, size_t src_len, uint16_t w, uint16_t h, uint8_t *dst) {
    const size_t total = (size_t)w * h;
    uint8_t *rgb = dst;
    uint8_t *alpha = dst + total * 2;
    size_t pos = 0; // read position in src
    size_t px = 0;  // pixels written so far

    while (pos < src_len) {
        const uint8_t op = src[pos++];
        size_t count;

        if (op <= OP_TRANSPARENT_MAX) {
            count = (size_t)op + 1;
            if (count > total - px)
                return false;
            memset(rgb + px * 2, 0, count * 2);
            memset(alpha + px, 0, count);
        } else if (op < OP_LITERAL_BASE) {
            count = (size_t)(op - OP_REPEAT_BASE) + 1;
            if (count > total - px || src_len - pos < PIXEL_BYTES)
                return false;
            for (size_t i = 0; i < count; i++) {
                rgb[(px + i) * 2] = src[pos];
                rgb[(px + i) * 2 + 1] = src[pos + 1];
                alpha[px + i] = src[pos + 2];
            }
            pos += PIXEL_BYTES;
        } else {
            count = (size_t)(op - OP_LITERAL_BASE) + 1;
            if (count > total - px || src_len - pos < count * PIXEL_BYTES)
                return false;
            for (size_t i = 0; i < count; i++) {
                rgb[(px + i) * 2] = src[pos];
                rgb[(px + i) * 2 + 1] = src[pos + 1];
                alpha[px + i] = src[pos + 2];
                pos += PIXEL_BYTES;
            }
        }
        px += count;
    }
    return px == total;
}

uint32_t talpy_fnv1a(const uint8_t *data, size_t len) {
    uint32_t hash = 0x811C9DC5u;
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 0x01000193u;
    }
    return hash;
}
