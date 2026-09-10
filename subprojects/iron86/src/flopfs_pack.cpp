/**
 * @file flopfs_pack.cpp
 * @brief Pack a host directory into a 360K or 720K FlopFS data disk (B:).
 */
#include "iron86/flopfs_pack.h"
#include "iron86/hdc.h"

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
constexpr uint32_t k_dirent = 32;
constexpr uint32_t k_sb_lba = 1;
constexpr uint32_t k_sbm_lba = 2;
constexpr uint32_t k_root_lba = 3;

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

/**
 * @brief True if BASE is already a DOS 8.3 name (one optional dot).
 *
 * Host junk (dosbox-x.conf, Unicode names) is skipped rather than truncated
 * into a colliding FCB.
 */
bool is_dos_83(const char *base)
{
    size_t name_len = 0;
    size_t ext_len = 0;
    bool dot = false;
    size_t i = 0;

    if ((base == nullptr) || (base[0] == '\0') || (base[0] == '.'))
    {
        return false;
    }
    for (i = 0; base[i] != '\0'; i++)
    {
        const unsigned char c = static_cast<unsigned char>(base[i]);
        const char u = static_cast<char>(std::toupper(c));
        const bool ok = ((u >= 'A') && (u <= 'Z')) || ((u >= '0') && (u <= '9')) ||
                        (u == '$') || (u == '%') || (u == '\'') || (u == '-') ||
                        (u == '_') || (u == '@') || (u == '~') || (u == '`') ||
                        (u == '!') || (u == '(') || (u == ')') || (u == '{') ||
                        (u == '}') || (u == '^') || (u == '#') || (u == '&');
        if (base[i] == '.')
        {
            if (dot)
            {
                return false;
            }
            dot = true;
            continue;
        }
        if (!ok)
        {
            return false;
        }
        if (!dot)
        {
            name_len++;
            if (name_len > 8u)
            {
                return false;
            }
        }
        else
        {
            ext_len++;
            if (ext_len > 3u)
            {
                return false;
            }
        }
    }
    return name_len >= 1u;
}

bool path_to_fcb(const char *base, char out[11])
{
    char tmp[32];
    size_t n = 0;
    if (!is_dos_83(base))
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

bool collect_tree(const std::string &dir, std::vector<packed_file> *files)
{
    DIR *d = opendir(dir.c_str());
    struct dirent *ent = nullptr;
    if (d == nullptr)
    {
        return false;
    }
    while ((ent = readdir(d)) != nullptr)
    {
        packed_file pf{};
        struct stat st{};
        std::string path;
        if ((ent->d_name[0] == '.') &&
            ((ent->d_name[1] == '\0') || ((ent->d_name[1] == '.') && (ent->d_name[2] == '\0'))))
        {
            continue;
        }
        path = dir + "/" + ent->d_name;
        if (stat(path.c_str(), &st) != 0)
        {
            closedir(d);
            return false;
        }
        if (S_ISDIR(st.st_mode))
        {
            if (!collect_tree(path, files))
            {
                closedir(d);
                return false;
            }
            continue;
        }
        if (!S_ISREG(st.st_mode) || (ent->d_name[0] == '.') || (!is_dos_83(ent->d_name)) ||
            (!path_to_fcb(ent->d_name, pf.fcb)))
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
        files->push_back(std::move(pf));
    }
    closedir(d);
    return true;
}

} // namespace

