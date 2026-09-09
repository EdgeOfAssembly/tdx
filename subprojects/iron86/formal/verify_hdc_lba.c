/**
 * @file verify_hdc_lba.c
 * @brief CBMC: Xebec DCB LBA = ((cyl * heads) + head) * spt + sector0.
 *
 * Sector is 0-based (IBM ROS DEC CL). Type-1 identities plus a 3×2×4 box.
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
    const uint32_t spt = 4u;

    assert(iron86_hdc_chs_to_lba(0u, 4u, 0u, 17u, 0u) == 0u);
    assert(iron86_hdc_chs_to_lba(0u, 4u, 0u, 17u, 16u) == 16u);
    assert(iron86_hdc_chs_to_lba(0u, 4u, 1u, 17u, 0u) == 17u);
    assert(iron86_hdc_chs_to_lba(1u, 4u, 0u, 17u, 0u) == 68u);
    assert(iron86_hdc_chs_to_lba(305u, 4u, 3u, 17u, 16u) == 20807u);
    assert(iron86_hdc_chs_to_lba(0u, 4u, 0u, 17u, 0u) == iron86_chs_to_lba(0u, 4u, 0u, 17u, 1u));

    for (cyl = 0; cyl < 3u; cyl++)
    {
        for (head = 0; head < heads; head++)
        {
            for (sec = 0; sec < spt; sec++)
            {
                const uint32_t lba = iron86_hdc_chs_to_lba(cyl, heads, head, spt, sec);
                const uint32_t want = ((cyl * heads) + head) * spt + sec;
                assert(lba == want);
            }
        }
    }
    return 0;
}
