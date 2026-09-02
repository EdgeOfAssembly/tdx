/**
 * @file ibm_bda.h
 * @brief IBM PC BIOS Data Area at 0040:0000 (linear 0x400).
 *
 * Only the fields tdx actually uses are named; the rest is padding so
 * offsets match the IBM PC Technical Reference. There is no BIOS Parameter
 * Block (boot-sector BPB) or INT 1Eh diskette table overlay in this tree yet.
 */
#ifndef IBM_BDA_H
#define IBM_BDA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 1)
/** BIOS Data Area, 0040:0000. Overlay at @c ram + 0x400. */
typedef struct ibm_bda
{
    uint16_t com_base[4];     /**< 00h */
    uint16_t lpt_base[4];     /**< 08h */
    uint16_t equipment;       /**< 10h */
    uint8_t mfg;              /**< 12h */
    uint16_t mem_kb;          /**< 13h: conventional memory KiB (640) */
    uint8_t pad_15_48[0x49 - 0x15];
    uint8_t video_mode;       /**< 49h: CRT_MODE */
    uint16_t video_cols;      /**< 4Ah */
    uint8_t pad_4c_65[0x66 - 0x4C];
    uint8_t crt_palette;      /**< 66h: CGA color-select / INT 10 AH=0Bh */
    uint8_t pad_67_6b[0x6C - 0x67];
    uint32_t timer_ticks;     /**< 6Ch: 18.2 Hz dword */
    uint8_t timer_overflow;   /**< 70h: midnight flag */
} ibm_bda;
#pragma pack(pop)

#ifndef __CPROVER__
static_assert(offsetof(ibm_bda, mem_kb) == 0x13, "BDA mem_kb");
static_assert(offsetof(ibm_bda, video_mode) == 0x49, "BDA video_mode");
static_assert(offsetof(ibm_bda, crt_palette) == 0x66, "BDA crt_palette");
static_assert(offsetof(ibm_bda, timer_ticks) == 0x6C, "BDA timer_ticks");
static_assert(offsetof(ibm_bda, timer_overflow) == 0x70, "BDA timer_overflow");
#endif

/** @return BDA overlay, or NULL if @p ram is NULL. */
static inline ibm_bda *ibm_bda_at(uint8_t *ram)
{
    return (ram != NULL) ? (ibm_bda *)(ram + 0x400) : NULL;
}

#ifdef __cplusplus
}
#endif

#endif /* IBM_BDA_H */
