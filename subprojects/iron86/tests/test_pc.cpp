/**
 * @file test_pc.cpp
 * @brief Packed 5150 chipset: PIC IMR, PIT countdown, DMA wrap, PPI DIP.
 */
#include "iron86/hw.h"
#include "iron86/pc.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
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

static void expect_eq(uint16_t got, uint16_t want, const char *msg)
{
    if (got != want)
    {
        std::fprintf(stderr, "FAIL %s got=%04X want=%04X\n", msg, got, want);
        g_fail = 1;
    }
}

static void run(iron86::cpu &c, int max_steps)
{
    int n = 0;
    while ((!c.halted()) && (n < max_steps))
    {
        if (!c.step())
        {
            break;
        }
        n++;
    }
}

int main()
{
    expect(sizeof(iron86::ppi8255) == 10u, "sizeof ppi8255");
    expect(sizeof(iron86::fdc765) == 50u, "sizeof fdc765");
    expect(sizeof(iron86::pc_hw) == 181u, "sizeof pc_hw");

    {
        /* PIC ICW then IMR 00 / FF readback (PCBIOS TST6). */
        iron86::pc p;
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0xB0, 0x13, 0xE6, 0x20, /* MOV AL,13h; OUT 20h,AL  ICW1 */
            0xB0, 0x08, 0xE6, 0x21, /* ICW2 */
            0xB0, 0x09, 0xE6, 0x21, /* ICW4 */
            0xB0, 0x00, 0xE6, 0x21, /* IMR=0 */
            0xE4, 0x21,             /* IN AL,21h */
            0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 32);
        expect_eq(static_cast<uint16_t>(p.c.ax() & 0xFFu), 0, "pic imr 0");
        expect(p.c.halted(), "pic imr0 hlt");
    }
    {
        iron86::pc p;
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0xB0, 0x13, 0xE6, 0x20, 0xB0, 0x08, 0xE6, 0x21, 0xB0, 0x09, 0xE6, 0x21,
            0xB0, 0xFF, 0xE6, 0x21, 0xE4, 0x21, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 32);
        expect_eq(static_cast<uint16_t>(p.c.ax() & 0xFFu), 0x00FF, "pic imr ff");
    }
    {
        /* DMA wrap: write FFFF to ch0 addr, read back (PCBIOS C17). */
        iron86::pc p;
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0xB0, 0xFF, 0xBA, 0x00, 0x00, 0xEE, 0xEE, 0xB8, 0x01, 0x01, 0xEC,
            0x88, 0xC4, 0xEC, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 32);
        expect_eq(p.c.ax(), 0xFFFF, "dma wrap ff");
    }
    {
        /* PPI: OUT 61h,0FCh (PB7=1 switches); IN 60h → DIP 0x2D CGA 80×25. */
        iron86::pc p;
        p.wire_pc_hw();
        const uint8_t prog[] = {0xB0, 0xFC, 0xE6, 0x61, 0xE4, 0x60, 0xF4};
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 16);
        expect_eq(static_cast<uint16_t>(p.c.ax() & 0xFFu), 0x2D, "ppi dip 2d");
    }
    {
        /* PIT ch1 mode 2 LSB count 0: after ticks, latch LSB is not stuck at 00. */
        iron86::pc p;
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0xB0, 0x54, 0xE6, 0x43, 0xB0, 0x00, 0xE6, 0x41,
            0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
            0xB0, 0x40, 0xE6, 0x43, 0xE4, 0x41, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 64);
        expect((p.c.ax() & 0xFFu) != 0, "pit ch1 counted");
    }
    {
        /* PyFloppy: DOR nRESET rising edge, Sense Int ST0=C0. */
        iron86::pc p;
        uint8_t img[512];
        std::memset(img, 0, sizeof(img));
        img[510] = 0x55;
        img[511] = 0xAA;
        expect(p.attach_floppy(img, sizeof(img)), "fdc attach");
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0xBA, 0xF2, 0x03, 0xB0, 0x08, 0xEE, 0xB0, 0x0C, 0xEE, /* OUT 3F2 */
            0xBA, 0xF5, 0x03, 0xB0, 0x08, 0xEE,                   /* OUT 3F5,08 Sense */
            0xEC, 0xF4,                                           /* IN AL,DX */
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 32);
        expect_eq(static_cast<uint16_t>(p.c.ax() & 0xFFu), 0x00C0, "fdc sense C0");
    }
    {
        /* XT make for 'a' appears on PPI 60h (Py86 type_scancodes). */
        iron86::pc p;
        p.wire_pc_hw();
        p.type_keys("a");
        const uint8_t prog[] = {0xE4, 0x60, 0xF4};
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 8);
        expect_eq(static_cast<uint16_t>(p.c.ax() & 0xFFu), 0x001E, "xt scan a");
    }
    {
        const char *path =
            "/mnt/RetroCodeMess/Py86/ROM/IBM/PC/5150/BIOS_IBM5150_24APR81_5700051_U33.BIN";
        std::ifstream in(path, std::ios::binary);
        if (in)
        {
            std::vector<uint8_t> rom((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
            iron86::pc p;
            expect(p.c.load_bios_5150_8k(rom.data(), rom.size()), "bios load");
            p.wire_pc_hw();
            p.enable_fast_post();
            uint64_t n = 0;
            while ((!p.c.halted()) && (n < 4000000ull))
            {
                if (!p.c.step())
                {
                    break;
                }
                n++;
            }
            const bool err01 = p.c.halted() && (p.c.cs() == 0xF000) && (p.c.ip() == 0xE0B0);
            expect(!err01, "post not ERR01 E0B0");
            bool vram = false;
            for (uint32_t i = 0; i < 80u * 25u; i++)
            {
                const uint8_t ch = p.c.mem_read8(0xB8000u + i * 2u);
                if ((ch >= 32u) && (ch < 127u) && (ch != ' '))
                {
                    vram = true;
                    break;
                }
            }
            std::fprintf(stderr, "post steps=%llu CS:IP=%04X:%04X halted=%d last=%02X vram=%d\n",
                         static_cast<unsigned long long>(n), p.c.cs(), p.c.ip(),
                         static_cast<int>(p.c.halted()), p.c.last_op(), static_cast<int>(vram));
            expect(vram || (!err01 && (p.c.cs() == 0xF000)), "post progressed");
        }
        else
        {
            std::fputs("skip BIOS POST (ROM not mounted)\n", stderr);
        }
    }

    if (g_fail != 0)
    {
        return 1;
    }
    std::puts("iron86 pc tests ok");
    return 0;
}
