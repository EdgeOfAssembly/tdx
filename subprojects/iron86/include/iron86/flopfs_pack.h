/**
 * @file flopfs_pack.h
 * @brief Pack a host directory into a 360K FlopFS image (read/execute).
 *
 * Non-recursive: regular files only, 8.3 names. No kernel/stage on the
 * image (data disk for B:). Max 32 dirents, 360K 40/2/9.
 */
#ifndef IRON86_FLOPFS_PACK_H
#define IRON86_FLOPFS_PACK_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace iron86
{

/**
 * @brief Build a 368640-byte FlopFS image from DIR.
 * @param[in] dir Host directory (must exist).
 * @param[out] out Image bytes (720*512) on success.
 * @return true if packed; false if missing dir, too many files, or overflow.
 */
bool flopfs_pack_dir(const char *dir, std::vector<uint8_t> *out);

} // namespace iron86

#endif /* IRON86_FLOPFS_PACK_H */
