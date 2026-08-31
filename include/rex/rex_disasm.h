/**
 * @file rex_disasm.h
 * @brief Capstone-backed instruction decode (internal).
 */
#ifndef REX_DISASM_H
#define REX_DISASM_H

#include "rex_types.h"

#ifdef __cplusplus
extern "C" {
#endif

rex_status rex_disasm_block(rex_arch arch, uint64_t linear, uint16_t seg, uint16_t off,
                            const uint8_t *code, size_t n, rex_insn *out, size_t cap,
                            size_t *wrote);

#ifdef __cplusplus
}
#endif

#endif /* REX_DISASM_H */
