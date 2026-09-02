/**
 * @file mz_parse.c
 * @brief DOS MZ EXE header + relocation table parser.
 */

#include "dos/mz_parse.h"

#include <string.h>

REX_C_DEF int mz_parse_header(const uint8_t *buf, size_t buf_len, uint32_t file_size, mz_info *out)
{
    uint16_t magic = 0;
    uint16_t last = 0;
    uint16_t pages = 0;
    uint32_t hdr = 0;
    uint32_t declared = 0;

    if (out == NULL)
    {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    if ((buf == NULL) || (buf_len < 28u))
    {
        return 1;
    }

    {
        mz_exe_hdr eh;
        memcpy(&eh, buf, sizeof(eh));
        magic = eh.magic;
        last = eh.last_page_bytes;
        pages = eh.pages;
        out->reloc_count = eh.reloc_count;
        out->header_paras = eh.header_paras;
        out->min_alloc = eh.min_alloc;
        out->max_alloc = eh.max_alloc;
        out->ss = eh.ss;
        out->sp = eh.sp;
        out->ip = eh.ip;
        out->cs = eh.cs;
        out->reloc_offset = eh.reloc_offset;
        out->overlay = eh.overlay;
    }
    if ((magic != 0x5A4Du) && (magic != 0x4D5Au))
    {
        return 1;
    }

    out->is_mz = 1;
    hdr = (uint32_t)out->header_paras * 16u;
    out->header_bytes = hdr;

    if ((out->header_paras == 0u) || (hdr > file_size) || (pages == 0u))
    {
        return -1;
    }

    if (last == 0u)
    {
        declared = (uint32_t)pages * 512u;
    }
    else
    {
        declared = ((uint32_t)pages - 1u) * 512u + (uint32_t)last;
    }
    if (declared < hdr)
    {
        return -1;
    }
    out->image_size = declared - hdr;
    if ((hdr + out->image_size) > file_size)
    {
        out->image_size = file_size - hdr;
    }
    return 0;
}

REX_C_DEF int mz_parse_relocs(const uint8_t *file, size_t file_len, const mz_info *info, mz_reloc *out,
                    size_t cap, size_t *wrote)
{
    size_t i = 0;
    size_t n = 0;
    uint32_t off = 0;

    if (wrote != NULL)
    {
        *wrote = 0;
    }
    if ((file == NULL) || (info == NULL) || (out == NULL))
    {
        return -1;
    }
    if (info->is_mz == 0)
    {
        return 0;
    }
    n = (size_t)info->reloc_count;
    if (n > cap)
    {
        return -1;
    }
    off = (uint32_t)info->reloc_offset;
    if ((off + (uint32_t)n * 4u) > file_len)
    {
        return -1;
    }
    for (i = 0; i < n; i++)
    {
        mz_reloc rel;
        memcpy(&rel, file + off + (uint32_t)i * 4u, sizeof(rel));
        out[i] = rel;
    }
    if (wrote != NULL)
    {
        *wrote = n;
    }
    return 0;
}
