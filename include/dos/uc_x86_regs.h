/**
 * @file uc_x86_regs.h
 * @brief Unicorn x86 register IDs (or stubs) and optional uc_emu_stop helper.
 *
 * @note When @c USE_UNICORN is 0, MZ EXE / Unicorn-only paths are unavailable;
 *       iron86 (@c --floppy-a / @c --bios) does not need Unicorn.
 */
#ifndef DOS_UC_X86_REGS_H
#define DOS_UC_X86_REGS_H

#ifndef USE_UNICORN
#define USE_UNICORN 1
#endif

#if USE_UNICORN
#include <unicorn/unicorn.h>
#include <unicorn/x86.h>
#else
/** Values match Unicorn 2 @c uc_x86_reg (dos_machine register-id ABI). */
enum : int
{
    UC_X86_REG_AX = 3,
    UC_X86_REG_BP = 6,
    UC_X86_REG_BX = 8,
    UC_X86_REG_CS = 11,
    UC_X86_REG_CX = 12,
    UC_X86_REG_DI = 14,
    UC_X86_REG_DS = 17,
    UC_X86_REG_DX = 18,
    UC_X86_REG_EIP = 26,
    UC_X86_REG_ES = 28,
    UC_X86_REG_IP = 34,
    UC_X86_REG_SI = 45,
    UC_X86_REG_SP = 47,
    UC_X86_REG_SS = 49,
    UC_X86_REG_FLAGS = 252
};
#endif

struct uc_struct;
typedef struct uc_struct uc_engine;

/** Stop Unicorn emulation; no-op when Unicorn is disabled or @p uc is null. */
static inline void dos_uc_stop(uc_engine *uc)
{
#if USE_UNICORN
    if (uc != nullptr)
    {
        uc_emu_stop(uc);
    }
#else
    (void)uc;
#endif
}

#endif /* DOS_UC_X86_REGS_H */
