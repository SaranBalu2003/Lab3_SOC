#ifndef VEC_DOT_Q8_H
#define VEC_DOT_Q8_H
#include <hls_half.h>

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

typedef half ggml_half;
//comment out for hls half.h
//typedef uint16_t ggml_half; 
// from vec dot function indirectly accesses struct qk8 by .d

// typedef uint16_t ggml_fp16_t; // from simd too


// This struct is taken from ggml-common.h line 252
#define QK8_0 32

// Lab 3 local-buffer contract: at most 4096 elements per call.
#define DOT_MAX_BLOCKS 128
typedef struct {
    ggml_half d;       // delta
    int8_t  qs[QK8_0]; // quants
} block_q8_0;
static_assert(sizeof(block_q8_0) == sizeof(ggml_half) + QK8_0, "wrong q8_0 block size/padding");


// The elements unused by scalar implementation. within vec dot function
#define GGML_UNUSED(x) (void)(x)

//vec dot function has GGML-RESTRICT signature for pointers
// This macro checks whether we are running in c or cpp and gives compiler
// info about how pointers access memory 
#ifdef __cplusplus
    // restrict not standard in C++
#    if defined(__GNUC__)
#        define GGML_RESTRICT __restrict__
#    elif defined(__clang__)
#        define GGML_RESTRICT __restrict
#    elif defined(_MSC_VER)
#        define GGML_RESTRICT __restrict
#    else
#        define GGML_RESTRICT
#    endif
#else
#    if defined (_MSC_VER) && (__STDC_VERSION__ < 201112L)
#        define GGML_RESTRICT __restrict
#    else
#        define GGML_RESTRICT restrict
#    endif
#endif


//void vec_dot_q8_init(void);
void ggml_vec_dot_q8_0_q8_0(int n, float * GGML_RESTRICT s, size_t bs, const block_q8_0 * GGML_RESTRICT vx, size_t bx, const block_q8_0 * GGML_RESTRICT vy, size_t by, int nrc);

#endif
