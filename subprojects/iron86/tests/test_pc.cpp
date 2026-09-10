/**
 * @file test_pc.cpp
 * @brief Packed 5150 chipset: PIC IMR, PIT, DMA, PPI DIP/256K nibble, speaker.
 */
#include "iron86/chs_lba.h"
#include "iron86/crtc.h"
#include "iron86/flopfs_pack.h"
#include "iron86/hw.h"
#include "iron86/pc.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <sys/stat.h>
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
    expect(sizeof(iron86::mc6845) == 29u, "sizeof mc6845");
    expect(sizeof(iron86::pc_hw) == 159u, "sizeof pc_hw");
    expect(offsetof(iron86::pc_hw, fdc) == 108u, "offsetof pc_hw.fdc");

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
        /* 5160 10JAN86 E360–E372: OCW1 00 then FF without a new ICW. */
        iron86::pc p;
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0xB0, 0x13, 0xE6, 0x20, 0xB0, 0x08, 0xE6, 0x21, 0xB0, 0x09, 0xE6, 0x21,
            0xB0, 0xFF, 0xE6, 0x21, /* IMR=FF after ICW */
            0xB0, 0x00, 0xE6, 0x21, 0xE4, 0x21, /* OUT 0; IN */
            0xB0, 0xFF, 0xE6, 0x21, 0xE4, 0x21, /* OUT FF; IN */
            0x04, 0x01, 0xF4,                   /* ADD AL,1  expect 0 */
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 64);
        expect_eq(static_cast<uint16_t>(p.c.ax() & 0xFFu), 0, "pic tst6 add");
        expect(p.c.halted(), "pic tst6 hlt");
    }
    {
        /* OCW1 before ICW (icw_step==0): 5160 still does IN 21h. */
        iron86::pc p;
        p.wire_pc_hw();
        const uint8_t prog[] = {0xB0, 0x00, 0xE6, 0x21, 0xE4, 0x21, 0xF4};
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 16);
        expect_eq(static_cast<uint16_t>(p.c.ax() & 0xFFu), 0, "pic ocw1 no icw");
    }
    {
        /* wall_clock_irq0 is a no-op until PIT ch0 is programmed. */
        iron86::pc p;
        p.wire_pc_hw();
        p.wall_clock_irq0();
        const uint8_t prog[] = {
            0xB0, 0x13, 0xE6, 0x20, 0xB0, 0x08, 0xE6, 0x21, 0xB0, 0x09, 0xE6, 0x21,
            0xB0, 0x00, 0xE6, 0x21, /* IMR=0 */
            0xE4, 0x21, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 32);
        expect_eq(static_cast<uint16_t>(p.c.ax() & 0xFFu), 0, "imr 0 after silent wall irq");
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
        /* Port C low nibble 0x0F → 64K + 15*32K = 544K (1981 BIOS max). */
        iron86::pc p;
        p.wire_pc_hw();
        const uint8_t prog[] = {0xE4, 0x62, 0xF4}; /* IN AL,62h; HLT */
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 8);
        expect_eq(static_cast<uint16_t>(p.c.ax() & 0x0Fu), 0x0D, "ppi pc sw1 lo");
    }
    {
        /* 5160 E211–E228: IN 62, OUT 61 AD, IN 62 → equipment 0x2D (1 FDD). */
        iron86::pc p;
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0xE4, 0x62, 0x24, 0x0F, 0x8A, 0xE0, 0xB0, 0xAD, 0xE6, 0x61, 0xE4, 0x62,
            0xB1, 0x04, 0xD2, 0xC0, 0x24, 0xF0, 0x0A, 0xC4, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 32);
        expect_eq(static_cast<uint16_t>(p.c.ax() & 0xFFu), 0x2D, "eq 2d 1 fdd");
    }
    {
        /* 5160 C10 OUT 61h,A5h (PB3=0) then E211 mux must still be 0x2D.
         * PB2 is 1 in both A5h and ADh; muxing on PB2 yields 0x22 (no IPL). */
        iron86::pc p;
        uint8_t rom[65536];
        std::memset(rom, 0xCC, sizeof(rom));
        rom[0xFFF0] = 0xEA;
        rom[0xFFF1] = 0x5B;
        rom[0xFFF2] = 0xE0;
        rom[0xFFF3] = 0x00;
        rom[0xFFF4] = 0xF0;
        expect(p.c.load_bios(rom, sizeof(rom)), "xt bios for ppi");
        expect(p.c.bios_xt(), "bios_xt");
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0xB0, 0xA5, 0xE6, 0x61,                   /* OUT 61h,A5h  C10 */
            0xE4, 0x62, 0x24, 0x0F, 0x8A, 0xE0,       /* IN 62, AND 0F, AH=AL */
            0xB0, 0xAD, 0xE6, 0x61,                   /* OUT 61h,ADh */
            0xE4, 0x62, 0xB1, 0x04, 0xD2, 0xC0, 0x24, 0xF0, 0x0A, 0xC4, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 40);
        expect_eq(static_cast<uint16_t>(p.c.ax() & 0xFFu), 0x2D, "eq 2d after C10 A5");
    }
    {
        /* OUT 3D9h latches color-select and BDA 40:66 (tdxview cga3d9). */
        iron86::pc p;
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0xB0, 0x20, 0xBA, 0xD9, 0x03, 0xEE, /* MOV AL,20h; MOV DX,3D9h; OUT DX,AL */
            0xEC, 0xF4,                         /* IN AL,DX; HLT */
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 16);
        expect_eq(static_cast<uint16_t>(p.c.ax() & 0xFFu), 0x20, "in 3d9");
        expect_eq(p.c.mem_read8(0x466u), 0x20, "bda 40:66");
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
        /* IN 60h does not ack; 5150 INT 9 PB7 pulse does. Two reads see
         * make 1Eh; after OR 80h / OUT 61h / restore, IN 60h is break 9Eh.
         * M1TP CGAGRPH wraps ROM INT 9 this way. */
        iron86::pc p;
        p.wire_pc_hw();
        p.c.set_cs(0x1000);
        p.c.set_ip(0x0100);
        p.c.mem_write8(0x10100u, 0xF4); /* HLT so type_scan pump does not run IVT */
        p.type_scan(0x1E);
        const uint8_t prog[] = {
            0xE4, 0x60, 0x88, 0xC3, 0xE4, 0x60, 0x38, 0xD8, 0x75, 0x17, 0x3C, 0x1E,
            0x75, 0x13, 0xE4, 0x61, 0x8A, 0xE0, 0x0C, 0x80, 0xE6, 0x61, 0x86, 0xE0,
            0xE6, 0x61, 0xE4, 0x60, 0x3C, 0x9E, 0x75, 0x01, 0xF4, 0xB0, 0xFF, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 64);
        expect_eq(static_cast<uint16_t>(p.c.ax() & 0xFFu), 0x009E, "ppi 60 latch wrap");
    }
    {
        /* Injected scancode must IRQ1 even if PB6 (kbd clock) is 0.
         * M1TP CGAGRPH: IVT[9] hooked, never INT 16. */
        iron86::pc p;
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0x33, 0xC0, 0x8E, 0xC0, 0x8C, 0xC8, 0x26, 0xA3, 0x26, 0x00, 0x26, 0xC7,
            0x06, 0x24, 0x00, 0x30, 0x01, 0xB0, 0x13, 0xE6, 0x20, 0xB0, 0x08, 0xE6,
            0x21, 0xB0, 0x09, 0xE6, 0x21, 0xB0, 0xFD, 0xE6, 0x21, 0xFB, 0xEB, 0xFE,
            0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
            0xE4, 0x60, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 24);
        p.type_scan(0x48);
        run(p.c, 32);
        expect(p.c.halted(), "irq1 inject hlt");
        expect_eq(static_cast<uint16_t>(p.c.ax() & 0xFFu), 0x0048, "irq1 inject scan");
    }
    {
        /* 5160 F9E3: OUT 61 08/C8/48 then IN 60 must be BAT 0xAA (else POST 301). */
        iron86::pc p;
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0xB0, 0x08, 0xE6, 0x61, 0xB0, 0xC8, 0xE6, 0x61, 0xB0, 0x48, 0xE6, 0x61,
            0xE4, 0x60, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 32);
        expect_eq(static_cast<uint16_t>(p.c.ax() & 0xFFu), 0x00AA, "xt kbd BAT AA");
    }
    {
        /* IRQ1: one edge until EOI. Else DOS DIR Internal stack overflow. */
        iron86::pc p;
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0x31, 0xC0, 0x8E, 0xD8, 0x8E, 0xD0, 0xBC, 0x00, 0x04, 0xB0, 0x13, 0xE6,
            0x20, 0xB0, 0x08, 0xE6, 0x21, 0xB0, 0x09, 0xE6, 0x21, 0xB0, 0xFD, 0xE6,
            0x21, 0xFB, 0xB9, 0x00, 0x10, 0xE2, 0xFE, 0xF4,
            /* INT 9 at CS:0120 (COM 0x20): inc [0500], ack 61h, EOI, IRET */
            0xFE, 0x06, 0x00, 0x05, 0xE4, 0x60, 0xE4, 0x61, 0x0C, 0x80, 0xE6, 0x61,
            0x24, 0x7F, 0xE6, 0x61, 0xB0, 0x20, 0xE6, 0x20, 0xCF,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        p.c.mem_write16(0x24, 0x0120);
        p.c.mem_write16(0x26, 0x1000);
        p.type_scan(0x20);
        run(p.c, 8000);
        const uint8_t hits = p.c.mem_read8(0x500);
        if (hits > 4u)
        {
            std::fprintf(stderr, "IRQ1 storm hits=%u\n", static_cast<unsigned>(hits));
        }
        expect(hits >= 1u, "irq1 delivered");
        expect(hits <= 4u, "irq1 not nested storm");
        expect(p.c.halted(), "irq1 test hlt");
    }
    {
        /* Unmasked ch0 address must move (POST ECA9 IN 00h wrap wait). */
        iron86::pc p;
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0xB0, 0x00, 0xE6, 0x0C, /* clear flip-flop */
            0xB0, 0x00, 0xE6, 0x00, 0xE6, 0x00, /* addr 0 */
            0xB0, 0x00, 0xE6, 0x0A,             /* unmask ch0 */
            0xB9, 0x10, 0x00, 0xE2, 0xFE,       /* delay */
            0xB0, 0x00, 0xE6, 0x0C, 0xE4, 0x00, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 64);
        expect(p.c.halted(), "dma tick hlt");
        expect((p.c.ax() & 0xFFu) != 0, "dma ch0 addr moved");
    }
    {
        /* PIT ch2 mode 3 + OUT 61h bits 0+1 → one rising-edge beep. */
        iron86::pc p;
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0xB0, 0xB6, 0xE6, 0x43, /* MOV AL,B6h; OUT 43h,AL */
            0xB0, 0xA9, 0xE6, 0x42, /* LSB */
            0xB0, 0x04, 0xE6, 0x42, /* MSB reload=04A9h */
            0xB0, 0x03, 0xE6, 0x61, /* OUT 61h, 03h (gate+data) */
            0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 32);
        expect(p.beep_count() >= 1u, "speaker beep");
    }
    {
        /* IN 3DA twice: POST TEST.10 needs status bits to change. */
        iron86::pc p;
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0xBA, 0xDA, 0x03, /* MOV DX,3DAh */
            0xEC, 0x88, 0xC3, 0xEC, 0x38, 0xD8, 0x74, 0x03, 0xB0, 0x01, 0xF4, 0xB0,
            0x00, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 16);
        expect_eq(static_cast<uint16_t>(p.c.ax() & 0xFFu), 1, "cga 3da toggle");
    }
    {
        /* IN 3BA twice: MDA status must also toggle (separate 6845). */
        iron86::pc p;
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0xBA, 0xBA, 0x03, /* MOV DX,3BAh */
            0xEC, 0x88, 0xC3, 0xEC, 0x38, 0xD8, 0x74, 0x03, 0xB0, 0x01, 0xF4, 0xB0,
            0x00, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 16);
        expect_eq(static_cast<uint16_t>(p.c.ax() & 0xFFu), 1, "mda 3ba toggle");
    }
    {
        /* BDA 40:49 = MDA: periodic tick is MDA; IN 3DA still toggles. */
        iron86::pc p;
        p.wire_pc_hw();
        p.c.mem_write8(0x449u, 7);
        const uint8_t prog[] = {
            0xBA, 0xDA, 0x03, /* MOV DX,3DAh */
            0xEC, 0x88, 0xC3, 0xEC, 0x38, 0xD8, 0x74, 0x03, 0xB0, 0x01, 0xF4, 0xB0,
            0x00, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 16);
        expect_eq(static_cast<uint16_t>(p.c.ax() & 0xFFu), 1, "cga 3da toggle mda mode");
    }
    {
        /* BDA 40:49 = CGA: periodic tick is CGA; IN 3BA still toggles. */
        iron86::pc p;
        p.wire_pc_hw();
        p.c.mem_write8(0x449u, 3);
        const uint8_t prog[] = {
            0xBA, 0xBA, 0x03, /* MOV DX,3BAh */
            0xEC, 0x88, 0xC3, 0xEC, 0x38, 0xD8, 0x74, 0x03, 0xB0, 0x01, 0xF4, 0xB0,
            0x00, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 16);
        expect_eq(static_cast<uint16_t>(p.c.ax() & 0xFFu), 1, "mda 3ba toggle cga mode");
    }
    {
        /* MDA LPT probe: 3BD not-busy 0xDF; 3BC data readback. */
        iron86::pc p;
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0xB0, 0xA5, 0xBA, 0xBC, 0x03, 0xEE, /* MOV AL,A5h; MOV DX,3BCh; OUT DX,AL */
            0xEC,                               /* IN AL,DX */
            0x88, 0xC4,                         /* MOV AH,AL */
            0xBA, 0xBD, 0x03, 0xEC,             /* MOV DX,3BDh; IN AL,DX */
            0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 16);
        expect_eq(p.c.ax(), 0xA5DF, "mda lpt 3bc/3bd");
    }
    {
        /* Separate 6845s: MDA R12 != CGA R12. */
        iron86::pc p;
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0xB0, 0x0C, 0xBA, 0xB4, 0x03, 0xEE, /* OUT 3B4,12 */
            0xB0, 0xAA, 0x42, 0xEE,             /* OUT 3B5,AAh */
            0xB0, 0x0C, 0xBA, 0xD4, 0x03, 0xEE, /* OUT 3D4,12 */
            0xB0, 0x55, 0x42, 0xEE,             /* OUT 3D5,55h */
            0xBA, 0xB5, 0x03, 0xEC, 0x88, 0xC4, /* IN 3B5 → AH */
            0xBA, 0xD5, 0x03, 0xEC,             /* IN 3D5 → AL */
            0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 32);
        expect_eq(p.c.ax(), 0xAA55, "mda/cga 6845 split");
    }
    {
        iron86::pc p;
        p.enable_audio(false);
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0xB0, 0xB6, 0xE6, 0x43, 0xB0, 0xA9, 0xE6, 0x42, 0xB0, 0x04, 0xE6, 0x42,
            0xB0, 0x03, 0xE6, 0x61, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 32);
        expect(p.beep_count() == 0u, "no-audio silent");
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
            expect_eq(p.c.bx(), 0x1234, "fast-post BX=1234");
            expect_eq(p.c.cs(), 0xF000, "fast-post CS");
            expect_eq(p.c.ip(), 0xE199, "fast-post IP");
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
    {
        uint8_t a[512];
        uint8_t bimg[512];
        std::memset(a, 0, sizeof(a));
        std::memset(bimg, 0, sizeof(bimg));
        a[0] = 0xAA;
        a[510] = 0x55;
        a[511] = 0xAA;
        bimg[0] = 0xBB;
        bimg[510] = 0x55;
        bimg[511] = 0xAA;
        iron86::pc p;
        expect(p.attach_floppy(0, a, sizeof(a)), "attach A");
        expect(p.attach_floppy(1, bimg, sizeof(bimg)), "attach B");
        p.boot();
        const uint8_t prog[] = {
            0x8C, 0xC8, 0x8E, 0xC0,                   /* MOV AX,CS; MOV ES,AX */
            0xB8, 0x01, 0x02, 0xBB, 0x00, 0x02,       /* AX=0201 BX=0200 */
            0xB9, 0x01, 0x00, 0xBA, 0x01, 0x00,       /* CX=0001 DX=0001 */
            0xCD, 0x13, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 32);
        expect((p.c.ax() & 0xFF00u) == 0, "int13 B AH=0");
        expect_eq(p.c.mem_read8(0x10200u), 0xBB, "int13 DL=1");
    }
    {
        /* 720K CHS cyl 50 after 5150 ROM load: ROM INT 13 is 40-track. */
        std::vector<uint8_t> img(500000, 0);
        img[900u * 512u] = 0x5A;
        iron86::pc p;
        expect(p.attach_floppy(0, img.data(), img.size()), "720k attach");
        const char *rom_path =
            "/mnt/TurboDebugger/Py86/ROM/IBM/PC/5150/BIOS_IBM5150_24APR81_5700051_U33.BIN";
        std::ifstream rin(rom_path, std::ios::binary);
        if (!rin)
        {
            rom_path =
                "/mnt/RetroCodeMess/Py86/ROM/IBM/PC/5150/BIOS_IBM5150_24APR81_5700051_U33.BIN";
            rin.open(rom_path, std::ios::binary);
        }
        if (rin)
        {
            std::vector<uint8_t> rom((std::istreambuf_iterator<char>(rin)),
                                     std::istreambuf_iterator<char>());
            expect(p.c.load_bios_5150_8k(rom.data(), rom.size()), "rom int13");
            expect(p.c.rom_bios(), "rom_bios");
        }
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0x8C, 0xC8, 0x8E, 0xC0,             /* MOV AX,CS; MOV ES,AX */
            0xB8, 0x01, 0x02, 0xBB, 0x00, 0x02, /* AX=0201 BX=0200 */
            0xB9, 0x01, 0x32, 0xBA, 0x00, 0x00, /* CX=3201 (cyl 50 sec 1) DX=0 */
            0xCD, 0x13, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 32);
        expect((p.c.ax() & 0xFF00u) == 0, "int13 cyl50 AH=0");
        expect_eq(p.c.mem_read8(0x10200u), 0x5A, "int13 cyl50 data");
    }
    {
        uint8_t bimg[512];
        std::memset(bimg, 0x11, sizeof(bimg));
        bimg[510] = 0x55;
        bimg[511] = 0xAA;
        iron86::pc p;
        expect(p.attach_floppy(1, bimg, sizeof(bimg)), "dip B");
        p.wire_pc_hw();
        const uint8_t prog[] = {0xB0, 0xFC, 0xE6, 0x61, 0xE4, 0x60, 0xF4};
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 16);
        expect_eq(static_cast<uint16_t>(p.c.ax() & 0xFFu), 0x6D, "ppi dip 2 fdd");
    }
    {
        /* DOR select drive 1: post-reset Sense Int still returns ST0=C0. */
        uint8_t bimg[512];
        std::memset(bimg, 0, sizeof(bimg));
        bimg[510] = 0x55;
        bimg[511] = 0xAA;
        iron86::pc p;
        expect(p.attach_floppy(1, bimg, sizeof(bimg)), "fdc B attach");
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0xBA, 0xF2, 0x03, 0xB0, 0x08, 0xEE, 0xB0, 0x0D, 0xEE, /* OUT 3F2 sel=1 */
            0xBA, 0xF5, 0x03, 0xB0, 0x08, 0xEE,                   /* Sense Int */
            0xEC, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 32);
        expect_eq(static_cast<uint16_t>(p.c.ax() & 0xFFu), 0x00C0, "fdc B sense C0");
    }
    {
        /* Read Data command drive 1 DMA-fills from B:, not A:. */
        uint8_t a[512];
        uint8_t bimg[512];
        std::memset(a, 0xAA, sizeof(a));
        std::memset(bimg, 0xBB, sizeof(bimg));
        a[510] = 0x55;
        a[511] = 0xAA;
        bimg[510] = 0x55;
        bimg[511] = 0xAA;
        iron86::pc p;
        expect(p.attach_floppy(0, a, sizeof(a)), "rd A");
        expect(p.attach_floppy(1, bimg, sizeof(bimg)), "rd B");
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0xB0, 0x46, 0xE6, 0x0B,             /* mode ch2 write */
            0xB0, 0x00, 0xE6, 0x04, 0xE6, 0x04, /* addr 0000 */
            0xB0, 0x01, 0xE6, 0x81,             /* page 01h → 10000h (XT 4-bit) */
            0xB0, 0xFF, 0xE6, 0x05,             /* count lo */
            0xB0, 0x01, 0xE6, 0x05,             /* count hi 01FFh */
            0xB0, 0x02, 0xE6, 0x0A,             /* unmask ch2 */
            0xBA, 0xF2, 0x03, 0xB0, 0x08, 0xEE, 0xB0, 0x2D, 0xEE,
            0xBA, 0xF5, 0x03, 0xB0, 0x08, 0xEE, 0xEC, 0xEC, /* Sense Int */
            0xB0, 0x03, 0xEE, 0xB0, 0xDF, 0xEE, 0xB0, 0x02, 0xEE, /* Specify */
            0xB0, 0x06, 0xEE, 0xB0, 0x01, 0xEE,             /* Read Data, drv 1 */
            0xB0, 0x00, 0xEE, 0xB0, 0x00, 0xEE, 0xB0, 0x01, 0xEE, 0xB0, 0x02, 0xEE,
            0xB0, 0x09, 0xEE, 0xB0, 0x2A, 0xEE, 0xB0, 0xFF, 0xEE,
            0xEC, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 128);
        expect_eq(p.c.mem_read8(0x10000u), 0xBB, "fdc read B");
    }
    {
        const char *dir = "/tmp/iron86-flopfs-pack-test";
        std::vector<uint8_t> img;
        (void)mkdir(dir, 0755);
        {
            std::ofstream f(std::string(dir) + "/HELLO.TXT", std::ios::binary);
            f << "hi";
        }
        expect(iron86::flopfs_pack_dir(dir, &img), "pack dir");
        expect(img.size() == 368640u, "pack 360k");
        expect(std::memcmp(img.data() + 512, "FLOPFS01", 8) == 0, "pack magic");
        expect(img[3 * 512] == static_cast<uint8_t>('H'), "pack dirent");
        expect(img[5 * 512] == static_cast<uint8_t>('h'), "pack payload");
        iron86::pc p;
        expect(p.attach_floppy_path(1, dir), "attach path B dir");
        p.wire_pc_hw();
        const uint8_t dip[] = {0xB0, 0xFC, 0xE6, 0x61, 0xE4, 0x60, 0xF4};
        p.c.load_com(dip, sizeof(dip), 0x1000);
        run(p.c, 16);
        expect_eq(static_cast<uint16_t>(p.c.ax() & 0xFFu), 0x6D, "path B dip 6d");
    }
    {
        /* 720K B: last sector cyl 79 head 1 sec 9 = LBA 1439. */
        std::vector<uint8_t> img(iron86::FLOPFS_BYTES_720K, 0);
        img[1439u * 512u] = 0x7E;
        img[510] = 0x55;
        img[511] = 0xAA;
        iron86::pc p;
        expect(p.attach_floppy(1, img.data(), img.size()), "attach 720k B");
        p.boot();
        const uint8_t prog[] = {
            0x8C, 0xC8, 0x8E, 0xC0,             /* MOV AX,CS; MOV ES,AX */
            0xB8, 0x01, 0x02, 0xBB, 0x00, 0x02, /* AX=0201 BX=0200 */
            0xB9, 0x09, 0x4F, 0xBA, 0x01, 0x01, /* CX=4F09 (cyl79 sec9) DX=head1 drv1 */
            0xCD, 0x13, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 32);
        expect((p.c.ax() & 0xFF00u) == 0, "int13 720k AH=0");
        expect_eq(p.c.mem_read8(0x10200u), 0x7E, "int13 720k last sec");
    }
    {
        /* Runtime swap B: (tdxctl floppy b PATH). */
        std::vector<uint8_t> first(368640u, 0);
        std::vector<uint8_t> second(368640u, 0);
        first[0] = 0x11;
        second[0] = 0x22;
        iron86::pc p;
        expect(p.attach_floppy(1, first.data(), first.size()), "swap B first");
        expect(p.attach_floppy(1, second.data(), second.size()), "swap B second");
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0x8C, 0xC8, 0x8E, 0xC0,
            0xB8, 0x01, 0x02, 0xBB, 0x00, 0x02,
            0xB9, 0x01, 0x00, 0xBA, 0x01, 0x00,
            0xCD, 0x13, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 32);
        expect((p.c.ax() & 0xFF00u) == 0, "swap B AH=0");
        expect_eq(p.c.mem_read8(0x10200u), 0x22, "swap B new boot");
    }
    {
        /* 1.44M 80/2/18: sector 18 is still C=0 H=0 (would be H=1 if 9 spt). */
        std::vector<uint8_t> img(1474560u, 0);
        img[17u * 512u] = 0xA5;
        iron86::pc p;
        expect(p.attach_floppy(0, img.data(), img.size()), "1440k attach");
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0x8C, 0xC8, 0x8E, 0xC0,
            0xB8, 0x01, 0x02, 0xBB, 0x00, 0x02,
            0xB9, 0x12, 0x00, 0xBA, 0x00, 0x00, /* C=0 S=18 H=0 */
            0xCD, 0x13, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 32);
        expect((p.c.ax() & 0xFF00u) == 0, "1440k AH=0");
        expect_eq(p.c.mem_read8(0x10200u), 0xA5, "1440k spt18");
    }
    {
        /* INT 13h AH=03 write then AH=02 read (SETUP config on B:). */
        std::vector<uint8_t> img(368640u, 0);
        iron86::pc p;
        expect(p.attach_floppy(1, img.data(), img.size()), "write attach B");
        p.wire_pc_hw();
        p.c.mem_write8(0x10200u, 0x5A);
        const uint8_t prog[] = {
            0x8C, 0xC8, 0x8E, 0xC0,
            0xB8, 0x01, 0x03, 0xBB, 0x00, 0x02, /* AH=3 write 1 sec */
            0xB9, 0x01, 0x00, 0xBA, 0x01, 0x00,
            0xCD, 0x13,
            0xB8, 0x01, 0x02, 0xBB, 0x00, 0x04, /* AH=2 read to 0400 */
            0xB9, 0x01, 0x00, 0xBA, 0x01, 0x00,
            0xCD, 0x13, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 64);
        expect((p.c.ax() & 0xFF00u) == 0, "int13 write AH=0");
        expect_eq(p.c.mem_read8(0x10400u), 0x5A, "int13 write roundtrip");
    }
    {
        /* INT 10h AH=02/03/09: SETUP.TNK centers menu lines (not AH=0E only). */
        iron86::pc p;
        p.boot();
        const uint8_t prog[] = {
            0xB4, 0x02, 0xB6, 0x05, 0xB2, 0x0A, 0xCD, 0x10, /* AH=2 DH=5 DL=10 */
            0xB4, 0x03, 0xCD, 0x10,                         /* AH=3 get cursor */
            0xB4, 0x09, 0xB0, 0x41, 0xB3, 0x1E, 0xB9, 0x01, 0x00, 0xCD, 0x10,
            0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 40);
        expect_eq(p.c.dx(), 0x050A, "int10 cursor DX");
        expect_eq(p.c.mem_read8(0xB8000u + (5u * 80u + 10u) * 2u), 0x41, "int10 AH=9 char");
        expect_eq(p.c.mem_read8(0xB8000u + (5u * 80u + 10u) * 2u + 1u), 0x1E, "int10 AH=9 attr");
    }
    {
        /* SETUP.TNK cls: INT 10h AX=0600h BH=0 CX=0 DX=184Fh (full 80×25). */
        iron86::pc p;
        p.boot();
        p.c.mem_write8(0xB8000u, static_cast<uint8_t>('X'));
        p.c.mem_write8(0xB8000u + 1u, 0x07);
        const uint8_t prog[] = {
            0xB8, 0x00, 0x06, 0xB7, 0x07, 0xB9, 0x00, 0x00, 0xBA, 0x4F, 0x18, 0xCD, 0x10,
            0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 20);
        expect_eq(p.c.mem_read8(0xB8000u), static_cast<uint16_t>(' '), "int10 AH=6 clear");
        expect_eq(p.c.mem_read8(0xB8000u + 1u), 0x07, "int10 AH=6 attr");
    }
    {
        /* BIOS ch0 mode 3 reload 0: stride must fire IRQ0 within a tdx F9 slice
         * (~32k insns). ISR at 1000:0300 increments 40:6C and EOIs. */
        iron86::pc p;
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0xB0, 0x13, 0xE6, 0x20,             /* MOV AL,13h; OUT 20h,AL ICW1 */
            0xB0, 0x08, 0xE6, 0x21,             /* ICW2 = 08h */
            0xB0, 0x09, 0xE6, 0x21,             /* ICW4 */
            0xB0, 0xFE, 0xE6, 0x21,             /* IMR unmask IRQ0 */
            0xB0, 0x36, 0xE6, 0x43,             /* PIT ch0 mode 3 */
            0xB0, 0x20, 0xE6, 0x40,             /* reload LSB 32 */
            0xB0, 0x00, 0xE6, 0x40,             /* reload MSB 0 */
            0x31, 0xC0, 0x8E, 0xD8,             /* XOR AX,AX; MOV DS,AX */
            0xC7, 0x06, 0x20, 0x00, 0x00, 0x03, /* IVT[08] off = 0300h */
            0xC7, 0x06, 0x22, 0x00, 0x00, 0x10, /* IVT[08] seg = 1000h */
            0xC7, 0x06, 0x6C, 0x04, 0x00, 0x00, /* 40:6C = 0 */
            0xFB,                               /* STI */
            0xA1, 0x6C, 0x04,                   /* MOV AX,[046C] */
            0x3D, 0x02, 0x00,                   /* CMP AX,2 */
            0x72, 0xF8,                         /* JB to MOV AX (IP after JB - 8) */
            0xF4,                               /* HLT */
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        static const uint8_t isr[] = {
            0x1E,                   /* PUSH DS */
            0x31, 0xC0,             /* XOR AX,AX */
            0x8E, 0xD8,             /* MOV DS,AX */
            0xFF, 0x06, 0x6C, 0x04, /* INC WORD [046C] */
            0x1F,                   /* POP DS */
            0xB0, 0x20,             /* MOV AL,20h */
            0xE6, 0x20,             /* OUT 20h,AL EOI */
            0xCF,                   /* IRET */
        };
        for (size_t i = 0; i < sizeof(isr); i++)
        {
            p.c.mem_write8(0x10000u + 0x300u + static_cast<uint32_t>(i), isr[i]);
        }
        run(p.c, 20000);
        expect(p.c.halted(), "pit reload0 irq0 hlt");
        expect(p.c.mem_read16(0x46Cu) >= 2u, "pit reload0 40:6C");
    }
    {
        /* --bios INT 10h AH=0Eh fast path (24APR81 VIDEO_IO = F000:F065). */
        const char *rom_path =
            "/mnt/TurboDebugger/Py86/ROM/IBM/PC/5150/BIOS_IBM5150_24APR81_5700051_U33.BIN";
        std::ifstream rin(rom_path, std::ios::binary);
        if (!rin)
        {
            rom_path =
                "/mnt/RetroCodeMess/Py86/ROM/IBM/PC/5150/BIOS_IBM5150_24APR81_5700051_U33.BIN";
            rin.open(rom_path, std::ios::binary);
        }
        if (rin)
        {
            std::vector<uint8_t> rom((std::istreambuf_iterator<char>(rin)),
                                     std::istreambuf_iterator<char>());
            {
                iron86::pc p;
                expect(p.c.load_bios_5150_8k(rom.data(), rom.size()), "int10-0e rom");
                p.wire_pc_hw();
                p.c.set_ivt(0x10, 0xF000, 0xF065);
                p.c.mem_write8(0x449u, 3); /* CRT_MODE 80×25 */
                p.c.mem_write16(0x450u, 0);
                const uint8_t prog[] = {
                    0xB4, 0x0E, 0xB0, 0x5A, 0xCD, 0x10, 0xF4, /* AH=0E AL='Z' */
                };
                p.c.load_com(prog, sizeof(prog), 0x1000);
                run(p.c, 16);
                expect(p.c.halted(), "int10-0e hlt");
                expect_eq(p.c.cs(), 0x1000, "int10-0e cs");
                expect_eq(p.c.mem_read8(0xB8000u), static_cast<uint16_t>('Z'), "int10-0e vram");
                expect_eq(p.c.mem_read16(0x450u), 0x0001, "int10-0e bda cursor");
            }
            {
                /* Hooked INT 10h: IRET stub, VRAM must stay empty. */
                iron86::pc p;
                expect(p.c.load_bios_5150_8k(rom.data(), rom.size()), "int10-hook rom");
                p.wire_pc_hw();
                p.c.set_ivt(0x10, 0x1000, 0x0300);
                p.c.mem_write8(0x449u, 3);
                p.c.mem_write8(0x10300u, 0xCF); /* IRET */
                const uint8_t prog[] = {
                    0xB4, 0x0E, 0xB0, 0x5A, 0xCD, 0x10, 0xF4,
                };
                p.c.load_com(prog, sizeof(prog), 0x1000);
                run(p.c, 16);
                expect(p.c.halted(), "int10-hook hlt");
                expect_eq(p.c.mem_read8(0xB8000u), 0, "int10-hook no steal");
            }
            {
                /* AH=00 mode-set always enters ROS VIDEO_IO. */
                iron86::pc p;
                expect(p.c.load_bios_5150_8k(rom.data(), rom.size()), "int10-00 rom");
                p.wire_pc_hw();
                p.c.set_ivt(0x10, 0xF000, 0xF065);
                p.c.mem_write8(0x449u, 3);
                const uint8_t prog[] = {0xCD, 0x10, 0xF4};
                p.c.load_com(prog, sizeof(prog), 0x1000);
                p.c.set_ax(0x0003);
                expect(p.c.step(), "int10-00 step");
                expect_eq(p.c.cs(), 0xF000, "int10-00 cs rom");
                expect_eq(p.c.ip(), 0xF065, "int10-00 ip rom");
            }
        }
    }

    {
        uint32_t spt = 0;
        uint32_t heads = 0;
        iron86::floppy_geom(iron86::k_floppy_bytes_160k, &spt, &heads);
        expect((spt == 8u) && (heads == 1u), "geom 160k 40/1/8");
        iron86::floppy_geom(iron86::k_floppy_bytes_180k, &spt, &heads);
        expect((spt == 9u) && (heads == 1u), "geom 180k 40/1/9 not 360k");
        iron86::floppy_geom(iron86::k_floppy_bytes_320k, &spt, &heads);
        expect((spt == 8u) && (heads == 2u), "geom 320k 40/2/8");
        iron86::floppy_geom(iron86::k_floppy_bytes_360k, &spt, &heads);
        expect((spt == 9u) && (heads == 2u), "geom 360k 40/2/9");
        iron86::floppy_geom(iron86::k_floppy_bytes_720k, &spt, &heads);
        expect((spt == 9u) && (heads == 2u), "geom 720k 80/2/9");
        iron86::floppy_geom(iron86::k_floppy_bytes_1200k, &spt, &heads);
        expect((spt == 15u) && (heads == 2u), "geom 1.2M 80/2/15");
        iron86::floppy_geom(iron86::k_floppy_bytes_1440k, &spt, &heads);
        expect((spt == 18u) && (heads == 2u), "geom 1.44M 80/2/18");
        iron86::floppy_geom(512u, &spt, &heads);
        expect((spt == 8u) && (heads == 1u), "geom 512 nearest 160k");
        iron86::floppy_geom(200000u, &spt, &heads);
        expect((spt == 9u) && (heads == 1u), "geom 200k nearest 180k");
        expect_eq(static_cast<uint16_t>(iron86_chs_to_lba(50u, 2u, 0u, 9u, 1u)), 900u,
                  "lba cyl50");
        expect_eq(static_cast<uint16_t>(iron86_chs_to_lba(0u, 1u, 0u, 9u, 9u)), 8u, "lba 180k s9");
        expect_eq(static_cast<uint16_t>(iron86_chs_to_lba(0u, 2u, 1u, 8u, 1u)), 8u, "lba 320k h1");
    }
    {
        /* 180K: cyl 1 / head 0 / sec 1 is LBA 9 (would be 18 if 360K 2 heads). */
        std::vector<uint8_t> img(iron86::k_floppy_bytes_180k, 0);
        img[9u * 512u] = 0xB1;
        iron86::pc p;
        expect(p.attach_floppy(0, img.data(), img.size()), "180k attach");
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0x8C, 0xC8, 0x8E, 0xC0, 0xB8, 0x01, 0x02, 0xBB, 0x00, 0x02,
            0xB9, 0x01, 0x01, 0xBA, 0x00, 0x00, /* C=1 S=1 H=0 */
            0xCD, 0x13, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 32);
        expect((p.c.ax() & 0xFF00u) == 0, "180k AH=0");
        expect_eq(p.c.mem_read8(0x10200u), 0xB1, "180k cyl1");
    }
    {
        /* 320K: C=0 H=1 S=1 is LBA 8 (would be 9 if 9 spt). */
        std::vector<uint8_t> img(iron86::k_floppy_bytes_320k, 0);
        img[8u * 512u] = 0xC8;
        iron86::pc p;
        expect(p.attach_floppy(0, img.data(), img.size()), "320k attach");
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0x8C, 0xC8, 0x8E, 0xC0, 0xB8, 0x01, 0x02, 0xBB, 0x00, 0x02,
            0xB9, 0x01, 0x00, 0xBA, 0x00, 0x01, /* C=0 S=1 H=1 */
            0xCD, 0x13, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 32);
        expect((p.c.ax() & 0xFF00u) == 0, "320k AH=0");
        expect_eq(p.c.mem_read8(0x10200u), 0xC8, "320k h1");
    }
    {
        /* INT 13 AH=08: cyls = size/(512*spt*heads); 180K → CH=39 CL=9 DH=0. */
        std::vector<uint8_t> img(iron86::k_floppy_bytes_180k, 0);
        iron86::pc p;
        expect(p.attach_floppy(0, img.data(), img.size()), "ah08 180k attach");
        p.wire_pc_hw();
        const uint8_t prog[] = {0xB4, 0x08, 0xB2, 0x00, 0xCD, 0x13, 0xF4};
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 16);
        expect((p.c.ax() & 0xFF00u) == 0, "ah08 180k AH=0");
        expect_eq(p.c.cx(), 0x2709, "ah08 180k CX");
        expect_eq(static_cast<uint16_t>(p.c.dx() >> 8), 0, "ah08 180k DH heads-1");
    }
    {
        std::vector<uint8_t> img(iron86::k_floppy_bytes_320k, 0);
        iron86::pc p;
        expect(p.attach_floppy(0, img.data(), img.size()), "ah08 320k attach");
        p.wire_pc_hw();
        const uint8_t prog[] = {0xB4, 0x08, 0xB2, 0x00, 0xCD, 0x13, 0xF4};
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 16);
        expect_eq(p.c.cx(), 0x2708, "ah08 320k CX");
        expect_eq(static_cast<uint16_t>(p.c.dx() >> 8), 1, "ah08 320k DH");
    }
    {
        std::vector<uint8_t> img(iron86::k_floppy_bytes_1200k, 0);
        img[14u * 512u] = 0x15;
        iron86::pc p;
        expect(p.attach_floppy(0, img.data(), img.size()), "1.2M attach");
        p.wire_pc_hw();
        const uint8_t prog[] = {
            0x8C, 0xC8, 0x8E, 0xC0, 0xB8, 0x01, 0x02, 0xBB, 0x00, 0x02,
            0xB9, 0x0F, 0x00, 0xBA, 0x00, 0x00, /* C=0 S=15 H=0 */
            0xCD, 0x13, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 32);
        expect((p.c.ax() & 0xFF00u) == 0, "1.2M AH=0");
        expect_eq(p.c.mem_read8(0x10200u), 0x15, "1.2M spt15");
        const uint8_t ah08[] = {0xB4, 0x08, 0xB2, 0x00, 0xCD, 0x13, 0xF4};
        p.c.load_com(ah08, sizeof(ah08), 0x1000);
        run(p.c, 16);
        expect_eq(p.c.cx(), 0x4F0F, "ah08 1.2M CX");
    }
    {
        /* INT 13 DL=80h must not map HDD onto floppy (Xebec ROS owns C:). */
        uint8_t img[512];
        std::memset(img, 0xAA, sizeof(img));
        img[510] = 0x55;
        img[511] = 0xAA;
        iron86::pc p;
        expect(p.attach_floppy(0, img, sizeof(img)), "dl80 attach");
        p.boot();
        p.c.set_ivt(0x13, 0x1000, 0x0300);
        p.c.mem_write8(0x10300u, 0xC6); /* MOV BYTE [0200], EEh */
        p.c.mem_write8(0x10301u, 0x06);
        p.c.mem_write8(0x10302u, 0x00);
        p.c.mem_write8(0x10303u, 0x02);
        p.c.mem_write8(0x10304u, 0xEE);
        p.c.mem_write8(0x10305u, 0xF4);
        const uint8_t prog[] = {
            0x8C, 0xC8, 0x8E, 0xC0, 0xB8, 0x01, 0x02, 0xBB, 0x00, 0x02,
            0xB9, 0x01, 0x00, 0xBA, 0x80, 0x00, /* DX=0080 HDD */
            0xCD, 0x13, 0xF4,
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 32);
        expect_eq(p.c.mem_read8(0x10200u), 0xEE, "int13 DL=80 not floppy");
    }
    {
        uint8_t img[512];
        std::memset(img, 0, sizeof(img));
        const char *hdc = "/tmp/iron86-hdc-stub.img";
        {
            std::ofstream f(hdc, std::ios::binary);
            f.write(reinterpret_cast<const char *>(img), sizeof(img));
        }
        iron86::pc p;
        expect(p.attach_hdc_path(hdc), "hdc stub attach");
        expect(p.hdc_path() == hdc, "hdc path stored");
        expect(p.hdc_bytes() == 512u, "hdc size 512");
        expect(!p.attach_hdc_path("/no/such/hdc.img"), "hdc missing");
        expect(!p.attach_hdc_path(""), "hdc empty");
        expect(!p.attach_hdc_path("/dev/sda"), "hdc refuse /dev/sda");
        expect(!p.attach_hdc_path("/dev/sdb"), "hdc refuse /dev/sdb");
    }
    {
        /* 10JAN86 F9E3 + dummy ISR FF23. 301 if CX=0, BL!=AA, or stuck-key IN 60. */
        const char *bios_path = "tests/images/hdc-proof/xt5160_10jan86_64k.bin";
        std::ifstream rin(bios_path, std::ios::binary);
        if (!rin)
        {
            bios_path = "/mnt/TurboDebugger/subprojects/iron86/tests/images/hdc-proof/"
                        "xt5160_10jan86_64k.bin";
            rin.open(bios_path, std::ios::binary);
        }
        expect(static_cast<bool>(rin), "5160 10jan86 bios open");
        if (rin)
        {
            std::vector<uint8_t> rom((std::istreambuf_iterator<char>(rin)),
                                     std::istreambuf_iterator<char>());
            expect(rom.size() == 65536u, "5160 bios 64k");
            iron86::pc p;
            expect(p.c.load_bios(rom.data(), rom.size()), "load 64k bios");
            {
                const uint8_t was = p.c.mem_read8(0xFE01Au);
                p.c.mem_write8(0xFE01Au, static_cast<uint8_t>(was ^ 0xFFu));
                expect_eq(p.c.mem_read8(0xFE01Au), was, "F000 ROM write ignored");
                p.c.mem_write8(0x400u, 0xAAu);
                expect_eq(p.c.mem_read8(0x400u), 0x00AA, "RAM below ROM writable");
            }
            p.wire_pc_hw();
            /* COM: DS=0, IVT 8/9 → F000:FF23, PIC ICW, STI, CALL F9E3 copy,
             * save CX/BX/[046B], then E3E9 stuck-key IN 60h. */
            std::vector<uint8_t> prog = {
                0x31, 0xC0, 0x8E, 0xD8, 0x8E, 0xC0, 0x8E, 0xD0, 0xBC, 0x00, 0x04,
                0xC7, 0x06, 0x20, 0x00, 0x23, 0xFF, 0xC7, 0x06, 0x22, 0x00, 0x00, 0xF0,
                0xC7, 0x06, 0x24, 0x00, 0x23, 0xFF, 0xC7, 0x06, 0x26, 0x00, 0x00, 0xF0,
                0xB0, 0x13, 0xE6, 0x20, 0xB0, 0x08, 0xE6, 0x21, 0xB0, 0x09, 0xE6, 0x21,
                0xB0, 0xFF, 0xE6, 0x21, 0xFB, 0xE8, 0x24, 0x00, 0x89, 0x0E, 0x00, 0x05,
                0x89, 0x1E, 0x02, 0x05, 0xA0, 0x6B, 0x04, 0xA2, 0x06, 0x05, 0xB0, 0x49,
                0xE6, 0x61, 0xB0, 0xC8, 0xE6, 0x61, 0xB0, 0x48, 0xE6, 0x61, 0x31, 0xC9,
                0xE2, 0xFE, 0xE4, 0x60, 0xA2, 0x04, 0x05, 0xF4,
            };
            prog.insert(prog.end(), rom.begin() + 0xF9E3, rom.begin() + 0xF9E3 + 47);
            p.c.load_com(prog.data(), prog.size(), 0x1000);
            run(p.c, 300000);
            expect(p.c.halted(), "f9e3 hlt");
            const uint16_t cx = p.c.mem_read16(0x500);
            const uint8_t bl = p.c.mem_read8(0x502);
            const uint8_t in60 = p.c.mem_read8(0x504);
            const uint8_t b46b = p.c.mem_read8(0x46B);
            if ((cx == 0) || (bl != 0xAAu) || (in60 != 0) || ((b46b & 2u) == 0))
            {
                std::fprintf(stderr, "F9E3 diag CX=%04X BL=%02X IN60=%02X [046B]=%02X\n",
                             cx, static_cast<unsigned>(bl), static_cast<unsigned>(in60),
                             static_cast<unsigned>(b46b));
            }
            expect(cx != 0, "f9e3 irq1 cx");
            expect_eq(bl, 0x00AA, "f9e3 BAT BL");
            expect((b46b & 2u) != 0, "f9e3 [046B] bit1");
            expect_eq(in60, 0, "f9e3 stuck-key IN60");
        }
    }

    if (g_fail != 0)
    {
        return 1;
    }
    std::puts("iron86 pc tests ok");
    return 0;
}
