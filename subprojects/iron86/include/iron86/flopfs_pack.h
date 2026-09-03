/**
 * @file flopfs_pack.h
 * @brief Pack a host directory into a 360K or 720K FlopFS image (read/execute).
 *
 * Non-recursive: regular files only, already-8.3 names (no truncation).
 * No kernel/stage on the image (data disk for B:). Max 32 dirents.
 * Fits 360K (40/2/9 = 720 sectors) when possible; otherwise 720K
 * (80/2/9 = 1440 sectors). Same SPT/heads as FloppyOS disk_read.
 */
#ifndef IRON86_FLOPFS_PACK_H
#define IRON86_FLOPFS_PACK_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace iron86
{

/** @brief 360K 5.25" DD: 40 cyl × 2 heads × 9 spt. */
constexpr uint32_t FLOPFS_SECS_360K = 720;
/** @brief 720K 3.5"/5.25" DD: 80 cyl × 2 heads × 9 spt. */
constexpr uint32_t FLOPFS_SECS_720K = 1440;
/** @brief Bytes in a 360K image. */
constexpr uint32_t FLOPFS_BYTES_360K = FLOPFS_SECS_360K * 512u;
/** @brief Bytes in a 720K image. */
constexpr uint32_t FLOPFS_BYTES_720K = FLOPFS_SECS_720K * 512u;

/**
 * @brief Build a 360K or 720K FlopFS image from DIR.
 * @param[in] dir Host directory (must exist).
 * @param[out] out Image bytes (720*512 or 1440*512) on success.
 * @return true if packed; false if missing dir, too many files, or >720K.
 */
bool flopfs_pack_dir(const char *dir, std::vector<uint8_t> *out);

} // namespace iron86

#endif /* IRON86_FLOPFS_PACK_H */