bool flopfs_pack_dir(const char *dir, std::vector<uint8_t> *out)
{
    std::vector<packed_file> files;
    uint32_t next = 0;
    uint32_t root_secs = 2;
    uint32_t data_lba = 0;
    flopfs_superblock sb{};
    const char *label = nullptr;
    size_t i = 0;
    static const uint32_t k_sizes[] = {FLOPFS_SECS_360K, FLOPFS_SECS_720K, FLOPFS_SECS_1200K,
                                       FLOPFS_SECS_1440K};

    if ((dir == nullptr) || (out == nullptr))
    {
        return false;
    }
    if (!collect_tree(dir, &files))
    {
        return false;
    }

    std::sort(files.begin(), files.end(), [](const packed_file &a, const packed_file &b) {
        return std::memcmp(a.fcb, b.fcb, 11) < 0;
    });
    for (i = 1; i < files.size(); i++)
    {
        if (std::memcmp(files[i].fcb, files[i - 1u].fcb, 11) == 0)
        {
            return false;
        }
    }
    root_secs = static_cast<uint32_t>((files.size() * k_dirent + k_sec - 1u) / k_sec);
    if (root_secs < 2u)
    {
        root_secs = 2;
    }
    data_lba = k_root_lba + root_secs;
    next = data_lba;
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
    uint32_t secs = 0;
    for (i = 0; i < (sizeof(k_sizes) / sizeof(k_sizes[0])); i++)
    {
        if (next <= k_sizes[i])
        {
            secs = k_sizes[i];
            break;
        }
    }
    if (secs == 0)
    {
        return false;
    }

    out->assign(secs * k_sec, 0);
    (*out)[510] = 0x55;
    (*out)[511] = 0xAA;

    std::memset(&sb, 0, sizeof(sb));
    std::memcpy(sb.magic, "FLOPFS01", 8);
    sb.version_major = 0;
    sb.version_minor = 4;
    sb.sector_size = static_cast<uint16_t>(k_sec);
    sb.sector_count = static_cast<uint16_t>(secs);
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
    sb.root_sectors = static_cast<uint16_t>(root_secs);
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

static void fat12_put(uint8_t *fat, uint16_t cl, uint16_t v)
{
    const uint32_t o = static_cast<uint32_t>(cl) + (static_cast<uint32_t>(cl) / 2u);
    if ((cl & 1u) != 0)
    {
        fat[o] = static_cast<uint8_t>((fat[o] & 0x0Fu) | static_cast<uint8_t>((v << 4) & 0xF0u));
        fat[o + 1u] = static_cast<uint8_t>(v >> 4);
    }
    else
    {
        fat[o] = static_cast<uint8_t>(v);
        fat[o + 1u] = static_cast<uint8_t>((fat[o + 1u] & 0xF0u) | static_cast<uint8_t>((v >> 8) & 0x0Fu));
    }
}

bool hdc_pack_dir(const char *dir, uint32_t cylinders, uint32_t heads, uint32_t spt,
                  std::vector<uint8_t> *out)
{
    std::vector<packed_file> files;
    const uint32_t cyl = (cylinders != 0) ? cylinders : k_hdc_cyl_xt;
    const uint32_t hd = (heads != 0) ? heads : k_hdc_heads_xt;
    const uint32_t sp = (spt != 0) ? spt : k_hdc_spt_xt;
    const uint32_t total = cyl * hd * sp;
    const uint32_t hidden = sp;
    const uint32_t part = (total > hidden) ? (total - hidden) : 0;
    const uint32_t spc = 8;
    const uint32_t root_ents = 512;
    const uint32_t root_secs = (root_ents * 32u) / k_sec;
    uint32_t spf = 8;
    uint32_t data_secs = 0;
    uint32_t clusters = 0;
    uint32_t i = 0;
    uint16_t cl = 2;
    uint8_t *fat = nullptr;
    uint8_t *root = nullptr;

    if ((dir == nullptr) || (out == nullptr) || (part < 64u))
    {
        return false;
    }
    if (!collect_tree(dir, &files))
    {
        return false;
    }
    std::sort(files.begin(), files.end(), [](const packed_file &a, const packed_file &b) {
        return std::memcmp(a.fcb, b.fcb, 11) < 0;
    });
    for (i = 1; i < files.size(); i++)
    {
        if (std::memcmp(files[i].fcb, files[i - 1u].fcb, 11) == 0)
        {
            return false;
        }
    }
    if (files.size() > root_ents)
    {
        return false;
    }
    for (;;)
    {
        data_secs = (part > (1u + 2u * spf + root_secs)) ? (part - 1u - 2u * spf - root_secs) : 0;
        clusters = data_secs / spc;
        if ((spf * k_sec) >= (((clusters + 2u) * 3u + 1u) / 2u))
        {
            break;
        }
        spf++;
        if (spf > 64u)
        {
            return false;
        }
    }
    out->assign(total * k_sec, 0);
    (*out)[0x1BE] = 0x80;
    (*out)[0x1C2] = 0x01;
    {
        const uint32_t lba = hidden;
        (*out)[0x1C6] = static_cast<uint8_t>(lba);
        (*out)[0x1C7] = static_cast<uint8_t>(lba >> 8);
        (*out)[0x1C8] = static_cast<uint8_t>(lba >> 16);
        (*out)[0x1C9] = static_cast<uint8_t>(lba >> 24);
        (*out)[0x1CA] = static_cast<uint8_t>(part);
        (*out)[0x1CB] = static_cast<uint8_t>(part >> 8);
        (*out)[0x1CC] = static_cast<uint8_t>(part >> 16);
        (*out)[0x1CD] = static_cast<uint8_t>(part >> 24);
    }
    (*out)[510] = 0x55;
    (*out)[511] = 0xAA;
    {
        uint8_t *bs = out->data() + hidden * k_sec;
        std::memcpy(bs + 3, "IBM  3.3", 8);
        bs[11] = 0x00;
        bs[12] = 0x02;
        bs[13] = static_cast<uint8_t>(spc);
        bs[14] = 0x01;
        bs[15] = 0x00;
        bs[16] = 0x02;
        bs[17] = static_cast<uint8_t>(root_ents);
        bs[18] = static_cast<uint8_t>(root_ents >> 8);
        if (part < 65536u)
        {
            bs[19] = static_cast<uint8_t>(part);
            bs[20] = static_cast<uint8_t>(part >> 8);
        }
        bs[21] = 0xF8;
        bs[22] = static_cast<uint8_t>(spf);
        bs[23] = static_cast<uint8_t>(spf >> 8);
        bs[24] = static_cast<uint8_t>(sp);
        bs[25] = static_cast<uint8_t>(sp >> 8);
        bs[26] = static_cast<uint8_t>(hd);
        bs[27] = static_cast<uint8_t>(hd >> 8);
        bs[28] = static_cast<uint8_t>(hidden);
        bs[29] = static_cast<uint8_t>(hidden >> 8);
        bs[30] = static_cast<uint8_t>(hidden >> 16);
        bs[31] = static_cast<uint8_t>(hidden >> 24);
        if (part >= 65536u)
        {
            bs[32] = static_cast<uint8_t>(part);
            bs[33] = static_cast<uint8_t>(part >> 8);
            bs[34] = static_cast<uint8_t>(part >> 16);
            bs[35] = static_cast<uint8_t>(part >> 24);
        }
        bs[510] = 0x55;
        bs[511] = 0xAA;
        fat = bs + k_sec;
        fat12_put(fat, 0, 0xFF8);
        fat12_put(fat, 1, 0xFFF);
        root = fat + 2u * spf * k_sec;
    }
    for (i = 0; i < files.size(); i++)
    {
        uint32_t left = static_cast<uint32_t>(files[i].data.size());
        uint32_t off = 0;
        uint16_t first = 0;
        uint16_t prev = 0;
        if (left == 0)
        {
            first = 0;
        }
        else
        {
            while (left > 0u)
            {
                const uint32_t n = (left > spc * k_sec) ? (spc * k_sec) : left;
                const uint32_t data0 = hidden + 1u + 2u * spf + root_secs;
                const uint32_t lba = data0 + static_cast<uint32_t>(cl - 2u) * spc;
                if ((lba * k_sec + n) > out->size())
                {
                    return false;
                }
                std::memcpy(out->data() + lba * k_sec, files[i].data.data() + off, n);
                if (first == 0)
                {
                    first = cl;
                }
                if (prev != 0)
                {
                    fat12_put(fat, prev, cl);
                    std::memcpy(fat + spf * k_sec, fat, spf * k_sec);
                }
                prev = cl;
                cl = static_cast<uint16_t>(cl + 1u);
                if (cl >= (clusters + 2u))
                {
                    return false;
                }
                off += n;
                left -= n;
            }
            fat12_put(fat, prev, 0xFFF);
            std::memcpy(fat + spf * k_sec, fat, spf * k_sec);
        }
        {
            uint8_t *de = root + i * 32u;
            std::memcpy(de, files[i].fcb, 11);
            de[11] = 0x20;
            de[26] = static_cast<uint8_t>(first);
            de[27] = static_cast<uint8_t>(first >> 8);
            de[28] = static_cast<uint8_t>(files[i].data.size());
            de[29] = static_cast<uint8_t>(files[i].data.size() >> 8);
            de[30] = static_cast<uint8_t>(files[i].data.size() >> 16);
            de[31] = static_cast<uint8_t>(files[i].data.size() >> 24);
        }
    }
    return true;
}

} // namespace iron86
