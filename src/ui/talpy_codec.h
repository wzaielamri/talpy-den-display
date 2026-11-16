#ifndef TALPY_CODEC_H
#define TALPY_CODEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Decodes one compressed Talpy frame into dst, which holds w*h*3 bytes: the RGB565 plane (little-endian), then the
// alpha plane. True only if the stream is exactly w*h pixels and ends exactly at src_len; never writes past w*h*3.
bool talpy_decode(const uint8_t *src, size_t src_len, uint16_t w, uint16_t h, uint8_t *dst);

// FNV-1a 32-bit checksum, used to check a decoded frame against the one the PC tool made.
uint32_t talpy_fnv1a(const uint8_t *data, size_t len);

#endif
