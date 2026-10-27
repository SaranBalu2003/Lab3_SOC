#include "dot_math.h"
#include "golden_cases.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

static void pack(const fixtures::RawBlock *raw, unsigned char *out) {
    std::memset(out, 0, DOT_BUFFER_BYTES);
    for (unsigned b = 0; b < DOT_MAX_BLOCKS; ++b) {
        unsigned char *p = out + b * DOT_SLOT_BYTES;
        p[0] = raw[b].d & 255; p[1] = raw[b].d >> 8;
        std::memcpy(p + 2, raw[b].qs, DOT_BLOCK_ELEMENTS);
    }
}
int main() {
    std::array<unsigned char, DOT_BUFFER_BYTES> x = {}, y = {};
    fixtures::RawBlock rx[DOT_MAX_BLOCKS], ry[DOT_MAX_BLOCKS];
    const unsigned count = sizeof golden_cases / sizeof golden_cases[0];
    for (unsigned i = 0; i < count; ++i) {
        const fixtures::Case &c = golden_cases[i];
        fixtures::fill(c, rx, ry); pack(rx, x.data()); pack(ry, y.data());
        uint32_t result;
        if (dot_math::compute(x.data(), y.data(), c.blocks * 32, result) != dot_math::OK || result != c.expected_bits) {
            std::printf("FAIL %s: result=0x%08x expected=0x%08x\n", c.name, result, c.expected_bits); return 1;
        }
    }
    unsigned scale_checks = 0;
    x.fill(0); y.fill(0); y[1] = 0x3c;
    for (unsigned j = 0; j < 32; ++j) x[2+j] = y[2+j] = 1;
    for (unsigned h = 0; h < 65536; ++h) {
        const unsigned e = (h >> 10) & 31, f = h & 1023;
        if (e == 31) continue;
        x[0] = h & 255; x[1] = h >> 8;
        const float magnitude = e ? std::ldexp(float(1024+f), int(e)-20) : std::ldexp(float(f), -19);
        const float expected = h & 0x8000 ? -magnitude : magnitude;
        uint32_t expected_bits, result;
        std::memcpy(&expected_bits, &expected, 4);
        if (magnitude == 0) expected_bits = 0;
        if (dot_math::compute(x.data(), y.data(), 32, result) != dot_math::OK || result != expected_bits) {
            std::printf("FAIL half scale 0x%04x\n", h); return 1;
        }
        ++scale_checks;
    }
    uint32_t result;
    if (dot_math::compute(x.data(), y.data(), 4097, result) != dot_math::BAD_LENGTH) return 1;
    if (dot_math::compute(x.data(), y.data(), 1, result) != dot_math::BAD_LENGTH) return 1;
    x[0] = 0; x[1] = 0x7c;
    if (dot_math::compute(x.data(), y.data(), 32, result) != dot_math::NONFINITE_SCALE) return 1;
    std::printf("MATH PASS: %u golden vectors (0 ULP), %u finite-half scale cases, and invalid-input checks.\n", count, scale_checks);
    return 0;
}
