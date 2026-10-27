// Derived from the uploaded Lab 2 alternate kernel. Original notices/licenses
// are retained in upstream/ and LICENSE. No Xilinx half/ap_fixed dependency.
#include "dot_math.h"

namespace {
typedef __int128 SignedWide;
typedef unsigned __int128 UnsignedWide;
static_assert(sizeof(SignedWide) == 16, "GCC/Clang 128-bit integer support required");
uint16_t scale_bits(const unsigned char *p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
int signed_byte(unsigned char v) { return v < 128 ? int(v) : int(v) - 256; }
unsigned significand(uint16_t h) { return (h & 1023u) + ((h & 0x7c00u) ? 1024u : 0u); }
unsigned shift(uint16_t h) { const unsigned e = (h >> 10) & 31u; return e ? e - 1u : 0u; }

uint32_t to_float_bits(SignedWide value) {
    if (value == 0) return 0;
    const uint32_t sign = value < 0 ? 0x80000000u : 0u;
    const UnsignedWide a = value < 0 ? UnsignedWide(-value) : UnsignedWide(value);
    unsigned k = 127;
    while (((a >> k) & 1u) == 0) --k;
    int exponent = int(k) - 48;
    uint32_t sig;
    if (k > 23) {
        const unsigned s = k - 23;
        sig = uint32_t(a >> s);
        const UnsignedWide remainder = a & ((UnsignedWide(1) << s) - 1);
        const UnsignedWide halfway = UnsignedWide(1) << (s - 1);
        if (remainder > halfway || (remainder == halfway && (sig & 1u))) ++sig;
        if (sig == (1u << 24)) { sig >>= 1; ++exponent; }
    } else {
        sig = uint32_t(a) << (23 - k);
    }
    // Finite half scales and <=128 blocks keep every nonzero result within
    // normal binary32 range (smallest possible magnitude is 2^-48).
    return sign | (uint32_t(exponent + 127) << 23) | (sig & 0x7fffffu);
}
}

dot_math::Error dot_math::compute(const unsigned char *x, const unsigned char *y,
                                uint32_t elements, uint32_t &result_bits) {
    result_bits = 0;
    if (elements > DOT_MAX_ELEMENTS || elements % DOT_BLOCK_ELEMENTS) return BAD_LENGTH;
    SignedWide total = 0;
    for (unsigned b = 0; b < elements / DOT_BLOCK_ELEMENTS; ++b) {
        const unsigned char *xb = x + b * DOT_SLOT_BYTES;
        const unsigned char *yb = y + b * DOT_SLOT_BYTES;
        const uint16_t dx = scale_bits(xb), dy = scale_bits(yb);
        if ((dx & 0x7c00u) == 0x7c00u || (dy & 0x7c00u) == 0x7c00u) return NONFINITE_SCALE;
        int isum = 0;
        for (unsigned j = 0; j < DOT_BLOCK_ELEMENTS; ++j)
            isum += signed_byte(xb[2 + j]) * signed_byte(yb[2 + j]);
        SignedWide term = SignedWide(isum) * significand(dx) * significand(dy);
        // Avoid left-shifting a negative native signed integer.
        term *= SignedWide(1) << (shift(dx) + shift(dy));
        if ((dx ^ dy) & 0x8000u) term = -term;
        total += term;
    }
    // The hardware uses 129 bits for arbitrary int-length vectors. With our
    // enforced 128-block bound, |raw total| < 2^106, so signed 128 bits is exact.
    result_bits = to_float_bits(total);
    return OK;
}
