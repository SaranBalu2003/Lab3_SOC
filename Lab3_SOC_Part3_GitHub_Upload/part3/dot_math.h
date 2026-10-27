#ifndef LAB3_DOT_MATH_H
#define LAB3_DOT_MATH_H
#include "dot_regs.h"
namespace dot_math {
enum Error { OK = 0, BAD_LENGTH = 1, NONFINITE_SCALE = 2 };
Error compute(const unsigned char *x, const unsigned char *y,
              uint32_t elements, uint32_t &result_bits);
// Inferred conservative length-dependent estimate: 128 + 13 = 141 cycles,
// anchored to HLS maximum and block II=1. This is not per-input RTL timing.
inline uint32_t cycles(uint32_t elements) { return elements / DOT_BLOCK_ELEMENTS + 13u; }
}
#endif
