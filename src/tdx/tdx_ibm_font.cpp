/**
 * @file tdx_ibm_font.cpp
 * @brief Detect IBM BIOS 8×8 CRT_CHAR_GEN (PCBIOS.ASM CRT_CHAR_GEN).
 */
#include "tdx/tdx_ibm_font.h"

int tdx_ibm_font_looks_cga8(const uint8_t *p, size_t n)
{
    static const uint8_t k_smile[8] = {0x7E, 0x81, 0xA5, 0x81, 0xBD, 0x99, 0x81, 0x7E};
    size_t i = 0;
    if ((p == nullptr) || (n < 1024u))
    {
        return 0;
    }
    for (i = 0; i < 8u; i++)
    {
        if (p[i] != 0)
        {
            return 0;
        }
    }
    for (i = 0; i < 8u; i++)
    {
        if (p[8u + i] != k_smile[i])
        {
            return 0;
        }
    }
    return 1;
}
