/**
 * @file test_pack.cpp
 * @brief 360K/720K FlopFS directory packer (no kernel on the image).
 */
#include "iron86/flopfs_pack.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

static int g_fail = 0;

static void expect(bool ok, const char *msg)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL %s\n", msg);
        g_fail = 1;
    }
}

static std::string make_temp_dir()
{
    char tmpl[] = "/tmp/iron86-pack-XXXXXX";
    if (mkdtemp(tmpl) == nullptr)
    {
        return {};
    }
    return std::string(tmpl);
}

static bool write_file(const std::filesystem::path &p, const void *data, size_t n)
{
    std::ofstream o(p, std::ios::binary);
    if (!o)
    {
        return false;
    }
    if ((n > 0) && (data != nullptr))
    {
        o.write(static_cast<const char *>(data), static_cast<std::streamsize>(n));
    }
    return static_cast<bool>(o);
}

int main()
{
    {
        const std::string dir = make_temp_dir();
        expect(!dir.empty(), "mkdtemp hello");
        if (!dir.empty())
        {
            std::vector<uint8_t> body(192, 0x90);
            std::vector<uint8_t> img;
            uint16_t sc = 0;
            uint32_t root = 0;
            uint16_t rsec = 0;
            expect(write_file(std::filesystem::path(dir) / "HELLO.COM", body.data(), body.size()),
                   "write HELLO.COM");
            expect(write_file(std::filesystem::path(dir) / ".hidden", "x", 1), "write hidden");
            std::filesystem::create_directory(std::filesystem::path(dir) / "SUB");
            expect(write_file(std::filesystem::path(dir) / "SUB" / "NOPE.COM", "n", 1), "write nested");
            expect(iron86::flopfs_pack_dir(dir.c_str(), &img), "pack hello");
            expect(img.size() == 368640u, "pack 360k size");
            if (img.size() == 368640u)
            {
                expect(img[510] == 0x55 && img[511] == 0xAA, "pack 55AA");
                expect(std::memcmp(img.data() + 512, "FLOPFS01", 8) == 0, "pack magic LBA1");
                std::memcpy(&sc, img.data() + 512 + 14, 2);
                std::memcpy(&root, img.data() + 512 + 66, 4);
                std::memcpy(&rsec, img.data() + 512 + 70, 2);
                expect(sc == 720, "sector_count 720");
                expect(root == 3, "root_lba 3");
                expect(rsec == 2, "root_sectors 2");
                expect(std::memcmp(img.data() + 3 * 512, "HELLO   COM", 11) == 0, "dirent 8.3");
                expect(img[5 * 512] == 0x90, "payload LBA5");
                expect(img[5 * 512 + 191] == 0x90, "payload 192");
            }
            std::filesystem::remove_all(dir);
        }
    }
    {
        const std::string dir = make_temp_dir();
        expect(!dir.empty(), "mkdtemp 72k");
        if (!dir.empty())
        {
            std::vector<uint8_t> body(72u * 1024u, 0xCC);
            std::vector<uint8_t> img;
            uint32_t size = 0;
            uint16_t secs = 0;
            expect(write_file(std::filesystem::path(dir) / "BUSHIDO.EXE", body.data(), body.size()),
                   "write 72k");
            expect(iron86::flopfs_pack_dir(dir.c_str(), &img), "pack 72k");
            expect(img.size() == 368640u, "pack 72k size");
            if (img.size() == 368640u)
            {
                std::memcpy(&size, img.data() + 3 * 512 + 16, 4);
                std::memcpy(&secs, img.data() + 3 * 512 + 20, 2);
                expect(size == 72u * 1024u, "no 32k cap size");
                expect(secs == 144, "72k sectors");
                expect(img[5 * 512] == 0xCC, "72k payload");
            }
            std::filesystem::remove_all(dir);
        }
    }
    {
        const std::string dir = make_temp_dir();
        expect(!dir.empty(), "mkdtemp 33");
        if (!dir.empty())
        {
            int i = 0;
            std::vector<uint8_t> img;
            for (i = 0; i < 33; i++)
            {
                char name[16];
                std::snprintf(name, sizeof(name), "F%02d.COM", i);
                expect(write_file(std::filesystem::path(dir) / name, "x", 1), "write 33");
            }
            expect(!iron86::flopfs_pack_dir(dir.c_str(), &img), "pack 33 fails");
            std::filesystem::remove_all(dir);
        }
    }
    {
        const std::string dir = make_temp_dir();
        expect(!dir.empty(), "mkdtemp 720k");
        if (!dir.empty())
        {
            std::vector<uint8_t> body(400u * 1024u, 0x5A);
            std::vector<uint8_t> img;
            uint16_t sc = 0;
            expect(write_file(std::filesystem::path(dir) / "DATA1", body.data(), body.size()),
                   "write 400k");
            expect(write_file(std::filesystem::path(dir) / "dosbox-x.conf", "x", 1),
                   "write host conf");
            expect(iron86::flopfs_pack_dir(dir.c_str(), &img), "pack 400k → 720k");
            expect(img.size() == iron86::FLOPFS_BYTES_720K, "pack 720k size");
            if (img.size() == iron86::FLOPFS_BYTES_720K)
            {
                std::memcpy(&sc, img.data() + 512 + 14, 2);
                expect(sc == iron86::FLOPFS_SECS_720K, "sector_count 1440");
                expect(std::memcmp(img.data() + 3 * 512, "DATA1      ", 11) == 0, "only DATA1");
                expect(img[5 * 512] == 0x5A, "400k payload");
            }
            std::filesystem::remove_all(dir);
        }
    }
    {
        const std::string dir = make_temp_dir();
        expect(!dir.empty(), "mkdtemp overflow");
        if (!dir.empty())
        {
            std::vector<uint8_t> body(800u * 1024u, 0x11);
            std::vector<uint8_t> img;
            expect(write_file(std::filesystem::path(dir) / "HUGE.DAT", body.data(), body.size()),
                   "write 800k");
            expect(!iron86::flopfs_pack_dir(dir.c_str(), &img), "pack 800k fails");
            std::filesystem::remove_all(dir);
        }
    }
    {
        std::vector<uint8_t> img;
        expect(!iron86::flopfs_pack_dir("/no/such/iron86-pack-dir", &img), "missing dir");
        expect(!iron86::flopfs_pack_dir(nullptr, &img), "null dir");
        expect(!iron86::flopfs_pack_dir("/tmp", nullptr), "null out");
    }

    if (g_fail != 0)
    {
        return 1;
    }
    std::puts("iron86 pack tests ok");
    return 0;
}
