/**
 * @file rex_types.h
 * @brief Portable types and architecture tags for the librex debug core.
 *
 * The core is CPU-agnostic: DOS 8086 is the first backend. Z80 / 6502 / M68K
 * targets are expected to implement the same @c rex_target_ops vtable later
 * (ZX Spectrum, C64, Amiga, Atari, Amstrad, …).
 */
#ifndef REX_TYPES_H
#define REX_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef REX_API
#define REX_API
#endif

/**
 * @brief CPU families the session can host.
 *
 * Only @c REX_ARCH_I8086 is implemented in v0.1. Other values are reserved so
 * emulators can share this ABI without a break.
 */
typedef enum rex_arch
{
    REX_ARCH_NONE = 0,
    REX_ARCH_I8086 = 1,  /**< Real-mode 8086/186/286 (DOS EXE/COM). */
    REX_ARCH_Z80 = 2,    /**< Reserved: ZX Spectrum / CPC. */
    REX_ARCH_M6502 = 3,  /**< Reserved: C64 / Atari 8-bit. */
    REX_ARCH_M68K = 4,   /**< Reserved: Amiga / ST. */
    REX_ARCH_ARM = 5     /**< Reserved. */
} rex_arch;

/** Status codes: 0 is success; negative is failure. */
typedef enum rex_status
{
    REX_OK = 0,
    REX_ERR_ARG = -1,
    REX_ERR_IO = -2,
    REX_ERR_FMT = -3,
    REX_ERR_CPU = -4,
    REX_ERR_MEM = -5,
    REX_ERR_NOSYS = -6,
    REX_ERR_TIMEOUT = -7,
    REX_ERR_BUSY = -8,
    REX_ERR_SOCK = -9
} rex_status;

/** Why the last step/run stopped. */
typedef enum rex_stop
{
    REX_STOP_NONE = 0,
    REX_STOP_STEP = 1,
    REX_STOP_BREAK = 2,
    REX_STOP_HALTED = 3,
    REX_STOP_FAULT = 4,
    REX_STOP_TIMEOUT = 5,
    REX_STOP_WAIT_KEY = 6,
    REX_STOP_REQUEST = 7
} rex_stop;

/**
 * @brief Real-mode 8086 user-visible registers (little-endian DOS).
 */
typedef struct rex_regs_i8086
{
    uint16_t ax;
    uint16_t bx;
    uint16_t cx;
    uint16_t dx;
    uint16_t si;
    uint16_t di;
    uint16_t bp;
    uint16_t sp;
    uint16_t cs;
    uint16_t ds;
    uint16_t es;
    uint16_t ss;
    uint16_t ip;
    uint16_t flags;
} rex_regs_i8086;

/**
 * @brief One disassembled instruction (UTF-8 mnemonic + operands).
 */
typedef struct rex_insn
{
    uint64_t linear;     /**< Physical / linear address. */
    uint16_t seg;        /**< Segment if applicable; 0 otherwise. */
    uint16_t off;        /**< Offset if applicable. */
    uint8_t size;        /**< Length in bytes (1..15). */
    uint8_t bytes[16];   /**< Raw opcode bytes. */
    char text[96];       /**< "mnemonic operands". */
    bool is_call;
    bool is_int;
    bool is_ret;
    bool is_jump;
    bool is_loop;
    bool is_rep;
    uint64_t target; /**< IMM jump/call destination (Capstone address space). */
} rex_insn;

/**
 * @brief Convert real-mode seg:off to a 20-bit-plus linear address.
 *
 * @param[in] seg Segment selector.
 * @param[in] off 16-bit offset.
 * @return (seg << 4) + off (may exceed 1 MiB when A20 is on).
 */
static inline uint32_t rex_segoff_to_linear(uint16_t seg, uint16_t off)
{
    return ((uint32_t)seg << 4) + (uint32_t)off;
}

#ifdef __cplusplus
}
#endif

#endif /* REX_TYPES_H */
