/**
 * @file ea.h
 * @brief 8086 ModR/M effective-address state (Py86 `_decode_modrm` port).
 */
#ifndef IRON86_EA_H
#define IRON86_EA_H

#include <cstdint>

namespace iron86
{

/**
 * @brief Decoded ModR/M byte plus 16-bit effective address.
 *
 * @p seg is DS, or SS for BP-based forms (rm=2,3 and rm=6 with mod!=0).
 * Direct disp16 (mod=0, rm=6) stays DS. Unused when @p mod == 3.
 */
struct modrm
{
    uint8_t mod = 0;
    uint8_t reg = 0;
    uint8_t rm = 0;
    uint16_t ea = 0;
    uint16_t seg = 0;
};

} // namespace iron86

#endif /* IRON86_EA_H */
