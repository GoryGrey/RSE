#ifndef RSE_BOOT_H
#define RSE_BOOT_H

#include <stdint.h>

#define RSE_BOOT_MAGIC 0x525345554649424FULL

struct rse_boot_info {
    uint64_t magic;
    uint64_t fb_addr;
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t fb_pitch;
    uint32_t fb_bpp;
    uint64_t system_table;
};

#endif
