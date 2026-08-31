/**
 * @file verify_mz.c
 * @brief CBMC harness for mz_parse_header (bounded 32-byte prefix).
 */

#include "dos/mz_parse.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

unsigned char nondet_uchar(void);

int main(void)
{
    uint8_t buf[32];
    mz_info info;
    size_t i = 0;
    int r = 0;

    for (i = 0; i < sizeof(buf); i++)
    {
        buf[i] = nondet_uchar();
    }
    memset(&info, 0xAA, sizeof(info));
    r = mz_parse_header(buf, sizeof(buf), 32u, &info);
    if (r == 0)
    {
        assert(info.is_mz == 1);
        assert(info.header_paras != 0);
        assert(info.header_bytes == (uint32_t)info.header_paras * 16u);
        assert(info.header_bytes <= 32u);
    }
    else if (r == 1)
    {
        assert(info.is_mz == 0);
    }
    return 0;
}
