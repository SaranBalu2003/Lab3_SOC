#include "vec_dot_q8.h"
#include <ap_fixed.h>
#include <utils/x_hls_utils.h>

// #include <string.h>

#define UNUSED GGML_UNUSED

// /*taken from ggml-impl.h line 378 to 394 */

// /* ggml-impl.h, derived from Maratyszcza/FP16. */
// static inline float fp32_from_bits(uint32_t w) {
//     union {
//         uint32_t as_bits;
//         float as_value;
//     } fp32;
//     fp32.as_bits = w;
//     return fp32.as_value;
// }

// static inline uint32_t fp32_to_bits(float f) {
//     union {
//         float as_value;
//         uint32_t as_bits;
//     } fp32;
//     fp32.as_value = f;
//     return fp32.as_bits;
// }

// /* taken from ggml-impl.h too. line 396 to 417 */
// static inline float ggml_compute_fp16_to_fp32(ggml_fp16_t h) {
//     const uint32_t w = (uint32_t) h << 16;
//     const uint32_t sign = w & UINT32_C(0x80000000);
//     const uint32_t two_w = w + w;

//     const uint32_t exp_offset = UINT32_C(0xE0) << 23;
// #if (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) || defined(__GNUC__) && !defined(__STRICT_ANSI__)) && (!defined(__cplusplus) || __cplusplus >= 201703L)
//     const float exp_scale = 0x1.0p-112f;
// #else
//     const float exp_scale = fp32_from_bits(UINT32_C(0x7800000));
// #endif
//     const float normalized_value = fp32_from_bits((two_w >> 4) + exp_offset) * exp_scale;

//     const uint32_t magic_mask = UINT32_C(126) << 23;
//     const float magic_bias = 0.5f;
//     const float denormalized_value = fp32_from_bits((two_w >> 17) | magic_mask) - magic_bias;

//     const uint32_t denormalized_cutoff = UINT32_C(1) << 27;
//     const uint32_t result = sign |
//         (two_w < denormalized_cutoff ? fp32_to_bits(denormalized_value) : fp32_to_bits(normalized_value));
//     return fp32_from_bits(result);
// }

// #define GGML_COMPUTE_FP16_TO_FP32(x) ggml_compute_fp16_to_fp32(x)

// float ggml_table_f32_f16[1 << 16];

// /* simd-mapping.h from line 146 to 151*/
// #if !defined(GGML_CPU_FP16_TO_FP32)
// inline static float ggml_lookup_fp16_to_fp32(ggml_fp16_t f) {
//     uint16_t s;
//     memcpy(&s, &f, sizeof(uint16_t));
//     return ggml_table_f32_f16[s];
// }

// #define GGML_CPU_FP16_TO_FP32(x) ggml_lookup_fp16_to_fp32(x)
// #endif

// /* Standalone wrapper for the FP16 table initialization from ggml_cpu_init. */
// void vec_dot_q8_init(void) {
//     for (int i = 0; i < (1 << 16); ++i) {
//         union {
//             uint16_t u16;
//             ggml_fp16_t fp16;
//         } u = {static_cast<uint16_t>(i)};
//         float f = GGML_COMPUTE_FP16_TO_FP32(u.fp16);
//         ggml_table_f32_f16[i] = f;
//     }
// }

// Finite scales equal sign * significand * 2^(exponent_shift - 24).
struct half_scale_parts {
    ap_uint<11> significand;
    ap_uint<5> exponent_shift;
    bool is_negative;
};

// Keep the significand separate so the exponent shift can follow multiplication.
static half_scale_parts decode_scale(ggml_half half_scale) {
    fp_struct<half> half_encoding(half_scale);
    // Extract the half's raw 16-bit pattern.
    const ap_uint<16> half_bits_16 = half_encoding.data();
    // Copy the bit pattern into a standard C++ unsigned integer.
    const unsigned int half_bits_uint = half_bits_16.to_uint();

    // Shift the 5 exponent bits to the lowest positions, then mask them.
    const unsigned int exponent_bits = (half_bits_uint >> 10) & 0x1fu;
    // Keep only the 10 fraction bits.
    const unsigned int fraction_bits = half_bits_uint & 0x03ffu;
    const bool is_negative = (half_bits_uint & 0x8000u) != 0;
    assert(exponent_bits != 0x1fu); // Exact arithmetic requires finite scales.

    // Zero and subnormal scales have no implicit leading 1 and need no shift.
    half_scale_parts scale;
    scale.significand = fraction_bits;
    scale.exponent_shift = 0;
    scale.is_negative = is_negative;
    if (exponent_bits != 0) {
        scale.significand = 1024 + fraction_bits;
        scale.exponent_shift = exponent_bits - 1;
    }
    return scale;
}

