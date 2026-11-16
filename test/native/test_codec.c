#include "check.h"
#include "ui/talpy_codec.h"
#include <string.h>

static void test_transparent_run(void) {
    const uint8_t src[] = {0x03};
    uint8_t dst[12];
    memset(dst, 0xAA, sizeof dst);
    CHECK(talpy_decode(src, sizeof src, 4, 1, dst));
    for (int i = 0; i < 12; i++)
        CHECK(dst[i] == 0);
}

static void test_repeat_run(void) {
    const uint8_t src[] = {0x81, 0x34, 0x12, 0xFF};
    const uint8_t want[] = {0x34, 0x12, 0x34, 0x12, 0xFF, 0xFF};
    uint8_t dst[6];
    CHECK(talpy_decode(src, sizeof src, 2, 1, dst));
    CHECK(memcmp(dst, want, sizeof want) == 0);
}

static void test_literal_run(void) {
    const uint8_t src[] = {0xC1, 0x01, 0x00, 0x80, 0x02, 0x00, 0x40};
    const uint8_t want[] = {0x01, 0x00, 0x02, 0x00, 0x80, 0x40};
    uint8_t dst[6];
    CHECK(talpy_decode(src, sizeof src, 2, 1, dst));
    CHECK(memcmp(dst, want, sizeof want) == 0);
}

static void test_mixed_ops(void) {
    // transparent 1, repeat 2 of (0x1234, 0xFF), literal 1 of (0x0506, 0x10)
    const uint8_t src[] = {0x00, 0x81, 0x34, 0x12, 0xFF, 0xC0, 0x06, 0x05, 0x10};
    const uint8_t want[] = {0, 0, 0x34, 0x12, 0x34, 0x12, 0x06, 0x05, 0x00, 0xFF, 0xFF, 0x10};
    uint8_t dst[12];
    CHECK(talpy_decode(src, sizeof src, 4, 1, dst));
    CHECK(memcmp(dst, want, sizeof want) == 0);
}

static void test_truncated_stream_fails(void) {
    const uint8_t src[] = {0xC1, 0x01, 0x00, 0x80};
    uint8_t dst[6];
    CHECK(!talpy_decode(src, sizeof src, 2, 1, dst));
}

static void test_overlong_run_fails(void) {
    const uint8_t src[] = {0x7F};
    uint8_t dst[7];
    memset(dst, 0x5A, sizeof dst);
    CHECK(!talpy_decode(src, sizeof src, 2, 1, dst));
    CHECK(dst[6] == 0x5A);
}

static void test_trailing_bytes_fail(void) {
    const uint8_t src[] = {0x01, 0x00};
    uint8_t dst[6];
    CHECK(!talpy_decode(src, sizeof src, 2, 1, dst));
}

static void test_short_stream_fails(void) {
    const uint8_t src[] = {0x00};
    uint8_t dst[6];
    CHECK(!talpy_decode(src, sizeof src, 2, 1, dst));
}

static void test_fnv1a(void) {
    CHECK(talpy_fnv1a((const uint8_t *)"", 0) == 0x811C9DC5u);
    CHECK(talpy_fnv1a((const uint8_t *)"a", 1) == 0xE40C292Cu);
}

int main(void) {
    test_transparent_run();
    test_repeat_run();
    test_literal_run();
    test_mixed_ops();
    test_truncated_stream_fails();
    test_overlong_run_fails();
    test_trailing_bytes_fail();
    test_short_stream_fails();
    test_fnv1a();
    return check_failures() ? 1 : 0;
}
