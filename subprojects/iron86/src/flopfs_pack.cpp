/**
 * @file flopfs_pack.cpp
 * @brief Pack a host directory into a 360K FlopFS data disk (B:).
 */
#include "iron86/flopfs_pack.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace iron86
{
namespace
{

constexpr uint32_t k_sec = 512;
constexpr uint32_t k_secs = 720;
constexpr uint32_t k_bytes = k_sec * k_secs;
constexpr uint32_t k_root_ents = 32;
constexpr uint32_t k_dirent = 32;
constexpr uint32_t k_root_secs = 2;
constexpr uint32_t k_sb_lba = 1;
constexpr uint32_t k_sbm_lba = 2;
constexpr uint32_t k_root_lba = 3;
constexpr uint32_t k_data_lba = 5;

#pragma pack(push, 1)
struct flopfs_superblock
{
    char magic[8];
    uint16_t version_major;
    uint16_t version_minor;
    uint16_t sector_size;
    uint16_t sector_count;
    uint32_t stage15_lba;
    uint16_t stage15_sectors;
    uint16_t flags;
    uint32_t generation;
    uint32_t crc32;
    char label[16];
    uint32_t kernel_lba;
    uint16_t kernel_sectors;
    uint16_t kernel_load_seg;
    uint8_t kernel_codec;
    uint8_t reserved0;
    uint32_t com_lba;
    uint16_t com_sectors;
    uint16_t com_load_seg;
    uint32_t root_lba;
    uint16_t root_sectors;
    char init_name[11];
    uint8_t reserved[429];
};

struct flopfs_dirent
{
    char name[11];
    uint8_t codec;
    uint32_t lba;
    uint32_t size;
    uint16_t sectors;
    uint8_t pad[10];
};
#pragma pack(pop)

static_assert(sizeof(flopfs_superblock) == 512, "sb");
static_assert(sizeof(flopfs_dirent) == 32, "dirent");

struct packed_file
{
    char fcb[11];
    std::vector<uint8_t> data;
    uint16_t sectors = 0;
    uint32_t lba = 0;
};

bool path_to_fcb(const char *base, char out[11])
{
    char tmp[32];
    size_t n = 0;
    if ((base == nullptr) || (base[0] == '\0') || (base[0] == '.'))
    {
        return false;
    }
    while ((base[n] != '\0') && (n < sizeof(tmp) - 1u))
    {
        tmp[n] = static_cast<char>(std::toupper(static_cast<unsigned char>(base[n])));
        n++;
    }
    tmp[n] = '\0';
    std::memset(out, ' ', 11);
    char *dot = std::strchr(tmp, '.');
    size_t name_len = 0;
    size_t ext_len = 0;
    if (dot != nullptr)
    {
        name_len = static_cast<size_t>(dot - tmp);
        ext_len = std::strlen(dot + 1);
    }
    else
    {
        name_len = std::strlen(tmp);
    }
    if (name_len == 0u)
    {
        return false;
    }
    if (name_len > 8u)
    {
        name_len = 8u;
    }
    if (ext_len > 3u)
    {
        ext_len = 3u;
    }
    std::memcpy(out, tmp, name_len);
    if (dot != nullptr)
    {
        std::memcpy(out + 8, dot + 1, ext_len);
    }
    return true;
}

const char *dir_basename(const char *dir)
{
    const char *slash = std::strrchr(dir, '/');
    if ((slash == nullptr) || (slash[1] == '\0'))
    {
        return dir;
    }
    return slash + 1;
}

bool read_all(const std::string &path, std::vector<uint8_t> *out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return false;
    }
    in.seekg(0, std::ios::end);
    const std::streamoff sz = in.tellg();
    if (sz < 0)
    {
        return false;
    }
    in.seekg(0);
    out->resize(static_cast<size_t>(sz));
    if (sz == 0)
    {
        return true;
    }
    return static_cast<bool>(in.read(reinterpret_cast<char *>(out->data()), sz));
}

} // namespace

