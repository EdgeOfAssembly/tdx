/**
 * @file chs_lba.h
 * @brief CHS → LBA for floppy INT 13h (1-based) and Xebec DCB (0-based).
 *
 * C linkage so CBMC can check the same formula the emulator uses.
 */
#ifndef IRON86_CHS_LBA_H
#define IRON86_CHS_LBA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Linear sector index from cylinder/head/sector.
 *
 * @param[in] cyl   Cylinder (0-based).
 * @param[in] heads Number of heads in the geometry (>= 1).
 * @param[in] head  Head (0-based, < @p heads).
 * @param[in] spt   Sectors per track (>= 1).
 * @param[in] sec   Sector (1-based, <= @p spt).
 * @return `((cyl * heads) + head) * spt + (sec - 1)`.
 *
 * @warning @p sec must be >= 1; callers reject 0 before calling.
 */
static inline uint32_t iron86_chs_to_lba(uint32_t cyl, uint32_t heads, uint32_t head,
                                         uint32_t spt, uint32_t sec)
{
    return ((cyl * heads) + head) * spt + (sec - 1u);
}

/**
 * @brief Linear sector index from IBM Xebec DCB CHS.
 *
 * @param[in] cyl      Cylinder (0-based).
 * @param[in] heads    Number of heads (>= 1).
 * @param[in] head     Head (0-based, < @p heads).
 * @param[in] spt      Sectors per track (>= 1).
 * @param[in] sector0  Sector in the DCB (0-based, < @p spt). IBM ROS stores
 *                     `CL-1` here; do not pass INT 13h CL.
 * @return `((cyl * heads) + head) * spt + sector0`.
 */
static inline uint32_t iron86_hdc_chs_to_lba(uint32_t cyl, uint32_t heads, uint32_t head,
                                             uint32_t spt, uint32_t sector0)
{
    return ((cyl * heads) + head) * spt + sector0;
}

#ifdef __cplusplus
}
#endif

#endif /* IRON86_CHS_LBA_H */
