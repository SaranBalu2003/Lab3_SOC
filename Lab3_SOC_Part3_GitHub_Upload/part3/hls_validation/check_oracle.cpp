// Native reference/fixture check ONLY. This does not compile or simulate the HLS kernel.
#include "golden_cases.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdint.h>
static float round32(float v) { volatile float x = v; return x; }
static uint32_t bits(float v) { uint32_t r; std::memcpy(&r, &v, 4); return r; }
static int mantissa(uint16_t h) { const int e = (h >> 10) & 31; return (h & 1023) + (e ? 1024 : 0); }
static int shift(uint16_t h) { const int e = (h >> 10) & 31; return e ? e - 1 : 0; }
static float scale_float(uint16_t h) {
    float v = std::ldexp(float(mantissa(h)), shift(h) - 24);
    return h & 0x8000u ? -v : v;
}
int main() {
    const unsigned count = sizeof golden_cases / sizeof golden_cases[0];
    unsigned differences = 0;
    for (unsigned i = 0; i < count; ++i) {
        const fixtures::Case &c = golden_cases[i];
        fixtures::RawBlock x[fixtures::CAPACITY], y[fixtures::CAPACITY];
        fixtures::fill(c, x, y);
        __int128 exact = 0;
        float scalar = 0;
        for (unsigned b = 0; b < c.blocks; ++b) {
            int isum = 0;
            for (unsigned j = 0; j < fixtures::QK; ++j) isum += int(x[b].qs[j]) * int(y[b].qs[j]);
            __int128 term = __int128(isum) * mantissa(x[b].d) * mantissa(y[b].d);
            // Multiplication avoids undefined left-shift of a negative signed integer.
            term *= (__int128(1) << (shift(x[b].d) + shift(y[b].d)));
            if ((x[b].d ^ y[b].d) & 0x8000u) term = -term;
            exact += term;
            const float product = round32(scale_float(x[b].d) * scale_float(y[b].d));
            scalar = round32(scalar + round32(round32(float(isum)) * product));
        }
        const float expected = float(std::ldexp(static_cast<long double>(exact), -48));
        if (bits(expected) != c.expected_bits || bits(scalar) != c.fp32_reference_bits) {
            std::printf("FAIL %s expected=%08x oracle=%08x scalar=%08x scalar_oracle=%08x\n",
                        c.name, bits(expected), c.expected_bits, bits(scalar), c.fp32_reference_bits);
            return 1;
        }
        if (c.expected_bits != c.fp32_reference_bits) ++differences;
    }
    std::printf("PASS: %u C++ fixtures match the Python exact oracle and sequential FP32 reference.\n", count);
    std::printf("Exact versus sequential-FP32 differences: %u/%u.\n", differences, count);
    std::puts("This is reference validation only; Vitis C simulation and RTL co-simulation remain required.");
    return 0;
}