bool flopfs_pack_dir(const char *dir, std::vector<uint8_t> *out)
{
    DIR *d = nullptr;
    struct dirent *ent = nullptr;
    std::vector<packed_file> files;
    uint32_t next = k_data_lba;
    flopfs_superblock sb{};
    const char *label = nullptr;
    size_t i = 0;

    if ((dir == nullptr) || (out == nullptr))
    {
        return false;
    }
    d = opendir(dir);
    if (d == nullptr)
    {
        return false;
    }
    while ((ent = readdir(d)) != nullptr)
    {
        packed_file pf{};
        struct stat st{};
        std::string path;
        if ((ent->d_name[0] == '.') || (!path_to_fcb(ent->d_name, pf.fcb)))
        {
            continue;
        }
        path = std::string(dir) + "/" + ent->d_name;
        if (stat(path.c_str(), &st) != 0)
        {
            closedir(d);
            return false;
        }
        if (!S_ISREG(st.st_mode))
        {
            continue;
        }
        if (!read_all(path, &pf.data))
        {
            closedir(d);
            return false;
        }
        pf.sectors = (pf.data.empty())
                         ? 0
                         : static_cast<uint16_t>((pf.data.size() + k_sec - 1u) / k_sec);
        files.push_back(std::move(pf));
    }
    closedir(d);

    std::sort(files.begin(), files.end(), [](const packed_file &a, const packed_file &b) {
        return std::memcmp(a.fcb, b.fcb, 11) < 0;
    });
    if (files.size() > k_root_ents)
    {
        return false;
    }
    for (i = 1; i < files.size(); i++)
    {
        if (std::memcmp(files[i].fcb, files[i - 1u].fcb, 11) == 0)
        {
            return false;
        }
    }
    for (i = 0; i < files.size(); i++)
    {
        if (files[i].sectors == 0)
        {
            files[i].lba = 0;
            continue;
        }
        files[i].lba = next;
        next += files[i].sectors;
    }
    if (next > k_secs)
    {
        return false;
    }

    out->assign(k_bytes, 0);
    (*out)[510] = 0x55;
    (*out)[511] = 0xAA;

    std::memset(&sb, 0, sizeof(sb));
    std::memcpy(sb.magic, "FLOPFS01", 8);
    sb.version_major = 0;
    sb.version_minor = 4;
    sb.sector_size = static_cast<uint16_t>(k_sec);
    sb.sector_count = static_cast<uint16_t>(k_secs);
    sb.flags = 1;
    sb.generation = 4;
    std::memset(sb.label, ' ', sizeof(sb.label));
    label = dir_basename(dir);
    {
        size_t llen = std::strlen(label);
        size_t k = 0;
        if (llen > sizeof(sb.label))
        {
            llen = sizeof(sb.label);
        }
        for (k = 0; k < llen; k++)
        {
            sb.label[k] =
                static_cast<char>(std::toupper(static_cast<unsigned char>(label[k])));
        }
    }
    sb.root_lba = k_root_lba;
    sb.root_sectors = static_cast<uint16_t>(k_root_secs);
    std::memset(sb.init_name, ' ', 11);
    if (!files.empty())
    {
        std::memcpy(sb.init_name, files[0].fcb, 11);
        sb.com_lba = files[0].lba;
        sb.com_sectors = files[0].sectors;
        sb.com_load_seg = 0x2000;
    }

    std::memcpy(out->data() + k_sb_lba * k_sec, &sb, sizeof(sb));
    std::memcpy(out->data() + k_sbm_lba * k_sec, &sb, sizeof(sb));

    for (i = 0; i < files.size(); i++)
    {
        flopfs_dirent de{};
        std::memcpy(de.name, files[i].fcb, 11);
        de.codec = 0;
        de.lba = files[i].lba;
        de.size = static_cast<uint32_t>(files[i].data.size());
        de.sectors = files[i].sectors;
        std::memcpy(out->data() + k_root_lba * k_sec + i * k_dirent, &de, sizeof(de));
        if ((files[i].lba != 0) && (!files[i].data.empty()))
        {
            std::memcpy(out->data() + files[i].lba * k_sec, files[i].data.data(),
                        files[i].data.size());
        }
    }
    return true;
}

} // namespace iron86
