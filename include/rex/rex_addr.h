/**
 * @file rex_addr.h
 * @brief Parse real-mode seg:off or linear hex for tdxctl / the agent socket.
 */
#ifndef REX_ADDR_H
#define REX_ADDR_H

#include "rex_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Canonical CS:IP for a 20-bit linear address: CS = lin>>4, IP = lin&0xF.
 */
static inline void rex_linear_to_canonical(uint32_t lin, uint16_t *seg, uint16_t *off)
{
    lin &= 0xFFFFFu;
    if (seg != NULL)
    {
        *seg = (uint16_t)(lin >> 4);
    }
    if (off != NULL)
    {
        *off = (uint16_t)(lin & 0xFu);
    }
}

/**
 * @brief Parse @c "0FDB:A360", @c "0xFE199", @c "FE199h", @c "$FE199", or @c "linear FE199".
 *
 * Bare numbers are hex (debugger convention). On linear input, @a oseg/@a ooff
 * get the canonical split so setip does not write CS:IP = 0000:0000.
 *
 * @retval true Parsed; @a lin always set.
 */
static inline bool rex_parse_addr(const char *s, uint64_t *lin, uint16_t *oseg, uint16_t *ooff)
{
    unsigned seg = 0;
    unsigned off = 0;
    const char *p = s;
    char *end = NULL;
    unsigned long v = 0;

    if ((s == NULL) || (lin == NULL) || (s[0] == '\0'))
    {
        return false;
    }
    while ((*p == ' ') || (*p == '\t'))
    {
        p++;
    }
    if ((strncmp(p, "linear", 6) == 0) && ((p[6] == '\0') || (p[6] == ' ') || (p[6] == ':') || (p[6] == '\t')))
    {
        p += 6;
        while ((*p == ' ') || (*p == '\t') || (*p == ':'))
        {
            p++;
        }
    }
    if (strchr(p, ':') != NULL)
    {
        if (sscanf(p, "%x:%x", &seg, &off) != 2)
        {
            return false;
        }
        *lin = rex_segoff_to_linear((uint16_t)seg, (uint16_t)off);
        if (oseg != NULL)
        {
            *oseg = (uint16_t)seg;
        }
        if (ooff != NULL)
        {
            *ooff = (uint16_t)off;
        }
        return true;
    }
    if (*p == '$')
    {
        p++;
    }
    else if ((p[0] == '0') && ((p[1] == 'x') || (p[1] == 'X')))
    {
        p += 2;
    }
    v = strtoul(p, &end, 16);
    if ((end == NULL) || (end == p))
    {
        return false;
    }
    if ((*end == 'h') || (*end == 'H'))
    {
        end++;
    }
    if (*end != '\0')
    {
        return false;
    }
    v &= 0xFFFFFul;
    *lin = v;
    rex_linear_to_canonical((uint32_t)v, oseg, ooff);
    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* REX_ADDR_H */
