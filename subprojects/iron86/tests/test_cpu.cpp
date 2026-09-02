/**
 * @file test_cpu.cpp
 * @brief iron86 CPU: hand-assembled opcode snippets (Py86 subset).
 */
#include "iron86/cpu.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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

static void run(iron86::cpu &c, int max_steps = 64)
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

static bool flag(const iron86::cpu &c, uint16_t bit)
{
    return (c.flags() & bit) != 0;
}

int main()
{
    {
        iron86::cpu c;
        const uint8_t prog[] = {0xB8, 0x01, 0x00, 0x40, 0xF4}; /* mov ax,1; inc ax; hlt */
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect(c.halted(), "hlt");
        expect_eq(c.ax(), 2, "ax==2");
        expect_eq(c.cs(), 0x1000, "cs");
    }
    {
        iron86::cpu c;
        const uint8_t prog[] = {0xB8, 0x01, 0x00, 0xCD, 0x20}; /* mov ax,1; int 20 */
        c.mem_write8(iron86::cpu::phys(0xF000, 0), 0xF4);
        c.set_ivt(0x20, 0xF000, 0);
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect(c.halted(), "int20 hlt");
        expect_eq(c.ax(), 1, "ax==1 after int20");
        expect_eq(c.cs(), 0xF000, "cs at stub");
    }
    {
        /* PUSH AX / POP AX */
        iron86::cpu c;
        const uint8_t prog[] = {0xB8, 0x34, 0x12, 0x50, 0xB8, 0x00, 0x00, 0x58, 0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect(c.halted(), "push/pop hlt");
        expect_eq(c.ax(), 0x1234, "push/pop ax");
        expect_eq(c.sp(), 0xFFFE, "sp restored");
    }
    {
        /* PUSH BX / POP CX */
        iron86::cpu c;
        const uint8_t prog[] = {0xBB, 0x78, 0x56, 0x53, 0xB9, 0x00, 0x00, 0x59, 0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect_eq(c.cx(), 0x5678, "pop cx from bx");
        expect_eq(c.bx(), 0x5678, "bx unchanged");
    }
    {
        /* INC CX / DEC BX  (40-4F) */
        iron86::cpu c;
        const uint8_t prog[] = {0xB9, 0xFF, 0xFF, 0x41, 0xBB, 0x01, 0x00, 0x4B, 0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect_eq(c.cx(), 0, "inc cx wrap");
        expect_eq(c.bx(), 0, "dec bx");
        expect(flag(c, iron86::k_flag_zf), "dec sets zf");
        expect(!flag(c, iron86::k_flag_of), "dec bx of clear");
    }
    {
        /* INC does not change CF (set CF via ADD AX,FFFF) */
        iron86::cpu c;
        const uint8_t prog[] = {0xB8, 0x01, 0x00, 0x05, 0xFF, 0xFF, 0xB9, 0x00, 0x00, 0x41, 0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect_eq(c.ax(), 0, "add ax,ffff");
        expect_eq(c.cx(), 1, "inc cx");
        expect(flag(c, iron86::k_flag_cf), "inc preserves cf");
        expect(!flag(c, iron86::k_flag_zf), "inc cx zf clear");
    }
    {
        /* JMP rel16 skips MOV AX,FFFF */
        iron86::cpu c;
        const uint8_t prog[] = {0xE9, 0x03, 0x00, 0xB8, 0xFF, 0xFF, 0xB8, 0x01, 0x00, 0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect_eq(c.ax(), 1, "jmp rel16");
    }
    {
        /* CALL rel16 / RET: AX=5, SP restored */
        iron86::cpu c;
        const uint8_t prog[] = {
            0xE8, 0x01, 0x00, /* call +1 → 0104 */
            0xF4,             /* 0103 hlt */
            0xB8, 0x05, 0x00, /* 0104 mov ax,5 */
            0xC3              /* 0107 ret */
        };
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect(c.halted(), "call/ret hlt");
        expect_eq(c.ax(), 5, "call/ret ax");
        expect_eq(c.sp(), 0xFFFE, "call/ret sp");
        expect_eq(c.ip(), 0x0104, "hlt after ret");
    }
    {
        /* MOV r/m16: [BX] store/load */
        iron86::cpu c;
        const uint8_t prog[] = {0xB8, 0xEF, 0xBE, 0xBB, 0x00, 0x02, 0x89, 0x07,
                                0xB8, 0x00, 0x00, 0x8B, 0x07, 0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect_eq(c.ax(), 0xBEEF, "mov [bx] roundtrip");
        expect_eq(c.mem_read16(iron86::cpu::phys(0x1000, 0x0200)), 0xBEEF, "ds:0200");
    }
    {
        iron86::cpu c;
        const uint8_t prog[] = {0xBB, 0x34, 0x12, 0x8B, 0xC3, 0xF4}; /* mov bx,1234; mov ax,bx */
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect_eq(c.ax(), 0x1234, "mov ax,bx");
    }
    {
        /* MOV r/m8 [BX] */
        iron86::cpu c;
        const uint8_t prog[] = {0xB0, 0x5A, 0xBB, 0x00, 0x02, 0x88, 0x07, 0xB0, 0x00, 0x8A, 0x07,
                                0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect_eq(static_cast<uint16_t>(c.ax() & 0xFF), 0x5A, "mov [bx] al");
        expect_eq(c.mem_read8(iron86::cpu::phys(0x1000, 0x0200)), 0x5A, "ds:0200 byte");
    }
    {
        /* MOV AX, [disp16]  8B 06 00 02 */
        iron86::cpu c;
        const uint8_t prog[] = {0x8B, 0x06, 0x00, 0x02, 0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        c.mem_write16(iron86::cpu::phys(0x1000, 0x0200), 0xCAFE);
        run(c);
        expect_eq(c.ax(), 0xCAFE, "mov ax,[disp16]");
    }
    {
        /* MOV AX, [BP+0] uses SS not DS */
        iron86::cpu c;
        const uint8_t prog[] = {0xBD, 0x10, 0x00, 0x8B, 0x46, 0x00, 0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        c.set_ss(0x2000);
        c.mem_write16(iron86::cpu::phys(0x2000, 0x0010), 0xAABB);
        c.mem_write16(iron86::cpu::phys(0x1000, 0x0010), 0x1111);
        run(c);
        expect_eq(c.ax(), 0xAABB, "mov ax,[bp] ss");
    }
    {
        /* ADD AL,imm8 / ADD AX,imm16 */
        iron86::cpu c;
        const uint8_t prog[] = {0xB0, 0x10, 0x04, 0x20, 0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect_eq(static_cast<uint16_t>(c.ax() & 0xFF), 0x30, "add al,20");
    }
    {
        iron86::cpu c;
        const uint8_t prog[] = {0xB8, 0x01, 0x00, 0x05, 0x02, 0x00, 0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect_eq(c.ax(), 3, "add ax,2");
        expect(!flag(c, iron86::k_flag_cf), "add ax cf");
        expect(!flag(c, iron86::k_flag_zf), "add ax zf");
    }
    {
        /* ADD AL,1 from 7F → 80: SF, OF */
        iron86::cpu c;
        const uint8_t prog[] = {0xB0, 0x7F, 0x04, 0x01, 0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect_eq(c.ax() & 0xFF, 0x80, "add al overflow val");
        expect(flag(c, iron86::k_flag_sf), "add al sf");
        expect(flag(c, iron86::k_flag_of), "add al of");
        expect(!flag(c, iron86::k_flag_cf), "add al no cf");
    }
    {
        /* CMP AX,imm + JZ taken */
        iron86::cpu c;
        const uint8_t prog[] = {0xB8, 0x00, 0x00, 0x3D, 0x00, 0x00, 0x74, 0x03, 0xB8, 0x11, 0x00,
                                0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect_eq(c.ax(), 0, "jz taken");
        expect(flag(c, iron86::k_flag_zf), "cmp zf");
    }
    {
        /* CMP AX,1 + JZ not taken */
        iron86::cpu c;
        const uint8_t prog[] = {0xB8, 0x00, 0x00, 0x3D, 0x01, 0x00, 0x74, 0x03, 0xB8, 0x22, 0x00,
                                0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect_eq(c.ax(), 0x22, "jz not taken");
        expect(!flag(c, iron86::k_flag_zf), "cmp nz");
        expect(flag(c, iron86::k_flag_cf), "cmp 0,1 cf");
    }
    {
        /* JNZ taken when AX != 0 */
        iron86::cpu c;
        const uint8_t prog[] = {0xB8, 0x01, 0x00, 0x3D, 0x00, 0x00, 0x75, 0x03, 0xB8, 0x33, 0x00,
                                0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect_eq(c.ax(), 1, "jnz taken");
    }
    {
        /* JNZ not taken */
        iron86::cpu c;
        const uint8_t prog[] = {0xB8, 0x00, 0x00, 0x3D, 0x00, 0x00, 0x75, 0x03, 0xB8, 0x44, 0x00,
                                0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect_eq(c.ax(), 0x44, "jnz not taken");
    }
    {
        /* JMP rel8 */
        iron86::cpu c;
        const uint8_t prog[] = {0xEB, 0x03, 0xB8, 0xFF, 0xFF, 0xB8, 0x07, 0x00, 0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect_eq(c.ax(), 7, "jmp rel8");
    }
    {
        /* CLI / STI */
        iron86::cpu c;
        const uint8_t prog[] = {0xFB, 0xFA, 0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect(!flag(c, iron86::k_flag_if), "cli");
    }
    {
        /* JMP FAR must fetch CS:IP from the instruction, not the target. */
        iron86::cpu c;
        uint8_t prog[0x110];
        std::memset(prog, 0x90, sizeof(prog));
        prog[0] = 0xEA;
        prog[1] = 0x00;
        prog[2] = 0x02; /* IP */
        prog[3] = 0x00;
        prog[4] = 0x10; /* CS = 1000h */
        prog[0x100] = 0xB8;
        prog[0x101] = 0x77;
        prog[0x102] = 0x00;
        prog[0x103] = 0xF4;
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c, 16);
        expect(c.halted(), "jmp far hlt");
        expect_eq(c.ax(), 0x0077, "jmp far ax");
        expect_eq(c.cs(), 0x1000, "jmp far cs");
        expect_eq(c.ip(), 0x0204, "jmp far ip after hlt");
    }
    {
        /* RETF */
        iron86::cpu c;
        const uint8_t prog[] = {0xB8, 0x00, 0x10, 0x50, 0xB8, 0x0C, 0x01, 0x50, 0xCB,
                                0xB8, 0xFF, 0xFF, 0xB8, 0x42, 0x00, 0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect_eq(c.ax(), 0x0042, "retf ax");
        expect_eq(c.cs(), 0x1000, "retf cs");
    }
    {
        /* CLD / STD */
        iron86::cpu c;
        const uint8_t prog[] = {0xFD, 0xFC, 0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect(!flag(c, iron86::k_flag_df), "cld");
    }
    {
        /* LOOP */
        iron86::cpu c;
        const uint8_t prog[] = {0xB9, 0x03, 0x00, 0xB8, 0x00, 0x00, 0x40, 0xE2, 0xFD, 0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect_eq(c.ax(), 3, "loop ax");
        expect_eq(c.cx(), 0, "loop cx");
    }
    {
        /* DIV r16 */
        iron86::cpu c;
        const uint8_t prog[] = {0xB8, 0x2A, 0x00, 0x31, 0xD2, 0xB9, 0x09, 0x00, 0xF7, 0xF1, 0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect_eq(c.ax(), 4, "div quot");
        expect_eq(c.dx(), 6, "div rem");
    }
    {
        /* ADD AX,imm8 via 83 /0 */
        iron86::cpu c;
        const uint8_t prog[] = {0xB8, 0x00, 0x01, 0x83, 0xC0, 0x10, 0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect_eq(c.ax(), 0x0110, "add ax,10h");
    }
    {
        /* MOV AX, moffs16 */
        iron86::cpu c;
        const uint8_t prog[] = {0xA1, 0x00, 0x02, 0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        c.mem_write16(iron86::cpu::phys(0x1000, 0x0200), 0xCAFE);
        run(c);
        expect_eq(c.ax(), 0xCAFE, "a1 moffs");
    }
    {
        /* MOV r/m16, imm16 */
        iron86::cpu c;
        const uint8_t prog[] = {0xC7, 0x06, 0x00, 0x02, 0x34, 0x12, 0xA1, 0x00, 0x02, 0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect_eq(c.ax(), 0x1234, "c7 imm");
    }
    {
        /* PUSH ES / POP DS */
        iron86::cpu c;
        const uint8_t prog[] = {0xB8, 0x78, 0x56, 0x8E, 0xC0, 0x06, 0x1F, 0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect_eq(c.ds(), 0x5678, "push es / pop ds");
    }
    {
        /* SHL BX,1 */
        iron86::cpu c;
        const uint8_t prog[] = {0xBB, 0x01, 0x00, 0xD1, 0xE3, 0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect_eq(c.bx(), 2, "shl bx,1");
    }
    {
        /* LES BX, [BX] */
        iron86::cpu c;
        const uint8_t prog[] = {0xBB, 0x00, 0x02, 0xC7, 0x07, 0x34, 0x12, 0xC7, 0x47, 0x02,
                                0x00, 0x10, 0xC4, 0x1F, 0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect_eq(c.bx(), 0x1234, "les off");
        expect_eq(c.es(), 0x1000, "les seg");
    }
    {
        /* REP STOSW */
        iron86::cpu c;
        const uint8_t prog[] = {0xB8, 0x00, 0x00, 0xBF, 0x00, 0x02, 0xB9, 0x04, 0x00, 0xFC,
                                0xF3, 0xAB, 0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        c.mem_write16(iron86::cpu::phys(0x1000, 0x0200), 0xFFFF);
        run(c);
        expect_eq(c.mem_read16(iron86::cpu::phys(0x1000, 0x0200)), 0, "stosw0");
        expect_eq(c.mem_read16(iron86::cpu::phys(0x1000, 0x0206)), 0, "stosw3");
        expect_eq(c.di(), 0x0208, "stosw di");
        expect_eq(c.cx(), 0, "stosw cx");
    }
    {
        /* CLC / STC */
        iron86::cpu c;
        const uint8_t prog[] = {0xF9, 0xF8, 0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        run(c);
        expect(!flag(c, iron86::k_flag_cf), "clc");
    }
    {
        /* CS: MOV AX, moffs16 */
        iron86::cpu c;
        const uint8_t prog[] = {0x2E, 0xA1, 0x00, 0x02, 0xF4};
        c.load_com(prog, sizeof(prog), 0x1000);
        c.mem_write16(iron86::cpu::phys(0x1000, 0x0200), 0xBEEF);
        run(c);
        expect_eq(c.ax(), 0xBEEF, "cs:a1 moffs");
        expect(c.halted(), "cs:a1 hlt");
    }

    if (g_fail != 0)
    {
        return 1;
    }
    std::puts("iron86 tests ok");
    return 0;
}
