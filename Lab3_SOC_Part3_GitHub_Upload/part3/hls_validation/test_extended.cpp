#include "vec_dot_q8.h"
#include "golden_cases.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdint.h>

// Preserve and execute the teammate's original 27 tests in the same run.
#define main lab2_original_tests_main
#include "lab2_original_tests.cpp"
#undef main

static uint32_t float_bits(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    return bits;
}
static uint32_t ordered_bits(uint32_t bits) {
    return (bits & 0x80000000u) ? ~bits : (bits | 0x80000000u);
}
static uint64_t ulp_distance(uint32_t a, uint32_t b) {
    if ((a & 0x7fffffffu) == 0 && (b & 0x7fffffffu) == 0) return 0;
    const uint32_t aa = ordered_bits(a), bb = ordered_bits(b);
    return aa > bb ? uint64_t(aa) - bb : uint64_t(bb) - aa;
}
int main() {
    if (lab2_original_tests_main() != 0) return 1;
    static_assert(DOT_MAX_BLOCKS == fixtures::CAPACITY, "Capacity mismatch");
    block_q8_0 x[DOT_MAX_BLOCKS] = {}, y[DOT_MAX_BLOCKS] = {};
    fixtures::RawBlock raw_x[DOT_MAX_BLOCKS], raw_y[DOT_MAX_BLOCKS];
    unsigned failures = 0, fp32_differences = 0;
    uint64_t max_ulp = 0;
    const unsigned count = sizeof golden_cases / sizeof golden_cases[0];
    FILE *csv = std::fopen("extended_results.csv", "w");
    if (!csv) { std::perror("extended_results.csv"); return 1; }
    std::fprintf(csv, "name,blocks,actual_bits,exact_fp32_bits,scalar_fp32_bits,ulp_error,status\n");
    for (unsigned i = 0; i < count; ++i) {
        const fixtures::Case &c = golden_cases[i];
        fixtures::fill(c, raw_x, raw_y);
        for (unsigned b = 0; b < DOT_MAX_BLOCKS; ++b) {
            std::memcpy(&x[b].d, &raw_x[b].d, sizeof(uint16_t));
            std::memcpy(&y[b].d, &raw_y[b].d, sizeof(uint16_t));
            std::memcpy(x[b].qs, raw_x[b].qs, fixtures::QK);
            std::memcpy(y[b].qs, raw_y[b].qs, fixtures::QK);
        }
        float actual = -999.0f;
        ggml_vec_dot_q8_0_q8_0(c.blocks * fixtures::QK, &actual, 0, x, 0, y, 0, 1);
        const uint32_t actual_bits = float_bits(actual);
        const uint64_t ulps = ulp_distance(actual_bits, c.expected_bits);
        // One final binary32 conversion may differ by one ULP from nearest-even.
        // Require an exactly zero result for exact-zero cases.
        const bool pass = std::isfinite(actual) && ulps <= 1 &&
                          (c.expected_bits != 0 || actual == 0.0f);
        if (!pass) ++failures;
        if (ulps > max_ulp) max_ulp = ulps;
        if (actual_bits != c.fp32_reference_bits) ++fp32_differences;
        std::fprintf(csv, "%s,%u,0x%08x,0x%08x,0x%08x,%llu,%s\n", c.name, c.blocks,
                     actual_bits, c.expected_bits, c.fp32_reference_bits,
                     static_cast<unsigned long long>(ulps), pass ? "PASS" : "FAIL");
        if (!pass) std::printf("FAIL %s: actual=0x%08x expected=0x%08x ULP=%llu\n",
                              c.name, actual_bits, c.expected_bits,
                              static_cast<unsigned long long>(ulps));
    }
    std::fclose(csv);
    std::printf("Extended tests: %u/%u passed\n", count - failures, count);
    std::printf("Maximum ULP error against exact-sum FP32 oracle: %llu\n",
                static_cast<unsigned long long>(max_ulp));
    std::printf("Outputs differing from sequential FP32 reference: %u/%u\n", fp32_differences, count);
    std::printf("%s: original 27 tests plus %u extended tests\n",
                failures ? "VALIDATION FAILED" : "ALL VALIDATION TESTS PASSED", count);
    return failures ? 1 : 0;
}
