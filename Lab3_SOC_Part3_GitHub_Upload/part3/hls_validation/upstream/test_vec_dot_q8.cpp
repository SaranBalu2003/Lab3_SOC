#include "vec_dot_q8.h"

#include <assert.h>
#include <cmath>
#include <cstring>
#include <random>
#include <stdio.h>


// Struct for collecting the results as manual highlights these criteria
// It sores maximum running errors and test counts.
struct TestResults {
    double max_absolute_error;
    double max_relative_error_percent;
    int count;
};


// Testbench need to calculate the scale's value itself without calling our helper for comparison.
static double reference_scale(const ggml_half &half_scale) {
    uint16_t half_bits_16;
    static_assert(sizeof(half_scale) == sizeof(half_bits_16), "half must occupy 16 bits");
    std::memcpy(&half_bits_16, &half_scale, sizeof(half_bits_16));
    // shift right by 10 and mask 5 lowest bit for exponent
    const int exponent_bits = (half_bits_16 >> 10) & 0x1f;
    // fraction bits already occupy 10 lowest bits. just mask
    const int fraction_bits = half_bits_16 & 0x03ff;
    const bool is_negative = (half_bits_16 & 0x8000u) != 0;
     // test is only for finite scales. exponent of 11111 is for special case infinity of nan in fp16
    assert(exponent_bits != 0x1f); 
   

    double double_scale;
    if (exponent_bits == 0) {
        // subnormal scale case
        double_scale = std::ldexp(static_cast<double>(fraction_bits), -24);
    } else {
        // Normal scale case
        double_scale = std::ldexp(static_cast<double>(1024 + fraction_bits), exponent_bits - 25);
    }

    if (is_negative) {
        double_scale = -double_scale;
    }

    return double_scale;
}

// This function handles 1 test case each time its called. 
// The last argument supplies either a calculated reference or a known exact answer.
static float run_test(const char *name, int n, const block_q8_0 *x, const block_q8_0 *y, TestResults &tests, double reference) {
    // A nonzero initial value also checks that zero-result calls write the output.

    float actual = -999.0f;
    ggml_vec_dot_q8_0_q8_0(n, &actual, 0, x, 0, y, 0, 1);

    // check if our actual value goes infinity neg inf or non value
    assert(std::isfinite(actual));
    const double absolute_error = std::fabs(static_cast<double>(actual) - reference);
    double relative_error_percent = 0.0;
    printf("%s: reference is %.9g and actual is %.9g. Absolute error is %.9g", name, reference,
           static_cast<double>(actual), absolute_error);

    if (reference == 0.0) {
        printf("Can't calculate due to 0 denominator case\n");
    } 
    
    else {

        relative_error_percent = 100.0 * absolute_error / std::fabs(reference);
        printf(", relative error percent is %.9g%%\n", relative_error_percent);
    }
    fflush(stdout);

    if (reference == 0.0) {
        // These zero cases use inputs whose products and cancellation are exact.
        assert(actual == 0.0f);
    } else {
        // Manual says to ensure hardware error be less than 10% explicitly. 
        // Added check here
        assert(relative_error_percent < 10.0);
    }

    // write the result into our struct
    // update max absolute error and percent if conditions are met
    if (absolute_error > tests.max_absolute_error) {
        tests.max_absolute_error = absolute_error;
    }
    if (relative_error_percent > tests.max_relative_error_percent) {
        tests.max_relative_error_percent = relative_error_percent;
    }
    ++tests.count;
    return actual;
}

// Ordinary tests calculate their reference in double precision.
static float run_test(const char *name, int n, const block_q8_0 *x, const block_q8_0 *y, TestResults &tests) {
    // Use the stored half scales, which may differ from the assigned literals.
    double reference = 0.0;

    for (int i = 0; i < n; ++i) {
        // QK8_0 is 32, so dividing i to 32 gives which block does i belong to
        // getting remainder gives the position witin it
        const int ib = i / QK8_0;
        const int j = i % QK8_0;
        const double x_value = reference_scale(x[ib].d) * x[ib].qs[j];
        const double y_value = reference_scale(y[ib].d) * y[ib].qs[j];
        reference += x_value * y_value;
    }

    return run_test(name, n, x, y, tests, reference);
}

