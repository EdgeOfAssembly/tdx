/**
 * @file mkimg.c
 * @brief Create raw floppy images: 360K (5.25") or 1440K (3.5").
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    SECTOR_SIZE = 512
};

struct geometry
{
    const char *name;
    int sectors; /* total */
    int spt;
    int heads;
    int cyls;
    uint8_t media;
};

static const struct geometry GEO_360 = {
    .name = "360K",
    .sectors = 720, /* 40*2*9 */
    .spt = 9,
    .heads = 2,
    .cyls = 40,
    .media = 0xFD,
};

static const struct geometry GEO_1440 = {
    .name = "1440K",
    .sectors = 2880, /* 80*2*18 */
    .spt = 18,
    .heads = 2,
    .cyls = 80,
    .media = 0xF0,
};

static void usage(const char *argv0)
{
    fprintf(stderr,
            "Usage: %s -o IMAGE.img [-b BOOT.BIN] [-s 360|1440]\n"
            "  Default -s 360 (5.25\" DD, Py86/5150 friendly).\n"
            "  Refuses /dev/* paths.\n",
            argv0);
}

static int read_boot(const char *path, uint8_t *buf)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
    {
        fprintf(stderr, "mkimg: open %s: %s\n", path, strerror(errno));
        return -1;
    }
    size_t n = fread(buf, 1, SECTOR_SIZE, fp);
    int c = fgetc(fp);
    fclose(fp);
    if (n != (size_t)SECTOR_SIZE || c != EOF)
    {
        fprintf(stderr, "mkimg: boot must be exactly %d bytes\n", SECTOR_SIZE);
        return -1;
    }
    if (buf[510] != 0x55 || buf[511] != 0xAA)
    {
        fprintf(stderr, "mkimg: missing 0x55AA signature\n");
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *out_path = NULL;
    const char *boot_path = NULL;
    const struct geometry *geo = &GEO_360;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0)
        {
            puts("mkimg 0.2");
            return 0;
        }
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
        {
            out_path = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "-b") == 0 && i + 1 < argc)
        {
            boot_path = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
        {
            const char *s = argv[++i];
            if (strcmp(s, "360") == 0)
            {
                geo = &GEO_360;
            }
            else if (strcmp(s, "1440") == 0)
            {
                geo = &GEO_1440;
            }
            else
            {
                fprintf(stderr, "mkimg: size must be 360 or 1440\n");
                return 2;
            }
            continue;
        }
        fprintf(stderr, "mkimg: unknown %s\n", argv[i]);
        usage(argv[0]);
        return 2;
    }

    if (out_path == NULL)
    {
        usage(argv[0]);
        return 2;
    }
    if (strncmp(out_path, "/dev/", 5) == 0)
    {
        fprintf(stderr, "mkimg: refusing device path\n");
        return 1;
    }

    const size_t image_size = (size_t)geo->sectors * SECTOR_SIZE;
    uint8_t *image = calloc(1, image_size);
    if (image == NULL)
    {
        return 1;
    }

    if (boot_path != NULL)
    {
        if (read_boot(boot_path, image) != 0)
        {
            free(image);
            return 1;
        }
    }
    else
    {
        image[510] = 0x55;
        image[511] = 0xAA;
    }

    FILE *out = fopen(out_path, "wb");
    if (out == NULL)
    {
        fprintf(stderr, "mkimg: open %s: %s\n", out_path, strerror(errno));
        free(image);
        return 1;
    }
    size_t w = fwrite(image, 1, image_size, out);
    if (fclose(out) != 0 || w != image_size)
    {
        free(image);
        return 1;
    }
    free(image);
    printf("mkimg: wrote %s (%s, %zu bytes, %d sectors, %d/%d/%d CHS)\n",
           out_path,
           geo->name,
           image_size,
           geo->sectors,
           geo->cyls,
           geo->heads,
           geo->spt);
    return 0;
}
