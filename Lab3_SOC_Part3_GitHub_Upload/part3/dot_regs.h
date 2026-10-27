#ifndef LAB3_DOT_REGS_H
#define LAB3_DOT_REGS_H
#include <stdint.h>

// Shared by the SystemC model, bare MMIO tests, and the future ARM HAL/driver.
#define DOT_PHYS_BASE       UINT64_C(0xa1000000)
#define DOT_MMIO_SIZE       0x10000u
#define DOT_REG_CONTROL     0x00u
#define DOT_REG_STATUS      0x04u
#define DOT_REG_N_ELEMENTS  0x08u
#define DOT_REG_RESULT      0x0cu
#define DOT_REG_ERROR       0x10u
#define DOT_REG_IRQ_ENABLE  0x14u
#define DOT_REG_ID          0x18u
#define DOT_REG_CYCLES      0x1cu
#define DOT_REG_CAPACITY    0x20u
#define DOT_CONTROL_START  1u
#define DOT_CONTROL_ACK    2u
#define DOT_STATUS_BUSY    1u
#define DOT_STATUS_DONE    2u
#define DOT_STATUS_ERROR   4u
#define DOT_ID             0x51384450u
#define DOT_MAX_BLOCKS     128u
#define DOT_BLOCK_ELEMENTS 32u
#define DOT_MAX_ELEMENTS   (DOT_MAX_BLOCKS * DOT_BLOCK_ELEMENTS)
#define DOT_SLOT_BYTES     64u
#define DOT_BUFFER_BYTES   (DOT_MAX_BLOCKS * DOT_SLOT_BYTES)
#define DOT_X_OFFSET       0x1000u
#define DOT_Y_OFFSET       0x3000u
#define DOT_IRQ_INDEX      5u
#define DOT_CLOCK_NS       10u
#define DOT_BUS_NS         10u

// Each 64-byte slot: scale bits LE at bytes 0..1, int8 qs at 2..33,
// padding at 34..63. One slot corresponds to the HLS 512-bit memory word.
static inline uint32_t dot_load_le32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline void dot_store_le32(unsigned char *p, uint32_t v) {
    unsigned i;
    for (i = 0; i < 4; ++i) p[i] = (unsigned char)(v >> (8 * i));
}
#endif
