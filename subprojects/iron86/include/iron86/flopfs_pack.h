/**
 * @file flopfs_pack.h
 * @brief Pack a host directory into a 360K or 720K FlopFS image (read/execute).
 *
 * Recursive: regular 8.3 files (no truncation); subdirs flattened to the
 * root (duplicate FCB names fail). No kernel/stage (data disk for B:).
 * Smallest image that fits: 360K → 720K → 1.2M → 1.44M. Root dirents grow
 * with the file count (16 per sector, minimum 32).
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
/** @brief 1.2M 5.25" HD: 80 cyl × 2 heads × 15 spt. */
constexpr uint32_t FLOPFS_SECS_1200K = 2400;
/** @brief 1.44M 3.5" HD: 80 cyl × 2 heads × 18 spt. */
constexpr uint32_t FLOPFS_SECS_1440K = 2880;
/** @brief Bytes in a 360K image. */
constexpr uint32_t FLOPFS_BYTES_360K = FLOPFS_SECS_360K * 512u;
/** @brief Bytes in a 720K image. */
constexpr uint32_t FLOPFS_BYTES_720K = FLOPFS_SECS_720K * 512u;
/** @brief Bytes in a 1.2M image. */
constexpr uint32_t FLOPFS_BYTES_1200K = FLOPFS_SECS_1200K * 512u;
/** @brief Bytes in a 1.44M image. */
constexpr uint32_t FLOPFS_BYTES_1440K = FLOPFS_SECS_1440K * 512u;

/**
 * @brief Build the smallest FlopFS image that fits DIR (360K–1.44M).
 * @param[in] dir Host directory (must exist).
 * @param[out] out Image bytes on success.
 * @return true if packed; false if missing dir, duplicate 8.3 names, or >1.44M.
 */
bool flopfs_pack_dir(const char *dir, std::vector<uint8_t> *out);

/**
 * @brief Pack DIR as a Type-1-sized FAT12 HDD image (MBR + one primary).
 *
 * Recursive 8.3 files flattened to C:\\. @p cylinders/heads/spt default to
 * IBM XT Type-1 when zero. Fails if the tree does not fit the volume.
 */
bool hdc_pack_dir(const char *dir, uint32_t cylinders, uint32_t heads, uint32_t spt,
                  std::vector<uint8_t> *out);

} // namespace iron86

#endif /* IRON86_FLOPFS_PACK_H */
