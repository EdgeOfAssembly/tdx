/**
 * @file mkimg1440.c
 * @brief Build a raw 1.44 MB floppy image with an optional boot sector.
 *
 * Creates exactly 1_474_560 bytes (2880 × 512). Never touches block devices;
 * use write-floppy.sh only after human confirmation for /dev/sdc.
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    SECTOR_SIZE = 512,
    SECTOR_COUNT = 2880,
    IMAGE_SIZE = SECTOR_SIZE * SECTOR_COUNT /* 1474560 */
};

static void usage(const char *argv0)
{
    fprintf(stderr,
            "Usage: %s -o IMAGE.img [-b BOOT.BIN]\n"
            "  Create a 1.44 MB (1474560-byte) raw floppy image.\n"
            "  -b  optional 512-byte boot sector (must end with 0x55 0xAA)\n"
            "  Does not write to /dev/* — image file only.\n",
            argv0);
}

static int read_boot_sector(const char *path, uint8_t *buf)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
    {
        fprintf(stderr, "mkimg1440: open %s: %s\n", path, strerror(errno));
        return -1;
    }

    size_t n = fread(buf, 1, SECTOR_SIZE, fp);
    if (n != (size_t)SECTOR_SIZE)
    {
        fprintf(stderr, "mkimg1440: %s: need exactly %d bytes, got %zu\n",
                path, SECTOR_SIZE, n);
        fclose(fp);
        return -1;
    }

    int c = fgetc(fp);
    if (c != EOF)
    {
        fprintf(stderr, "mkimg1440: %s: larger than %d bytes\n", path, SECTOR_SIZE);
        fclose(fp);
        return -1;
    }
    fclose(fp);

    if (buf[510] != 0x55 || buf[511] != 0xAA)
    {
        fprintf(stderr, "mkimg1440: %s: missing 0x55AA boot signature\n", path);
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *out_path = NULL;
    const char *boot_path = NULL;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0)
        {
            puts("mkimg1440 0.1");
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
        fprintf(stderr, "mkimg1440: unknown argument: %s\n", argv[i]);
        usage(argv[0]);
        return 2;
    }

    if (out_path == NULL)
    {
        usage(argv[0]);
        return 2;
    }

    /* Refuse accidental device paths */
    if (strncmp(out_path, "/dev/", 5) == 0)
    {
        fprintf(stderr,
                "mkimg1440: refusing to write block device %s\n"
                "  Use tools/write-floppy.sh only after human confirmation.\n",
                out_path);
        return 1;
    }

    uint8_t *image = calloc(1, (size_t)IMAGE_SIZE);
    if (image == NULL)
    {
        fprintf(stderr, "mkimg1440: out of memory\n");
        return 1;
    }

    if (boot_path != NULL)
    {
        if (read_boot_sector(boot_path, image) != 0)
        {
            free(image);
            return 1;
        }
    }
    else
    {
        /* Minimal non-bootable placeholder signature for empty images */
        image[510] = 0x55;
        image[511] = 0xAA;
    }

    FILE *out = fopen(out_path, "wb");
    if (out == NULL)
    {
        fprintf(stderr, "mkimg1440: open %s: %s\n", out_path, strerror(errno));
        free(image);
        return 1;
    }

    size_t written = fwrite(image, 1, (size_t)IMAGE_SIZE, out);
    if (fclose(out) != 0 || written != (size_t)IMAGE_SIZE)
    {
        fprintf(stderr, "mkimg1440: write %s failed\n", out_path);
        free(image);
        return 1;
    }

    free(image);
    printf("mkimg1440: wrote %s (%d bytes)%s\n",
           out_path,
           IMAGE_SIZE,
           boot_path != NULL ? " with boot sector" : "");
    return 0;
}
