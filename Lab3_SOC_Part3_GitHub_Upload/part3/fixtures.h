#ifndef LAB3_VALIDATION_FIXTURES_H
#define LAB3_VALIDATION_FIXTURES_H
#include <stdint.h>
#include <string.h>

namespace fixtures {
enum { CAPACITY = 128, QK = 32 };
struct RawBlock { uint16_t d; int8_t qs[QK]; };
static_assert(sizeof(RawBlock) == 34, "Unexpected Q8_0 layout");
struct Case {
    const char *name;
    unsigned blocks;
    unsigned mode;
    uint32_t seed;
    uint32_t expected_bits;
    uint32_t fp32_reference_bits;
};
inline uint32_t next(uint32_t &s) { s = s * 1664525u + 1013904223u; return s; }
inline uint16_t scale(uint32_t &s) {
    const uint32_t r = next(s);
    return static_cast<uint16_t>((r % 0x7c00u) | ((r >> 16) & 0x8000u));
}
inline void fill(const Case &c, RawBlock x[CAPACITY], RawBlock y[CAPACITY]) {
    memset(x, 0, sizeof(RawBlock) * CAPACITY);
    memset(y, 0, sizeof(RawBlock) * CAPACITY);
    uint32_t state = c.seed;
    for (unsigned b = 0; b < CAPACITY; ++b) {
        x[b].d = y[b].d = 0x3c00u;
        if (c.mode == 0) {
            x[b].d = scale(state); y[b].d = scale(state);
            for (unsigned j = 0; j < QK; ++j) {
                x[b].qs[j] = static_cast<int8_t>(static_cast<int>(next(state) >> 24) - 128);
                y[b].qs[j] = static_cast<int8_t>(static_cast<int>(next(state) >> 24) - 128);
            }
        } else if (c.mode == 1 || c.mode == 2 || c.mode == 5 || c.mode == 7 || c.mode == 8) {
            if (c.mode == 2 || c.mode == 7 || c.mode == 8) x[b].d = y[b].d = 0x7bffu;
            if (c.mode == 5) x[b].d = y[b].d = 0x0001u;
            for (unsigned j = 0; j < QK; ++j) {
                const int v = (c.mode == 2 || c.mode == 7 || c.mode == 8) ? -128 : 1;
                x[b].qs[j] = static_cast<int8_t>(v);
                y[b].qs[j] = static_cast<int8_t>(c.mode == 8 ? 127 : v);
            }
            if (c.mode == 7 && b < c.blocks) memset(x[b].qs, 0, QK);
        }
    }
    if (c.mode == 3 || c.mode == 4) {
        for (unsigned b = 0; b < 3; b += 2) {
            x[b].d = y[b].d = c.mode == 3 ? 0x4c00u : 0x7bffu;
            if (c.mode == 4 && b == 2) x[b].d |= 0x8000u;
            for (unsigned j = 0; j < QK; ++j) {
                x[b].qs[j] = static_cast<int8_t>(c.mode == 4 ? -128 : (b == 0 ? 64 : -64));
                y[b].qs[j] = static_cast<int8_t>(c.mode == 4 ? -128 : 64);
            }
        }
        x[1].qs[0] = y[1].qs[0] = 1;
        if (c.mode == 4) x[1].d = y[1].d = 0x0001u;
    }
    if (c.mode == 6 && c.blocks) {
        x[c.blocks - 1].qs[QK - 1] = -128;
        y[c.blocks - 1].qs[QK - 1] = 127;
    }
}
} // namespace fixtures
#endif
