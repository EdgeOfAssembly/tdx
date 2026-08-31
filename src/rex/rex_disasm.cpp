/**
 * @file rex_disasm.cpp
 * @brief Capstone wrapper for 16-bit x86 (other arches reserved).
 */

#include "rex/rex_disasm.h"

#include <capstone/capstone.h>

#include <cassert>
#include <cstdio>
#include <cstring>

rex_status rex_disasm_block(rex_arch arch, uint64_t linear, uint16_t seg, uint16_t off,
                            const uint8_t *code, size_t n, rex_insn *out, size_t cap,
                            size_t *wrote)
{
    csh handle = 0;
    cs_insn *insn = nullptr;
    size_t count = 0;
    size_t i = 0;
    size_t take = 0;
    uint64_t addr = linear;

    if (wrote != nullptr)
    {
        *wrote = 0;
    }
    if ((code == NULL) || (out == NULL) || (cap == 0) || (n == 0))
    {
        return REX_ERR_ARG;
    }
    if (arch != REX_ARCH_I8086)
    {
        return REX_ERR_NOSYS;
    }
    if (cs_open(CS_ARCH_X86, CS_MODE_16, &handle) != CS_ERR_OK)
    {
        return REX_ERR_CPU;
    }
    cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);
    count = cs_disasm(handle, code, n, addr, cap, &insn);
    if (count == 0)
    {
        cs_close(&handle);
        return REX_ERR_CPU;
    }
    take = count;
    if (take > cap)
    {
        take = cap;
    }
    for (i = 0; i < take; i++)
    {
        rex_insn *dst = &out[i];
        const cs_detail *d = insn[i].detail;
        size_t k = 0;
        std::memset(dst, 0, sizeof(*dst));
        dst->linear = insn[i].address;
        dst->seg = seg;
        dst->off = (uint16_t)(off + (uint16_t)(insn[i].address - linear));
        dst->size = (uint8_t)insn[i].size;
        assert(insn[i].size <= 16);
        std::memcpy(dst->bytes, insn[i].bytes, insn[i].size);
        std::snprintf(dst->text, sizeof(dst->text), "%.40s %.50s", insn[i].mnemonic,
                      insn[i].op_str);
        if (d != nullptr)
        {
            for (k = 0; k < d->groups_count; k++)
            {
                const uint8_t g = d->groups[k];
                dst->is_call = dst->is_call || (g == CS_GRP_CALL);
                dst->is_int = dst->is_int || (g == CS_GRP_INT);
                dst->is_ret = dst->is_ret || (g == CS_GRP_RET);
                dst->is_jump = dst->is_jump || (g == CS_GRP_JUMP);
            }
        }
        if ((std::strncmp(insn[i].mnemonic, "loop", 4) == 0) ||
            (std::strcmp(insn[i].mnemonic, "jcxz") == 0) ||
            (std::strcmp(insn[i].mnemonic, "jecxz") == 0))
        {
            dst->is_loop = true;
        }
        if (d != nullptr)
        {
            const uint8_t pref = d->x86.prefix[0];
            dst->is_rep = (pref == X86_PREFIX_REP) || (pref == X86_PREFIX_REPNE);
        }
    }
    if (wrote != nullptr)
    {
        *wrote = take;
    }
    cs_free(insn, count);
    cs_close(&handle);
    return REX_OK;
}
