// Part 3 platform smoke test; the Part 4 kernel driver/HAL is still separate work.
#include "dot_regs.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
static volatile unsigned char *base;
static uint32_t rd(unsigned r) { return *(volatile uint32_t *)(base+r); }
static void wr(unsigned r, uint32_t v) { *(volatile uint32_t *)(base+r) = v; __sync_synchronize(); }
int main(void) {
    const unsigned lengths[] = {0, 1, 3, 128};
    int fd = open("/dev/mem", O_RDWR|O_SYNC);
    if (fd < 0) { perror("open /dev/mem"); return 1; }
    void *mapping = mmap(NULL, DOT_MMIO_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, (off_t)DOT_PHYS_BASE);
    if (mapping == MAP_FAILED) { perror("mmap"); close(fd); return 1; }
    base = (volatile unsigned char *)mapping;
    if (rd(DOT_REG_ID) != DOT_ID || rd(DOT_REG_CAPACITY) != DOT_MAX_ELEMENTS) {
        fprintf(stderr,"Unexpected accelerator ID or capacity\n"); return 1;
    }
    if (rd(DOT_REG_STATUS) & DOT_STATUS_BUSY) { fprintf(stderr,"Accelerator already busy\n"); return 1; }
    wr(DOT_REG_IRQ_ENABLE, 0); wr(DOT_REG_CONTROL, DOT_CONTROL_ACK);
    for (unsigned b = 0; b < DOT_MAX_BLOCKS; ++b) {
        unsigned char slot[DOT_SLOT_BYTES] = {0}; slot[1] = 0x3c;
        memset(slot+2, 1, DOT_BLOCK_ELEMENTS);
        for (unsigned w = 0; w < DOT_SLOT_BYTES; w += 4) {
            const uint32_t v = dot_load_le32(slot+w);
            wr(DOT_X_OFFSET+b*DOT_SLOT_BYTES+w, v);
            wr(DOT_Y_OFFSET+b*DOT_SLOT_BYTES+w, v);
        }
    }
    for (unsigned i = 0; i < sizeof(lengths)/sizeof(lengths[0]); ++i) {
        const unsigned n = lengths[i];
        wr(DOT_REG_N_ELEMENTS, n*32); wr(DOT_REG_CONTROL, DOT_CONTROL_START);
        unsigned polls = 0;
        while (!(rd(DOT_REG_STATUS) & DOT_STATUS_DONE) && ++polls < 100000) usleep(10);
        if (!(rd(DOT_REG_STATUS) & DOT_STATUS_DONE) || (rd(DOT_REG_STATUS) & DOT_STATUS_ERROR)) {
            fprintf(stderr,"Timeout/device error: %u\n",rd(DOT_REG_ERROR)); return 1;
        }
        uint32_t result_bits = rd(DOT_REG_RESULT); float result;
        memcpy(&result, &result_bits, sizeof result);
        printf("blocks=%u result=%g expected=%u modeled_cycles=%u\n", n, result, n*32, rd(DOT_REG_CYCLES));
        if (result != (float)(n*32)) return 1;
        wr(DOT_REG_CONTROL, DOT_CONTROL_ACK);
    }
    puts("QEMU MMIO SMOKE TEST PASSED");
    munmap(mapping, DOT_MMIO_SIZE); close(fd); return 0;
}