void ggml_vec_dot_q8_0_q8_0(int n, float * GGML_RESTRICT s, size_t bs, const block_q8_0 * GGML_RESTRICT vx, size_t bx, const block_q8_0 * GGML_RESTRICT vy, size_t by, int nrc) {
    // Start/done control for each dot-product operation.
    // default for vivado ip flow we chose
    #pragma HLS INTERFACE mode=ap_ctrl_hs port=return

    // Scalar inputs and the single floating-point output.
    // ap_none is for a plain data port for scalar value.
    #pragma HLS INTERFACE mode=ap_none port=n
    #pragma HLS INTERFACE mode=ap_none port=s depth=1
    #pragma HLS INTERFACE mode=ap_none port=bs
    #pragma HLS INTERFACE mode=ap_none port=bx
    #pragma HLS INTERFACE mode=ap_none port=by
    #pragma HLS INTERFACE mode=ap_none port=nrc

    // Each input holds eight block_q8_0 elements, matching the testbench.
    // bram is for memory interface with signals like address, data and enable
    #pragma HLS INTERFACE mode=bram port=vx depth=8
    #pragma HLS INTERFACE mode=bram port=vy depth=8

    const int qk = QK8_0;
    const int nb = n / qk;

    assert(n >= 0);
    assert(n % qk == 0);
    assert(nrc == 1);
    UNUSED(nrc);
    UNUSED(bx);
    UNUSED(by);
    UNUSED(bs);

    const block_q8_0 * GGML_RESTRICT x = vx;
    const block_q8_0 * GGML_RESTRICT y = vy;


    // Use exact fixed-point products and accumulation.
    // ap_fixed<W,I> where W is total number of bits and I is integer portion, W-I is fractional
    // exact_block_result is one block's complete contribution.
    // Based on sumi += x[ib].qs[j] * y[ib].qs[j]
    // the two qs or integer needs 21 bits since largest input ranges between -128 to 127 and 
    // -128 x -128 = 16384 and 32 x 16384 = 2^19. The x and y has range of +- 65504. or 17 bits
    // required bits = 21 + 17x2 = 55 for integer
    // We need 48 fractional bits since each scale has 24 fractional bits. 55+ 48 = 103. 
    typedef ap_fixed<103, 55> exact_block_result;

    // For one block the dot product, we have 103 and 55. The 48 fractional bit remains the same
    // only the integer part may need additional bit based on the number of n and input blocks
    // we have nb = n /32. then nb < 2^31/ 32 or < 2^26. 2^26 -1 is the max # of blocks 
    // 55 + 26 = 81, 81 + 48 = 129
    typedef ap_fixed<129, 81> exact_accumulator;
    exact_accumulator total = 0;

    for (int ib = 0; ib < nb; ++ib) {
        #pragma HLS PIPELINE II=1
        int sumi = 0;
        for (int j = 0; j < qk; j++) {
            #pragma HLS UNROLL
            int product;
            #pragma HLS BIND_OP variable=product op=mul impl=fabric
            product = x[ib].qs[j] * y[ib].qs[j];
            sumi += product;
        }

        //sumf += sumi * (static_cast<float>(x[ib].d) * static_cast<float>(y[ib].d));
        // Multiply the small significands before applying the exponent shift.
        const half_scale_parts x_scale = decode_scale(x[ib].d);
        const half_scale_parts y_scale = decode_scale(y[ib].d);
        const ap_uint<22> significand_product = x_scale.significand * y_scale.significand;
        #pragma HLS BIND_OP variable=significand_product op=mul impl=dsp latency=0
        // Add a zero sign bit before multiplying by the signed integer sum.
        const ap_int<23> signed_scale_product = significand_product;
        // as explained earlier, the sumi portion uses 21 integer bits
        const ap_int<21> block_integer_sum = sumi;
        ap_int<44> scaled_sum = block_integer_sum * signed_scale_product;
        if (x_scale.is_negative != y_scale.is_negative) {
            scaled_sum = -scaled_sum;
        }

        // Each shift is at most 29, so their sum needs six bits.
        const ap_uint<6> combined_shift = ap_uint<6>(x_scale.exponent_shift) + ap_uint<6>(y_scale.exponent_shift);
        // Widen before shifting to preserve every product bit.
        ap_int<103> block_bits = scaled_sum;
        block_bits <<= combined_shift;
        // Each raw bit pattern represents an exact multiple of 2^-48.
        exact_block_result block_result;
        block_result.range(102, 0) = block_bits.range(102, 0);
        total += block_result;
    }

    // Convert to float
    //We need it because our internal total is fixed-point, but the function’s output must be float
    // previously we had local *s = sumf;
    *s = total.to_float();
}
