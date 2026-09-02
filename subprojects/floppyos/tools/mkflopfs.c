/**
 * @file mkflopfs.c
 * @brief Pack FlopFS image: SB, stage1.5, kernel, root dir, files (M7).
 */

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    SECTOR_SIZE = 512,
    SECTOR_COUNT_360 = 720,
    SECTOR_COUNT_1440 = 2880,
    IMAGE_SIZE_360 = SECTOR_SIZE * SECTOR_COUNT_360,
    IMAGE_SIZE_1440 = SECTOR_SIZE * SECTOR_COUNT_1440,
    SB_LBA = 1,
    SB_MIRROR_LBA = 2,
    STAGE15_LBA = 3,
    STAGE15_MAX_SECS = 2,
    STAGE15_MAX_BYTES = STAGE15_MAX_SECS * SECTOR_SIZE,
    KERNEL_LBA_DEFAULT = 5,
    KERNEL_MAX_SECS = 64,
    KERNEL_MAX_BYTES = KERNEL_MAX_SECS * SECTOR_SIZE,
    KERNEL_LOAD_SEG_DEFAULT = 0x1000,
    COM_LOAD_SEG_DEFAULT = 0x2000,
    ROOT_ENTRIES = 16,
    DIRENT_SIZE = 32,
    ROOT_BYTES = ROOT_ENTRIES * DIRENT_SIZE,
    FILE_MAX_SECS = 64,
    FILE_MAX_BYTES = FILE_MAX_SECS * SECTOR_SIZE,
    MAX_FILES = 8
};

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

_Static_assert(sizeof(struct flopfs_superblock) == 512, "sb");
_Static_assert(sizeof(struct flopfs_dirent) == 32, "dirent");
_Static_assert(ROOT_BYTES == 512, "root one sector");

struct file_item
{
    char path[256];
    char fcb[11];
    uint8_t *data;
    size_t len;
    uint16_t sectors;
    uint32_t lba;
};

static void usage(const char *argv0)
{
    fprintf(stderr,
            "Usage: %s -i IMAGE -s STAGE15 -k KERNEL [-c COM] [-f FILE]...\n"
            "  -c COM     also added as directory file + init COM cache\n"
            "  -f FILE    pack extra file (basename becomes 8.3)\n"
            "  Refuses /dev/*\n",
            argv0);
}

static uint16_t secs_for(size_t len)
{
    if (len == 0)
    {
        return 0;
    }
    return (uint16_t)((len + SECTOR_SIZE - 1) / SECTOR_SIZE);
}

static int read_file(const char *path, uint8_t *buf, size_t max, size_t *out_len)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
    {
        fprintf(stderr, "mkflopfs: open %s: %s\n", path, strerror(errno));
        return -1;
    }
    size_t n = fread(buf, 1, max, fp);
    int c = fgetc(fp);
    fclose(fp);
    if (c != EOF)
    {
        fprintf(stderr, "mkflopfs: %s too large\n", path);
        return -1;
    }
    if (n == 0)
    {
        fprintf(stderr, "mkflopfs: %s empty\n", path);
        return -1;
    }
    *out_len = n;
    return 0;
}

static int write_at(FILE *fp, long lba, const void *buf, size_t len)
{
    if (fseek(fp, lba * (long)SECTOR_SIZE, SEEK_SET) != 0)
    {
        return -1;
    }
    return fwrite(buf, 1, len, fp) == len ? 0 : -1;
}

/** Convert path basename to 11-byte FCB name (upper, space pad). */
static int path_to_fcb(const char *path, char out[11])
{
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    char tmp[32];
    size_t n = 0;
    while (base[n] != '\0' && n < sizeof tmp - 1)
    {
        tmp[n] = (char)toupper((unsigned char)base[n]);
        n++;
    }
    tmp[n] = '\0';

    memset(out, ' ', 11);
    char *dot = strchr(tmp, '.');
    size_t name_len;
    size_t ext_len = 0;
    if (dot != NULL)
    {
        name_len = (size_t)(dot - tmp);
        ext_len = strlen(dot + 1);
    }
    else
    {
        name_len = strlen(tmp);
    }
    if (name_len == 0 || name_len > 8 || ext_len > 3)
    {
        fprintf(stderr, "mkflopfs: bad 8.3 name: %s\n", path);
        return -1;
    }
    memcpy(out, tmp, name_len);
    if (dot != NULL)
    {
        memcpy(out + 8, dot + 1, ext_len);
    }
    return 0;
}

