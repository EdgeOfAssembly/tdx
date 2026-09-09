/**
 * @file verify_chs_lba.c
 * @brief CBMC: LBA = ((cyl * heads) + head) * spt + (sec - 1).
 *
 * Nested loops over a 4×2×9 box (unwind 16) plus a few exact IBM identities.
 */
#include "iron86/chs_lba.h"

#include <assert.h>
#include <stdint.h>

int main(void)
{
    uint32_t cyl = 0;
    uint32_t head = 0;
    uint32_t sec = 0;
    const uint32_t heads = 2u;
    const uint32_t spt = 9u;

    assert(iron86_chs_to_lba(50u, 2u, 0u, 9u, 1u) == 900u);
    assert(iron86_chs_to_lba(0u, 1u, 0u, 9u, 9u) == 8u);
    assert(iron86_chs_to_lba(0u, 2u, 1u, 8u, 1u) == 8u);
    assert(iron86_chs_to_lba(79u, 2u, 1u, 18u, 18u) == 2879u);
    assert(iron86_chs_to_lba(0u, 2u, 0u, 15u, 15u) == 14u);

    for (cyl = 0; cyl < 4u; cyl++)
    {
        for (head = 0; head < heads; head++)
        {
            for (sec = 1u; sec <= spt; sec++)
            {
                const uint32_t lba = iron86_chs_to_lba(cyl, heads, head, spt, sec);
                const uint32_t want = ((cyl * heads) + head) * spt + (sec - 1u);
                assert(lba == want);
            }
        }
    }
    return 0;
}
