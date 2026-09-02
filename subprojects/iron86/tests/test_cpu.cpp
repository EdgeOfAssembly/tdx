/**
 * @file test_cpu.cpp
 * @brief iron86 CPU smoke: MOV AX,1 / INC AX / HLT and INT 20h via IVT.
 */
#include "iron86/cpu.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

static int g_fail = 0;

static void expect(bool ok, const char *msg)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL %s\n", msg);
        g_fail = 1;
    }
}

int main()
{
    {
        iron86::cpu c;
        const uint8_t prog[] = {0xB8, 0x01, 0x00, 0x40, 0xF4}; /* mov ax,1; inc ax; hlt */
        c.load_com(prog, sizeof(prog), 0x1000);
        while (!c.halted())
        {
            if (!c.step())
            {
                break;
            }
        }
        expect(c.halted(), "hlt");
        expect(c.ax() == 2, "ax==2");
        expect(c.cs() == 0x1000, "cs");
    }
    {
        iron86::cpu c;
        const uint8_t prog[] = {0xB8, 0x01, 0x00, 0xCD, 0x20}; /* mov ax,1; int 20 */
        c.mem_write8(iron86::cpu::phys(0xF000, 0), 0xF4);
        c.set_ivt(0x20, 0xF000, 0);
        c.load_com(prog, sizeof(prog), 0x1000);
        while (!c.halted())
        {
            if (!c.step())
            {
                break;
            }
        }
        expect(c.halted(), "int20 hlt");
        expect(c.ax() == 1, "ax==1 after int20");
        expect(c.cs() == 0xF000, "cs at stub");
    }
    if (g_fail != 0)
    {
        return 1;
    }
    std::puts("iron86 tests ok");
    return 0;
}
