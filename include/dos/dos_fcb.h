/**
 * @file dos_fcb.h
 * @brief MS-DOS FCB as laid out in RAM (MS-DOS 1.25 FCBLOCK + 4th RR byte).
 */
#ifndef DOS_FCB_H
#define DOS_FCB_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 1)
/** 37-byte FCB (drive+name through RR including optional high byte). */
typedef struct dos_fcb
{
    uint8_t drive;     /**< 00h: 0 = default, 1 = A: */
    char name[8];      /**< 01h: space-padded */
    char ext[3];       /**< 09h: space-padded */
    uint16_t extent;   /**< 0Ch: current block */
    uint16_t recsiz;   /**< 0Eh: record size (OPEN sets 128) */
    uint32_t filsiz;   /**< 10h: file size */
    uint16_t fdate;    /**< 14h */
    uint16_t ftime;    /**< 16h */
    uint8_t devid;     /**< 18h: DOS device id; tdx stores host fd */
    uint16_t firclus;  /**< 19h: first cluster (unaligned in 1.25); tdx cookie in low byte */
    uint16_t lstclus;  /**< 1Bh */
    uint16_t cluspos;  /**< 1Dh */
    uint8_t nr_pad;    /**< 1Fh: pad so NR is at 20h */
    uint8_t nr;        /**< 20h: next sequential record */
    uint8_t rr[3];     /**< 21h: random record (3 bytes if recsiz >= 64) */
    uint8_t rr_hi;     /**< 24h: 4th RR byte if recsiz < 64 */
} dos_fcb;
#pragma pack(pop)

#ifndef __CPROVER__
static_assert(sizeof(dos_fcb) == 37, "FCB is 37 bytes");
static_assert(offsetof(dos_fcb, recsiz) == 0x0E, "RECSIZ");
static_assert(offsetof(dos_fcb, filsiz) == 0x10, "FILSIZ");
static_assert(offsetof(dos_fcb, devid) == 0x18, "DEVID");
static_assert(offsetof(dos_fcb, nr) == 0x20, "NR");
static_assert(offsetof(dos_fcb, rr) == 0x21, "RR");
static_assert(offsetof(dos_fcb, rr_hi) == 0x24, "RR hi");
#endif

#ifdef __cplusplus
}
#endif

#endif /* DOS_FCB_H */
