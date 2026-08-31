/**
 * @file mz_parse.h
 * @brief DOS MZ EXE header parser (C23, CBMC-friendly).
 */
#ifndef MZ_PARSE_H
#define MZ_PARSE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Parsed MZ fields needed to load a real-mode image. */
typedef struct mz_info
{
    int is_mz;              /**< 1 if MZ/ZM magic. */
    uint16_t header_paras;  /**< Size of header in 16-byte paragraphs. */
    uint16_t reloc_count;
    uint16_t reloc_offset;
    uint16_t cs;
    uint16_t ip;
    uint16_t ss;
    uint16_t sp;
    uint16_t min_alloc;
    uint16_t max_alloc;
    uint16_t overlay;
    uint32_t header_bytes;
    uint32_t image_size;    /**< Bytes after the header to load. */
} mz_info;

/**
 * @brief Parse an MZ header from a file image prefix.
 *
 * @param[in]  buf       File bytes (at least 28 bytes for MZ).
 * @param[in]  buf_len   Length of @p buf.
 * @param[in]  file_size Full file size on disk.
 * @param[out] out       Result; zeroed on failure.
 *
 * @return 0 on success (MZ), 1 if not MZ (caller may treat as COM),
 *         -1 on truncated/corrupt MZ.
 */
int mz_parse_header(const uint8_t *buf, size_t buf_len, uint32_t file_size, mz_info *out);

/**
 * @brief One relocation entry: @c (seg:off) within the load image.
 */
typedef struct mz_reloc
{
    uint16_t off;
    uint16_t seg;
} mz_reloc;

/**
 * @brief Read relocation table from the file image.
 *
 * @param[in]  file      Entire file or enough to cover the table.
 * @param[in]  file_len  Length of @p file.
 * @param[in]  info      Parsed header.
 * @param[out] out       Array of @p cap entries.
 * @param[in]  cap       Capacity.
 * @param[out] wrote     Optional count.
 *
 * @return 0 on success, -1 on overflow/truncation.
 */
int mz_parse_relocs(const uint8_t *file, size_t file_len, const mz_info *info, mz_reloc *out,
                    size_t cap, size_t *wrote);

#ifdef __cplusplus
}
#endif

#endif /* MZ_PARSE_H */