int main(void) {

    //fill the lookup table before dot product use it
    //vec_dot_q8_init();

    // Now eight blocks per vector for tests of up to 256 elements instead of 2 blocks.
    // Each test passes n to choose how many of these elements to process.
    // initialize our block here
    const int max_blocks = 8;
    block_q8_0 x[max_blocks] = {};
    block_q8_0 y[max_blocks] = {};
    float result = 0.0f;

    // initialize our result struct
    TestResults tests = {};

    // represented value = scale × stored integer
    // d is scale here. 
    // qs is the stored integer
    // FP16 bits 0x3c00 represent a scale of 1.0.

    // we no longer need hexadecimal because d is now half from HLS
    // then we can use float -> half conversion directly

    x[0].d = 1.0f;
    y[0].d = 1.0f;
    for (int j = 0; j < QK8_0; ++j) {
        x[0].qs[j] = 1;
        y[0].qs[j] = 1;
    }

    // 32 * (1 * 1) * (1.0 * 1.0) = 32.
    result = run_test("Positive values", QK8_0, x, y, tests);
    assert(result == 32.0f);

    for (int j = 0; j < QK8_0; ++j) {
        x[0].qs[j] = 0;
    }
    // Every product is zero.
    result = run_test("Zero values", QK8_0, x, y, tests);
    assert(result == 0.0f);

    for (int j = 0; j < QK8_0; ++j) {
        x[0].qs[j] = -1;
    }
    // 32 * (-1 * 1) * (1.0 * 1.0) = -32.
    result = run_test("Negative values", QK8_0, x, y, tests);
    assert(result == -32.0f);

    x[0].d = 0.5f; // FP16 scale 0.5.
    y[0].d = 0.25f; // FP16 scale 0.25.
    for (int j = 0; j < QK8_0; ++j) {
        x[0].qs[j] = 1;
    }
    // 32 * (1 * 1) * (0.5 * 0.25) = 4.
    result = run_test("Fractional scales", QK8_0, x, y, tests);
    assert(result == 4.0f);

    x[1].d = 2.0f; // FP16 scale 2.0.
    y[1].d = 1.0f; // FP16 scale 1.0.
    for (int j = 0; j < QK8_0; ++j) {
        x[1].qs[j] = 2;
        y[1].qs[j] = 3;
    }
    // Keep the first block above. Total = 4 + 32 * (2 * 3) * (2.0 * 1.0) = 388.
    result = run_test("Two blocks", 2 * QK8_0, x, y, tests);
    assert(result == 388.0f);


    //////////////////More tests added starting here/////////////////////

    // A zero scale makes the represented values zero even with nonzero integers.
    x[0].d = 0.0f;
    result = run_test("Zero scale", QK8_0, x, y, tests);
    assert(result == 0.0f);

    x[0].d = 1.0f;
    y[0].d = 1.0f;
    for (int j = 0; j < QK8_0; ++j) {
        x[0].qs[j] = -128;
        y[0].qs[j] = -128;
    }
    result = run_test("Minimum integers", QK8_0, x, y, tests);
    assert(result == 524288.0f); // 32 * (-128 * -128).


    for (int j = 0; j < QK8_0; ++j) {
        x[0].qs[j] = 127;
        y[0].qs[j] = 127;
    }
    result = run_test("Maximum integers", QK8_0, x, y, tests);
    assert(result == 516128.0f); // 32 * (127 * 127).



    for (int j = 0; j < QK8_0; ++j) {
        x[0].qs[j] = -128;
    }
    result = run_test("Mixed integer limits", QK8_0, x, y, tests);
    assert(result == -520192.0f); // 32 * (-128 * 127).



    for (int j = 0; j < QK8_0; ++j) {
        //x[0].qs = { 1, -1, 1, -1, ... };
        //y[0].qs = { 1,  1, 1,  1, ... };
        x[0].qs[j] = (j % 2 == 0) ? 1 : -1;
        y[0].qs[j] = 1;
    }
    result = run_test("Cancellation within a block", QK8_0, x, y, tests);
    assert(result == 0.0f);



    x[1].d = 1.0f;
    y[1].d = 1.0f;
    for (int j = 0; j < QK8_0; ++j) {
        x[0].qs[j] = 1;
        x[1].qs[j] = -1;
        y[1].qs[j] = 1;
    }
    result = run_test("Cancellation across blocks", 2 * QK8_0, x, y, tests);
    assert(result == 0.0f);



    y[1].d = 1.0f - 1.0f / 1024.0f;
    result = run_test("Nearly cancelling blocks", 2 * QK8_0, x, y, tests);
    assert(result == 0.03125f); // 32 - 32 * (1 - 1/1024).


    result = run_test("Empty vector", 0, x, y, tests);
    assert(result == 0.0f);

    // These decimal scales are rounded when stored as half.
    // Assertion for this case is within the test function that checks < 10%
    x[0].d = 0.1f;
    y[0].d = 0.3f;
    for (int j = 0; j < QK8_0; ++j) {
        x[0].qs[j] = static_cast<int8_t>(j - 16);
        y[0].qs[j] = static_cast<int8_t>(j % 5 - 2);
    }
    run_test("Rounded decimal scales", QK8_0, x, y, tests);


    // These are more strict cases, corner cases

    // Half is 16 bit floating type. That means there are limits when we represent
    // numbers within this ranges
    // Half uses the form of sign fraction exponent where exponent ranges from -14 to 15
    // this means smallest positive value we can use is 1x2^-14 or 1/16384
    // subnormal cases happen implicitly based on the exponent value. 
    // If exponent is 0, that means the minimum we could achieve further goes down to -24 
    // value = + 2^-10 x 2^-14 

    const float min_normal_scale = 1.0f / 16384.0f;
    
    x[0].d = min_normal_scale;
    y[0].d = 1.0f;
    assert(static_cast<float>(x[0].d) == min_normal_scale);
    for (int j = 0; j < QK8_0; ++j) {
        x[0].qs[j] = 1;
        y[0].qs[j] = 1;
    }
    result = run_test("Minimum normal half scale", QK8_0, x, y, tests);
    assert(result == 1.0f / 512.0f);

    // subnormal or denormal case 
    const uint16_t min_subnormal_bits = 0x0001;

    
    // Copy exact FP16 bits because float-to-half assignment flushes subnormals.
    // We previously tried x[0].d = min_subnormal_scale;

    // 32 x 2^-24 x 1

    // Here min subnormal bits is 0x0001 and memcpy copies that into x[0].d without numerical conversion.
    std::memcpy(&x[0].d, &min_subnormal_bits, sizeof(min_subnormal_bits));
    result = run_test("Minimum subnormal half scale", QK8_0, x, y, tests);
    assert(result == 1.0f / 524288.0f);

    // 32 x 2^-24 x 2^-24
    std::memcpy(&y[0].d, &min_subnormal_bits, sizeof(min_subnormal_bits));
    result = run_test("Two subnormal half scales", QK8_0, x, y, tests);
    assert(result == 1.0f / 8796093022208.0f);

    // Check the sign bit and all ten fraction bits in the subnormal conversion.
    const uint16_t negative_subnormal_bits = 0x8001;
    std::memcpy(&x[0].d, &negative_subnormal_bits, sizeof(negative_subnormal_bits));
    y[0].d = 1.0f;
    result = run_test("Negative subnormal half scale", QK8_0, x, y, tests);
    assert(result == -1.0f / 524288.0f);

    const uint16_t max_subnormal_bits = 0x03ff;
    std::memcpy(&x[0].d, &max_subnormal_bits, sizeof(max_subnormal_bits));
    result = run_test("Maximum subnormal half scale", QK8_0, x, y, tests);
    assert(result == 1023.0f / 524288.0f);

    // 1.111111....(in binary) is 2- 2^-10 and exponent of 2^15 gives 65504
    x[0].d = 65504.0f;
    y[0].d = 65504.0f;
    for (int j = 0; j < QK8_0; ++j) {
        x[0].qs[j] = 127;
        y[0].qs[j] = 127;
    }
    run_test("Maximum finite half scales", QK8_0, x, y, tests);



     // Now testing with multiple blocks (8) here with different scales and values

    const float x_scales[max_blocks] = {0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 3.5f, 0.03125f, 16.0f};
    const float y_scales[max_blocks] = {0.3f, 2.0f, 0.125f, 0.75f, 1.5f, 0.0625f, 4.0f, 0.015625f};
    for (int ib = 0; ib < max_blocks; ++ib) {
        x[ib].d = x_scales[ib];
        y[ib].d = y_scales[ib];
        for (int j = 0; j < QK8_0; ++j) {
            x[ib].qs[j] = static_cast<int8_t>((ib * 37 + j * 13) % 256 - 128);
            y[ib].qs[j] = static_cast<int8_t>((ib * 19 + j * 29 + 7) % 256 - 128);
        }
    }
    run_test("Three blocks", 3 * QK8_0, x, y, tests);
    run_test("Eight blocks", max_blocks * QK8_0, x, y, tests);

    // A fixed seed random inputs

    std::mt19937 generator(42);
    std::uniform_real_distribution<float> random_scale(0.0f, 16.0f);
    std::uniform_int_distribution<int> random_integer(-128, 127);
    for (int ib = 0; ib < max_blocks; ++ib) {
        x[ib].d = random_scale(generator);
        y[ib].d = random_scale(generator);
        for (int j = 0; j < QK8_0; ++j) {
            x[ib].qs[j] = static_cast<int8_t>(random_integer(generator));
            y[ib].qs[j] = static_cast<int8_t>(random_integer(generator));
        }
    }
    run_test("Eight random blocks", max_blocks * QK8_0, x, y, tests);


    // Sequential FP32 accumulation can lose the remaining unit term due to floating point rounding off.

    // clear the previous inputs
    for (int ib = 0; ib < max_blocks; ++ib) {
        x[ib].d = 1.0f;
        y[ib].d = 1.0f;
        for (int j = 0; j < QK8_0; ++j) {
            x[ib].qs[j] = 0;
            y[ib].qs[j] = 0;
        }
    }
    x[0].d = y[0].d = x[2].d = y[2].d = 16.0f;
    for (int j = 0; j < QK8_0; ++j) {
        x[0].qs[j] = y[0].qs[j] = 64;
        x[2].qs[j] = -64;
        y[2].qs[j] = 64;
    }
    x[1].qs[0] = y[1].qs[0] = 1;
    // 32 x (64 x 64) x (16 x 16) = 2^25, and negative is -2^25
    // we want to see if adding small number to large can maintain accumulation accuracy.
    result = run_test("Serial cancellation <2^25, 1, -2^25, 0>", 4 * QK8_0, x, y, tests);
    assert(result == 1.0f);

    // Even a double accumulator loses 2^-48 between these opposite large terms.
    // This checks whether our fixed point accumulator preserves its smallest possible fractional 
    // contribution 2^-48 while adding very large values.
    x[0].d = y[0].d = y[2].d = 65504.0f;
    x[2].d = -65504.0f;
    for (int j = 0; j < QK8_0; ++j) {
        x[0].qs[j] = y[0].qs[j] = x[2].qs[j] = y[2].qs[j] = -128;
    }
    // earlier we defined min_subnormal_bits as 0x0001. We use the same method here
    std::memcpy(&x[1].d, &min_subnormal_bits, sizeof(min_subnormal_bits));
    std::memcpy(&y[1].d, &min_subnormal_bits, sizeof(min_subnormal_bits));
    //block 0 and block 2 are very large number that cancels out and block 1 is 2^-48
    // Supply the known answer because double accumulation would lose this residual.
    result = run_test("Maximum-scale cancellation leaves 2^-48", 3 * QK8_0, x, y, tests,
                      std::ldexp(1.0, -48));
    assert(result == std::ldexp(1.0f, -48));
    

    // All eight blocks reach the largest positive integer sum and half scales.
    for (int ib = 0; ib < max_blocks; ++ib) {
        x[ib].d = y[ib].d = 65504.0f;
        for (int j = 0; j < QK8_0; ++j) {
            x[ib].qs[j] = y[ib].qs[j] = -128;
        }
    }
    result = run_test("Maximum eight block sum", max_blocks * QK8_0, x, y, tests);
    assert(result == std::ldexp(4290774016.0f, 22));


    // Check if the dot product reaches the last integer of last block
    // Set every product zero except the final one

    for (int ib = 0; ib < max_blocks; ++ib) {
        for (int j = 0; j < QK8_0; ++j) {
            x[ib].qs[j] = 0;
            y[ib].qs[j] = 0;
        }
    }

    x[max_blocks - 1].d = 1.0f;
    y[max_blocks - 1].d = 1.0f;
    x[max_blocks - 1].qs[QK8_0 - 1] = -128;
    y[max_blocks - 1].qs[QK8_0 - 1] = 127;
    result = run_test("Last element boundary check", max_blocks * QK8_0, x, y, tests);
    assert(result == -16256.0f);

    printf("Maximum absolute error: %.9g\n", tests.max_absolute_error);
    // relative error is non zero eference
    printf("Maximum relative error: %.9g%%\n", tests.max_relative_error_percent);
    printf("%d test cases passed.\n", tests.count);
    return 0;
}