static void fcb_to_str(const char fcb[11], char *out, size_t out_sz)
{
    char name[9];
    char ext[4];
    memcpy(name, fcb, 8);
    name[8] = '\0';
    for (int i = 7; i >= 0 && name[i] == ' '; --i)
    {
        name[i] = '\0';
    }
    memcpy(ext, fcb + 8, 3);
    ext[3] = '\0';
    for (int i = 2; i >= 0 && ext[i] == ' '; --i)
    {
        ext[i] = '\0';
    }
    if (ext[0] != '\0')
    {
        snprintf(out, out_sz, "%s.%s", name, ext);
    }
    else
    {
        snprintf(out, out_sz, "%s", name);
    }
}

int main(int argc, char **argv)
{
    const char *img_path = NULL;
    const char *stage_path = NULL;
    const char *kernel_path = NULL;
    const char *com_path = NULL;
    const char *label = "FloppyOS";
    const char *file_paths[MAX_FILES];
    int file_count = 0;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0)
        {
            puts("mkflopfs 0.4");
            return 0;
        }
        if (strcmp(argv[i], "-i") == 0 && i + 1 < argc)
        {
            img_path = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
        {
            stage_path = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "-k") == 0 && i + 1 < argc)
        {
            kernel_path = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc)
        {
            com_path = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "-f") == 0 && i + 1 < argc)
        {
            if (file_count >= MAX_FILES)
            {
                fprintf(stderr, "mkflopfs: too many -f files\n");
                return 2;
            }
            file_paths[file_count++] = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "-l") == 0 && i + 1 < argc)
        {
            label = argv[++i];
            continue;
        }
        fprintf(stderr, "mkflopfs: unknown %s\n", argv[i]);
        usage(argv[0]);
        return 2;
    }

    if (img_path == NULL || stage_path == NULL || kernel_path == NULL)
    {
        usage(argv[0]);
        return 2;
    }
    if (strncmp(img_path, "/dev/", 5) == 0)
    {
        fprintf(stderr, "mkflopfs: refusing device\n");
        return 1;
    }

    /* Build file list: -c first as HELLO.COM style, then -f */
    struct file_item files[MAX_FILES + 1];
    int nfiles = 0;
    memset(files, 0, sizeof files);

    if (com_path != NULL)
    {
        snprintf(files[nfiles].path, sizeof files[nfiles].path, "%s", com_path);
        if (path_to_fcb(com_path, files[nfiles].fcb) != 0)
        {
            return 1;
        }
        nfiles++;
    }
    for (int i = 0; i < file_count; ++i)
    {
        snprintf(files[nfiles].path, sizeof files[nfiles].path, "%s", file_paths[i]);
        if (path_to_fcb(file_paths[i], files[nfiles].fcb) != 0)
        {
            return 1;
        }
        nfiles++;
    }

    FILE *fp = fopen(img_path, "r+b");
    if (fp == NULL)
    {
        fprintf(stderr, "mkflopfs: %s: %s\n", img_path, strerror(errno));
        return 1;
    }
    if (fseek(fp, 0, SEEK_END) != 0)
    {
        fclose(fp);
        return 1;
    }
    long img_sz = ftell(fp);
    int sector_count;
    if (img_sz == IMAGE_SIZE_360)
    {
        sector_count = SECTOR_COUNT_360;
    }
    else if (img_sz == IMAGE_SIZE_1440)
    {
        sector_count = SECTOR_COUNT_1440;
    }
    else
    {
        fprintf(stderr,
                "mkflopfs: bad image size %ld (want %d or %d)\n",
                img_sz,
                IMAGE_SIZE_360,
                IMAGE_SIZE_1440);
        fclose(fp);
        return 1;
    }

    uint8_t stage[STAGE15_MAX_BYTES];
    memset(stage, 0, sizeof stage);
    size_t stage_len = 0;
    if (read_file(stage_path, stage, sizeof stage, &stage_len) != 0)
    {
        fclose(fp);
        return 1;
    }
    uint16_t stage_secs = secs_for(stage_len);

    uint8_t *kernel = calloc(1, KERNEL_MAX_BYTES);
    if (kernel == NULL)
    {
        fclose(fp);
        return 1;
    }
    size_t kernel_len = 0;
    if (read_file(kernel_path, kernel, KERNEL_MAX_BYTES, &kernel_len) != 0)
    {
        free(kernel);
        fclose(fp);
        return 1;
    }
    uint16_t kernel_secs = secs_for(kernel_len);
    uint32_t kernel_lba = KERNEL_LBA_DEFAULT;
    uint32_t next = kernel_lba + kernel_secs;

    for (int i = 0; i < nfiles; ++i)
    {
        files[i].data = calloc(1, FILE_MAX_BYTES);
        if (files[i].data == NULL)
        {
            fprintf(stderr, "mkflopfs: OOM\n");
            free(kernel);
            fclose(fp);
            return 1;
        }
        if (read_file(files[i].path, files[i].data, FILE_MAX_BYTES, &files[i].len) != 0)
        {
            free(kernel);
            fclose(fp);
            return 1;
        }
        files[i].sectors = secs_for(files[i].len);
        files[i].lba = next;
        next += files[i].sectors;
    }

    uint32_t root_lba = next;
    next += 1; /* one root sector */
    if (next > (uint32_t)sector_count)
    {
        fprintf(stderr, "mkflopfs: image full\n");
        free(kernel);
        fclose(fp);
        return 1;
    }

    /* Root directory */
    uint8_t root[ROOT_BYTES];
    memset(root, 0, sizeof root);
    for (int i = 0; i < nfiles && i < ROOT_ENTRIES; ++i)
    {
        struct flopfs_dirent *de = (struct flopfs_dirent *)(root + i * DIRENT_SIZE);
        memcpy(de->name, files[i].fcb, 11);
        de->codec = 0;
        de->lba = files[i].lba;
        de->size = (uint32_t)files[i].len;
        de->sectors = files[i].sectors;
    }

    struct flopfs_superblock sb;
    memset(&sb, 0, sizeof sb);
    memcpy(sb.magic, "FLOPFS01", 8);
    sb.version_major = 0;
    sb.version_minor = 4;
    sb.sector_size = SECTOR_SIZE;
    sb.sector_count = (uint16_t)sector_count;
    sb.stage15_lba = STAGE15_LBA;
    sb.stage15_sectors = stage_secs;
    sb.flags = 1;
    sb.generation = 4;
    memset(sb.label, ' ', sizeof sb.label);
    size_t llen = strlen(label);
    if (llen > sizeof sb.label)
    {
        llen = sizeof sb.label;
    }
    memcpy(sb.label, label, llen);
    sb.kernel_lba = kernel_lba;
    sb.kernel_sectors = kernel_secs;
    sb.kernel_load_seg = KERNEL_LOAD_SEG_DEFAULT;
    sb.kernel_codec = 0;
    sb.com_load_seg = COM_LOAD_SEG_DEFAULT;
    sb.root_lba = root_lba;
    sb.root_sectors = 1;

    if (nfiles > 0)
    {
        sb.com_lba = files[0].lba;
        sb.com_sectors = files[0].sectors;
        memcpy(sb.init_name, files[0].fcb, 11);
    }
    else
    {
        memcpy(sb.init_name, "HELLO   COM", 11);
    }

    if (write_at(fp, SB_LBA, &sb, sizeof sb) != 0 ||
        write_at(fp, SB_MIRROR_LBA, &sb, sizeof sb) != 0 ||
        write_at(fp, STAGE15_LBA, stage, STAGE15_MAX_BYTES) != 0 ||
        write_at(fp, (long)kernel_lba, kernel, (size_t)kernel_secs * SECTOR_SIZE) != 0)
    {
        fprintf(stderr, "mkflopfs: core write failed\n");
        free(kernel);
        fclose(fp);
        return 1;
    }

    for (int i = 0; i < nfiles; ++i)
    {
        size_t pad = (size_t)files[i].sectors * SECTOR_SIZE;
        if (write_at(fp, (long)files[i].lba, files[i].data, pad) != 0)
        {
            fprintf(stderr, "mkflopfs: file write failed\n");
            free(kernel);
            fclose(fp);
            return 1;
        }
        free(files[i].data);
        files[i].data = NULL;
    }

    if (write_at(fp, (long)root_lba, root, sizeof root) != 0)
    {
        fprintf(stderr, "mkflopfs: root write failed\n");
        free(kernel);
        fclose(fp);
        return 1;
    }

    free(kernel);
    if (fclose(fp) != 0)
    {
        return 1;
    }

    char init_str[16];
    fcb_to_str(sb.init_name, init_str, sizeof init_str);
    printf("mkflopfs: %s — kernel LBA %u (%u), root LBA %u, %d files, init=%s\n",
           img_path, kernel_lba, kernel_secs, root_lba, nfiles, init_str);
    for (int i = 0; i < nfiles; ++i)
    {
        char nm[16];
        fcb_to_str(files[i].fcb, nm, sizeof nm);
        printf("  %s  LBA %u  %zu bytes\n", nm, files[i].lba, files[i].len);
    }
    return 0;
}
