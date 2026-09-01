/**
 * @file test_pit.cpp
 * @brief 8253/8259 IRQ0 edge after EOI, and a guest ISR that waits for two ticks.
 */

#include "dos/pic8259.h"
#include "dos/pit8253.h"
#include "rex/rex.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

TEST_CASE("PIT mode 3 re-asserts IRQ0 after ack and EOI")
{
    pic8259 pic{};
    pit8253 pit{pic};
    unsigned seen = 0;
    unsigned t = 0;

    pic.pc_boot_state();
    pit.pc_boot_state();
    pit.write(3, 0x36); /* ch0 mode 3, lobyte/hibyte */
    pit.write(0, 32);
    pit.write(0, 0);

    for (t = 0; t < 10000u; t++)
    {
        pit.tick();
        if (pic.pending())
        {
            REQUIRE(pic.ack_vector() == 0x08);
            pic.write_command(0x20); /* EOI */
            seen++;
            if (seen == 2u)
            {
                break;
            }
        }
    }
    REQUIRE(seen == 2u);
}

TEST_CASE("PIT mode 3 still pulses when IRQ0 pin was left high")
{
    pic8259 pic{};
    pit8253 pit{pic};
    unsigned seen = 0;
    unsigned t = 0;

    pic.pc_boot_state();
    pit.pc_boot_state();
    pit.write(3, 0x36);
    pit.write(0, 32);
    pit.write(0, 0);
    /* Reproduce the Bushido freeze: pin high, IRR empty, after the first EOI. */
    pic.assert_irq(0);
    (void)pic.ack_vector();
    pic.write_command(0x20);
    REQUIRE_FALSE(pic.pending());

    for (t = 0; t < 10000u; t++)
    {
        pit.tick();
        if (pic.pending())
        {
            seen++;
            REQUIRE(pic.ack_vector() == 0x08);
            pic.write_command(0x20);
            if (seen == 2u)
            {
                break;
            }
        }
    }
    REQUIRE(seen == 2u);
}

TEST_CASE("guest IRQ0 ISR runs at least twice then INT20")
{
    rex_session *s = rex_session_create();
    uint8_t ctr[2] = {0, 0};

    REQUIRE(s != nullptr);
    REQUIRE(rex_session_load(s, "tests/fixtures/irq0.com", nullptr) == REX_OK);
    REQUIRE(rex_session_run(s, 200000) == REX_OK);
    REQUIRE(rex_session_halted(s));
    REQUIRE(rex_session_exit_code(s) == 0);
    /* COM CS=1000h, org 100h; counter label is at 013Eh (nasm listing). */
    REQUIRE(rex_session_read_mem(s, 0x1013Eull, ctr, 2) == REX_OK);
    REQUIRE(ctr[0] >= 2);
    rex_session_destroy(s);
}
