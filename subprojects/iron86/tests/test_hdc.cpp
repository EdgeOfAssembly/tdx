/**
 * @file test_hdc.cpp
 * @brief Xebec HDC: IBM CHS LBA, opcode 05, PIO persist, DMA3.
 */
#include "iron86/chs_lba.h"
#include "iron86/hdc.h"
#include "iron86/pc.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <sys/stat.h>
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

static void expect_eq(unsigned got, unsigned want, const char *msg)
{
    if (got != want)
    {
        std::fprintf(stderr, "FAIL %s got=%u want=%u\n", msg, got, want);
        g_fail = 1;
    }
}

static void expect_eq_u32(uint32_t got, uint32_t want, const char *msg)
{
    if (got != want)
    {
        std::fprintf(stderr, "FAIL %s got=%u want=%u\n", msg, got, want);
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

static bool write_zero_img(const char *path, uint32_t nsec)
{
    std::vector<uint8_t> z(static_cast<size_t>(nsec) * iron86::k_hdc_sector_size, 0);
    std::ofstream f(path, std::ios::binary);
    if (!f)
    {
        return false;
    }
    f.write(reinterpret_cast<const char *>(z.data()), static_cast<std::streamsize>(z.size()));
    return static_cast<bool>(f);
}

static void send_dcb(iron86::hdc_xebec &h, const uint8_t dcb[6])
{
    h.write_port(iron86::k_hdc_port_select, 0);
    uint8_t i = 0;
    for (; i < 6u; i++)
    {
        h.write_port(iron86::k_hdc_port_data, dcb[i]);
    }
}

static uint8_t wait_completion(iron86::hdc_xebec &h)
{
    uint8_t st = 0;
    int i = 0;
    for (; i < 8; i++)
    {
        st = h.read_port(iron86::k_hdc_port_status);
        if ((st & (iron86::k_hdc_stat_req | iron86::k_hdc_stat_io | iron86::k_hdc_stat_cd)) ==
            (iron86::k_hdc_stat_req | iron86::k_hdc_stat_io | iron86::k_hdc_stat_cd))
        {
            return h.read_port(iron86::k_hdc_port_data);
        }
    }
    std::fprintf(stderr, "FAIL no completion last st=%02X\n", st);
    g_fail = 1;
    return 0xFF;
}

static bool file_exists(const char *path)
{
    struct stat st{};
    return (path != nullptr) && (stat(path, &st) == 0) && S_ISREG(st.st_mode);
}

int main()
{
    expect_eq_u32(iron86_hdc_chs_to_lba(0, 4, 0, 17, 0), 0u, "lba 0/0/0");
    expect_eq_u32(iron86_hdc_chs_to_lba(0, 4, 0, 17, 16), 16u, "lba 0/0/16");
    expect_eq_u32(iron86_hdc_chs_to_lba(0, 4, 1, 17, 0), 17u, "lba 0/1/0");
    expect_eq_u32(iron86_hdc_chs_to_lba(1, 4, 0, 17, 0), 68u, "lba 1/0/0");
    expect_eq_u32(iron86_hdc_chs_to_lba(305, 4, 3, 17, 16), 20807u, "lba last Type-1");
    expect_eq_u32(iron86_hdc_chs_to_lba(0, 4, 0, 17, 0),
                  iron86_chs_to_lba(0, 4, 0, 17, 1), "dcb0 vs int13 s=1");
    {
        const uint32_t cyl = 50;
        const uint32_t head = 2;
        const uint32_t s0 = 8;
        expect_eq_u32(iron86_hdc_chs_to_lba(cyl, 4, head, 17, s0),
                      ((cyl * 4u) + head) * 17u + s0, "306x4x17 formula");
    }

    const char *img = "/tmp/iron86-hdc-unit.img";
    const uint32_t cyls = 2;
    const uint32_t heads = 2;
    const uint32_t spt = 17;
    expect(write_zero_img(img, cyls * heads * spt), "write img");

    {
        iron86::hdc_xebec h;
        expect(h.attach(img, cyls, heads, spt), "attach");
        expect_eq_u32(h.cylinders(), cyls, "cyl");
        expect_eq_u32(h.heads(), heads, "hd");
        expect_eq_u32(h.spt(), spt, "spt");
        h.write_port(iron86::k_hdc_port_mask, 0);
        const uint8_t wr[6] = {0x0A, 0x00, 0x00, 0x00, 0x01, 0x05};
        send_dcb(h, wr);
        uint8_t pattern[512];
        uint32_t i = 0;
        for (; i < 512u; i++)
        {
            pattern[i] = static_cast<uint8_t>(i ^ 0xA5u);
            h.write_port(iron86::k_hdc_port_data, pattern[i]);
        }
        const uint8_t stw = wait_completion(h);
        expect((stw & 0x02u) == 0, "write completion");

        const uint8_t rd[6] = {0x08, 0x00, 0x00, 0x00, 0x01, 0x05};
        send_dcb(h, rd);
        uint8_t got[512];
        for (i = 0; i < 512u; i++)
        {
            got[i] = h.read_port(iron86::k_hdc_port_data);
        }
        const uint8_t str = wait_completion(h);
        expect((str & 0x02u) == 0, "read completion");
        expect(std::memcmp(got, pattern, 512) == 0, "pio readback");
        expect(std::memcmp(h.image().data(), pattern, 512) == 0, "image[0]");
    }

    {
        iron86::hdc_xebec h2;
        expect(h2.attach(img, cyls, heads, spt), "reopen");
        const uint8_t rd[6] = {0x08, 0x00, 0x00, 0x00, 0x01, 0x05};
        send_dcb(h2, rd);
        uint8_t got[512];
        uint32_t i = 0;
        for (; i < 512u; i++)
        {
            got[i] = h2.read_port(iron86::k_hdc_port_data);
        }
        (void)wait_completion(h2);
        expect(got[0] == static_cast<uint8_t>(0 ^ 0xA5u), "persist byte0");
        expect(got[1] == static_cast<uint8_t>(1 ^ 0xA5u), "persist byte1");
        expect(got[511] == static_cast<uint8_t>(511u ^ 0xA5u), "persist last");
    }

    {
        iron86::hdc_xebec h;
        expect(h.attach(img, cyls, heads, spt), "attach verify");
        const uint8_t vf[6] = {0x05, 0x00, 0x00, 0x00, 0x01, 0x05};
        send_dcb(h, vf);
        const uint8_t st = wait_completion(h);
        expect((st & 0x02u) == 0, "opcode 05 verify ok");
        const uint8_t vf2[6] = {0x05, 0x00, 0x10, 0x00, 0x01, 0x05}; /* sector 16 */
        send_dcb(h, vf2);
        const uint8_t st2 = wait_completion(h);
        expect((st2 & 0x02u) == 0, "verify last spt-1");
        const uint8_t bad[6] = {0x05, 0x00, 0x11, 0x00, 0x01, 0x05}; /* sector 17 illegal */
        send_dcb(h, bad);
        const uint8_t stb = wait_completion(h);
        expect((stb & 0x02u) != 0, "verify illegal sector errors");
    }

    {
        iron86::hdc_xebec h;
        expect(h.attach(img, cyls, heads, spt), "attach ready");
        const uint8_t dcb[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        send_dcb(h, dcb);
        const uint8_t st0 = h.read_port(iron86::k_hdc_port_status);
        expect((st0 & iron86::k_hdc_stat_req) == 0, "post-DCB REQ clear");
        const uint8_t st = wait_completion(h);
        expect((st & 0x02u) == 0, "test ready");
    }

    {
        iron86::hdc_xebec h;
        expect(h.attach(img, cyls, heads, spt), "attach init");
        const uint8_t dcb[6] = {0x0C, 0x00, 0x00, 0x00, 0x00, 0x00};
        send_dcb(h, dcb);
        const uint8_t st0 = h.read_port(iron86::k_hdc_port_status);
        expect((st0 & iron86::k_hdc_stat_req) == 0, "0Ch post-DCB REQ clear");
        const uint8_t st1 = h.read_port(iron86::k_hdc_port_status);
        expect((st1 & iron86::k_hdc_stat_req) != 0, "0Ch data-out REQ");
        expect((st1 & iron86::k_hdc_stat_io) == 0, "0Ch IO=0");
        expect((st1 & iron86::k_hdc_stat_cd) == 0, "0Ch CD=0");
        const uint8_t params[8] = {0x01, 0x32, 0x04, 0x01, 0x32, 0x00, 0x00, 0x0B};
        uint8_t i = 0;
        for (; i < 8u; i++)
        {
            h.write_port(iron86::k_hdc_port_data, params[i]);
        }
        const uint8_t st = wait_completion(h);
        expect((st & 0x02u) == 0, "0Ch completion");
        expect_eq_u32(h.cylinders(), 306u, "0Ch cyl 306");
        expect_eq_u32(h.heads(), 4u, "0Ch heads 4");
    }

    {
        iron86::hdc_xebec h;
        expect(h.attach(img, cyls, heads, spt), "attach illegal");
        const uint8_t dcb[6] = {0x08, 0x1F, 0x00, 0x00, 0x01, 0x05}; /* head 31 */
        send_dcb(h, dcb);
        const uint8_t st = wait_completion(h);
        expect((st & 0x02u) != 0, "illegal head error");
        const uint8_t sense_cmd[6] = {0x03, 0x00, 0x00, 0x00, 0x00, 0x00};
        send_dcb(h, sense_cmd);
        uint8_t sense[4];
        uint8_t i = 0;
        for (; i < 4u; i++)
        {
            (void)h.read_port(iron86::k_hdc_port_status);
            sense[i] = h.read_port(iron86::k_hdc_port_data);
        }
        expect((sense[0] & 0x0Fu) == 0x01u, "sense illegal address");
        expect((sense[0] & 0x30u) == 0x20u, "sense type 2");
    }

    {
        iron86::hdc_xebec h;
        expect(h.attach(img, cyls, heads, spt), "attach fmt");
        const uint8_t dcb[6] = {0x06, 0x00, 0x00, 0x00, 0x01, 0x05};
        send_dcb(h, dcb);
        const uint8_t st = wait_completion(h);
        expect((st & 0x02u) == 0, "format track");
        expect(h.image()[0] == 0xF6u, "format 0xF6");
        expect(h.image()[16u * 512u] == 0xF6u, "format track last sec");
    }

    {
        /* Through pc ports + DMA channel 3. */
        iron86::pc p;
        p.wire_pc_hw();
        expect(p.attach_hdc_path(img, cyls, heads, spt), "pc attach");
        p.hdc().write_port(iron86::k_hdc_port_mask, 0);
        const uint8_t wr[6] = {0x0A, 0x00, 0x00, 0x00, 0x01, 0x05};
        send_dcb(p.hdc(), wr);
        uint32_t i = 0;
        for (; i < 512u; i++)
        {
            p.hdc().write_port(iron86::k_hdc_port_data, 0x5Au);
        }
        (void)wait_completion(p.hdc());

        const uint8_t dma_prog[] = {
            0xB0, 0x00, 0xE6, 0x0C, /* OUT 0Ch,0 clear FF */
            0xB0, 0x00, 0xE6, 0x06, /* ch3 addr 9000 */
            0xB0, 0x90, 0xE6, 0x06, 0xB0, 0xFF, 0xE6, 0x07, /* count 511 */
            0xB0, 0x01, 0xE6, 0x07, 0xB0, 0x00, 0xE6, 0x82, /* page 0 */
            0xB0, 0x03, 0xE6, 0x0A,                         /* unmask ch3 */
            0xF4,
        };
        p.c.load_com(dma_prog, sizeof(dma_prog), 0x1000);
        run(p.c, 64);
        p.hdc().write_port(iron86::k_hdc_port_mask, 0x01); /* DMA, no IRQ */
        const uint8_t rd[6] = {0x08, 0x00, 0x00, 0x00, 0x01, 0x05};
        send_dcb(p.hdc(), rd);
        (void)wait_completion(p.hdc());
        expect_eq(p.c.mem_read8(0x9000u), 0x5A, "dma3 byte0");
        expect_eq(p.c.mem_read8(0x9000u + 511u), 0x5A, "dma3 last");
    }

    {
        /* COM program: Test Ready via 320h–323h. */
        iron86::pc p;
        p.wire_pc_hw();
        expect(p.attach_hdc_path(img, cyls, heads, spt), "pc ready attach");
        const uint8_t prog[] = {
            0xBA, 0x23, 0x03, 0xB0, 0x00, 0xEE, /* OUT 323h,0 */
            0xBA, 0x22, 0x03, 0xEE,             /* OUT 322h select */
            0xBA, 0x20, 0x03, 0xB0, 0x00, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, /* DCB 00 */
            0xBA, 0x21, 0x03, 0xEC, 0xEC,       /* two status polls */
            0xBA, 0x20, 0x03, 0xEC, 0xF4,       /* completion */
        };
        p.c.load_com(prog, sizeof(prog), 0x1000);
        run(p.c, 64);
        expect((p.c.ax() & 0x02u) == 0, "COM test ready");
    }

    if (file_exists(iron86::k_hdc_ros_default))
    {
        const char *t1 = "/tmp/iron86-hdc-type1.img";
        expect(write_zero_img(t1, iron86::k_hdc_cyl_xt * iron86::k_hdc_heads_xt *
                                      iron86::k_hdc_spt_xt),
               "type1 img");
        iron86::pc p;
        p.wire_pc_hw();
        expect(p.attach_hdc_path(t1), "type1 attach");
        expect(p.load_hdc_rom_path(iron86::k_hdc_ros_default), "ros load");
        expect_eq(p.c.mem_read8(iron86::k_hdc_ros_phys), 0x55, "ros 55");
        expect_eq(p.c.mem_read8(iron86::k_hdc_ros_phys + 1u), 0xAA, "ros AA");
        p.c.set_ivt(0x13, 0xF000, 0x1000);
        p.c.mem_write8(0xF1000u, 0xCF); /* IRET floppy / INT 40 stub */
        /* IRQ0: bump BDA 40:6C and EOI (ROS timeout uses the timer). */
        const uint8_t irq0[] = {
            0x1E, 0x50, 0xB8, 0x40, 0x00, 0x8E, 0xD8, 0xFF, 0x06, 0x6C, 0x00,
            0xB0, 0x20, 0xE6, 0x20, 0x58, 0x1F, 0xCF,
        };
        uint32_t bi = 0;
        for (; bi < sizeof(irq0); bi++)
        {
            p.c.mem_write8(0xF1010u + bi, irq0[bi]);
        }
        p.c.set_ivt(8, 0xF000, 0x1010);
        {
            uint8_t irq = 9;
            for (; irq < 16u; irq++)
            {
                p.c.set_ivt(irq, 0xF000, 0x1000);
            }
        }
        p.c.mem_write8(0x472u, 0x34);
        p.c.mem_write8(0x473u, 0x12);
        /* PIC ICW + CALL FAR C800:0003 + HLT. load_com clears halted. */
        const uint8_t init[] = {
            0xB0, 0x13, 0xE6, 0x20, 0xB0, 0x08, 0xE6, 0x21, 0xB0, 0x09, 0xE6, 0x21,
            0xB0, 0xFF, 0xE6, 0x21, 0x9A, 0x03, 0x00, 0x00, 0xC8, 0xF4,
        };
        p.c.load_com(init, sizeof(init), 0x0800);
        p.c.set_flags(0x0002);
        run(p.c, 500000);
        expect(p.c.halted(), "ros init halted");
        expect_eq(p.c.mem_read16(0x13u * 4u + 2u), iron86::k_hdc_ros_seg, "INT13 CS C800");
        expect(p.hdc().image()[0] != 0xF6, "ROS E4 diag did not Format Drive");
        /* INT 13 AH=03 write CHS 0/0/1 then AH=02 read. */
        uint32_t i = 0;
        for (; i < 512u; i++)
        {
            p.c.mem_write8(0x9000u + i, static_cast<uint8_t>((i * 3u) + 1u));
        }
        const uint8_t wr13[] = {
            0xFB, 0xB8, 0x01, 0x03, 0xBB, 0x00, 0x90, 0xB9, 0x01, 0x00, 0xBA, 0x80, 0x00,
            0xCD, 0x13, 0xF4,
        };
        p.c.load_com(wr13, sizeof(wr13), 0x0800);
        p.c.set_es(0);
        run(p.c, 200000);
        expect((p.c.flags() & iron86::k_flag_cf) == 0, "INT13 write CF");
        for (i = 0; i < 512u; i++)
        {
            p.c.mem_write8(0x9000u + i, 0);
        }
        const uint8_t rd13[] = {
            0xFB, 0xB8, 0x01, 0x02, 0xBB, 0x00, 0x90, 0xB9, 0x01, 0x00, 0xBA, 0x80, 0x00,
            0xCD, 0x13, 0xF4,
        };
        p.c.load_com(rd13, sizeof(rd13), 0x0800);
        p.c.set_es(0);
        run(p.c, 200000);
        expect((p.c.flags() & iron86::k_flag_cf) == 0, "INT13 read CF");
        expect_eq(p.c.mem_read8(0x9000u), 1, "INT13 data0");
        expect_eq(p.c.mem_read8(0x9001u), 4, "INT13 data1");
        expect(p.hdc().image()[1] == 4, "image persist INT13");
        (void)std::remove(t1);
    }

    {
        /* Class-7 E4h must not run opcode 04 Format Drive. */
        const char *e4p = "/tmp/iron86-hdc-e4.img";
        expect(write_zero_img(e4p, 32), "e4 img");
        iron86::hdc_xebec h;
        expect(h.attach(e4p, 1, 1, 17), "e4 attach");
        const uint8_t e4[6] = {0xE4, 0, 0, 0, 0, 0};
        send_dcb(h, e4);
        expect(h.image()[0] == 0, "E4 did not fill 0xF6");
        (void)std::remove(e4p);
    }

    (void)std::remove(img);
    if (g_fail != 0)
    {
        return 1;
    }
    std::puts("iron86 hdc tests ok");
    return 0;
}
