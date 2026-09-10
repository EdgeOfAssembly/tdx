/**
 * @file hdc.cpp
 * @brief IBM XT Xebec HDC: CHS DCB, SASI handshake, DMA3, image persist.
 */
#include "iron86/hdc.h"
#include "iron86/chs_lba.h"
#include "iron86/cpu.h"
#include "iron86/hw.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

namespace iron86
{

hdc_xebec::hdc_xebec()
    : cpu_(nullptr), dma_(nullptr), fp_(nullptr), image_(), path_(), cylinders_(k_hdc_cyl_xt),
      heads_(k_hdc_heads_xt), spt_(k_hdc_spt_xt), sector_count_(0), phase_(phase::idle),
      status_(0), dma_irq_(0), dcb_{}, dcb_n_(0), completion_(0), sense_{}, sense_idx_(0), buf_(),
      buf_pos_(0), cur_cyl_{}, selected_(false), write_lba_(0), post_dcb_status_reads_(0),
      defer_completion_(false), defer_sense_(false), defer_init_(false), init_params_{}, init_n_(0),
      attached_(false), irq_latched_(false), last_cyl_(0), last_head_(0), last_sec_(0),
      last_drive_(0)
{
}

hdc_xebec::~hdc_xebec()
{
    close_file();
}

void hdc_xebec::close_file()
{
    if (fp_ != nullptr)
    {
        std::fflush(fp_);
        std::fclose(fp_);
        fp_ = nullptr;
    }
}

void hdc_xebec::bind_bus(cpu *c, dma8237 *dma)
{
    cpu_ = c;
    dma_ = dma;
}

bool hdc_xebec::attach(const char *path, uint32_t cylinders, uint32_t heads, uint32_t spt)
{
    if ((path == nullptr) || (path[0] == '\0') || (cylinders == 0) || (heads == 0) || (spt == 0))
    {
        return false;
    }
    close_file();
    /* One fopen + fread; istreambuf_iterator is per-byte and stalls ASAN on 10 MiB. */
    std::FILE *fp = std::fopen(path, "r+b");
    if (fp == nullptr)
    {
        return false;
    }
    {
        struct stat st{};
        if ((::fstat(::fileno(fp), &st) != 0) || S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode))
        {
            std::fclose(fp);
            return false;
        }
    }
    if (std::fseek(fp, 0, SEEK_END) != 0)
    {
        std::fclose(fp);
        return false;
    }
    const long sz = std::ftell(fp);
    if (sz < 0)
    {
        std::fclose(fp);
        return false;
    }
    std::rewind(fp);
    cylinders_ = cylinders;
    heads_ = heads;
    spt_ = spt;
    sector_count_ = cylinders * heads * spt;
    const uint32_t need = sector_count_ * k_hdc_sector_size;
    const size_t file_n = static_cast<size_t>(sz);
    const size_t load_n =
        (file_n < static_cast<size_t>(need)) ? file_n : static_cast<size_t>(need);
    image_.assign(static_cast<size_t>(need), 0);
    if (load_n > 0u)
    {
        if (std::fread(image_.data(), 1, load_n, fp) != load_n)
        {
            image_.clear();
            std::fclose(fp);
            return false;
        }
    }
    fp_ = fp;
    path_ = path;
    attached_ = true;
    cur_cyl_[0] = 0;
    cur_cyl_[1] = 0;
    dma_irq_ = 0;
    set_idle();
    return true;
}

void hdc_xebec::reset()
{
    clear_irq();
    set_idle();
    dma_irq_ = 0;
    cur_cyl_[0] = 0;
    cur_cyl_[1] = 0;
}

void hdc_xebec::set_idle()
{
    phase_ = phase::idle;
    status_ = 0;
    dcb_n_ = 0;
    selected_ = false;
    defer_completion_ = false;
    defer_sense_ = false;
    defer_init_ = false;
    post_dcb_status_reads_ = 0;
    init_n_ = 0;
}

void hdc_xebec::raise_irq()
{
    irq_latched_ = true;
    status_ = static_cast<uint8_t>(status_ | k_hdc_stat_irq);
}

void hdc_xebec::clear_irq()
{
    irq_latched_ = false;
    status_ = static_cast<uint8_t>(status_ & static_cast<uint8_t>(~k_hdc_stat_irq));
}

bool hdc_xebec::irq_line() const
{
    return irq_latched_ && ((dma_irq_ & 0x02u) != 0);
}

void hdc_xebec::fail(uint8_t error_type, uint8_t error_code, uint8_t drive)
{
    sense_[0] = static_cast<uint8_t>(0x80u | (static_cast<uint8_t>((error_type & 3u) << 4) |
                                             (error_code & 0x0Fu)));
    sense_[1] = static_cast<uint8_t>(static_cast<uint8_t>((drive & 1u) << 5) |
                                     static_cast<uint8_t>(last_head_ & 0x1Fu));
    sense_[2] = static_cast<uint8_t>(static_cast<uint8_t>((last_cyl_ & 0x300u) >> 2) |
                                     static_cast<uint8_t>(last_sec_ & 0x3Fu));
    sense_[3] = static_cast<uint8_t>(last_cyl_ & 0xFFu);
    completion_ = static_cast<uint8_t>(0x02u | static_cast<uint8_t>((drive & 1u) << 5));
    arm_completion();
}

void hdc_xebec::ok(uint8_t drive)
{
    sense_[0] = 0;
    sense_[1] = 0;
    sense_[2] = 0;
    sense_[3] = 0;
    completion_ = static_cast<uint8_t>((drive & 1u) << 5);
    arm_completion();
}

void hdc_xebec::arm_completion()
{
    /* IBM ROS: TEST AL,1 (REQ) must read clear once, then WAIT_INT bit5. */
    phase_ = phase::send_status;
    status_ = static_cast<uint8_t>(k_hdc_stat_bsy | k_hdc_stat_cd);
    defer_completion_ = true;
    post_dcb_status_reads_ = 0;
}

uint8_t hdc_xebec::switch_nibble()
{
    /* Full low nibble: ROS Type-1-ish (306/4). Exact DIP map is poorly OCR'd. */
    return 0x0Fu;
}

void hdc_xebec::write_port(uint16_t port, uint8_t value)
{
    const uint16_t p = static_cast<uint16_t>(port & 0xFFFFu);
    const uint8_t v = value;
    if (p == k_hdc_port_probe)
    {
        return;
    }
    if (p == k_hdc_port_status)
    {
        reset();
        return;
    }
    if (p == k_hdc_port_select)
    {
        selected_ = true;
        dcb_n_ = 0;
        defer_completion_ = false;
        defer_sense_ = false;
        defer_init_ = false;
        post_dcb_status_reads_ = 0;
        phase_ = phase::recv_cmd;
        status_ = static_cast<uint8_t>(k_hdc_stat_req | k_hdc_stat_bsy | k_hdc_stat_cd);
        return;
    }
    if (p == k_hdc_port_mask)
    {
        dma_irq_ = static_cast<uint8_t>(v & 0x03u);
        return;
    }
    if (p == k_hdc_port_data)
    {
        write_data(v);
    }
}

uint8_t hdc_xebec::read_port(uint16_t port)
{
    const uint16_t p = static_cast<uint16_t>(port & 0xFFFFu);
    if (p == k_hdc_port_probe)
    {
        return switch_nibble();
    }
    if (p == k_hdc_port_status)
    {
        if (defer_init_)
        {
            if (post_dcb_status_reads_ == 0)
            {
                post_dcb_status_reads_ = 1;
                return k_hdc_stat_bsy;
            }
            defer_init_ = false;
            status_ = static_cast<uint8_t>(k_hdc_stat_req | k_hdc_stat_bsy);
            return status_;
        }
        if (defer_completion_ || defer_sense_)
        {
            if (post_dcb_status_reads_ == 0)
            {
                post_dcb_status_reads_ = 1;
                return static_cast<uint8_t>(k_hdc_stat_bsy | k_hdc_stat_cd);
            }
            if (defer_sense_)
            {
                defer_sense_ = false;
                status_ = static_cast<uint8_t>(k_hdc_stat_req | k_hdc_stat_io | k_hdc_stat_bsy);
                raise_irq();
            }
            else
            {
                defer_completion_ = false;
                status_ = static_cast<uint8_t>(k_hdc_stat_req | k_hdc_stat_io | k_hdc_stat_cd |
                                               k_hdc_stat_bsy);
                raise_irq();
            }
        }
        uint8_t ret = status_;
        if (((dma_irq_ & 0x01u) != 0) &&
            ((phase_ == phase::data_in) || (phase_ == phase::data_out)))
        {
            ret = static_cast<uint8_t>(ret | k_hdc_stat_drq);
        }
        return ret;
    }
    if (p == k_hdc_port_data)
    {
        return read_data();
    }
    if (p == k_hdc_port_select)
    {
        return switch_nibble();
    }
    return 0xFFu;
}

void hdc_xebec::write_data(uint8_t value)
{
    if (phase_ == phase::recv_cmd)
    {
        if (dcb_n_ < 6u)
        {
            dcb_[dcb_n_] = value;
            dcb_n_ = static_cast<uint8_t>(dcb_n_ + 1u);
        }
        if (dcb_n_ >= 6u)
        {
            exec_dcb();
        }
        return;
    }
    if (phase_ == phase::recv_init)
    {
        defer_init_ = false;
        if (init_n_ < 8u)
        {
            init_params_[init_n_] = value;
            init_n_ = static_cast<uint8_t>(init_n_ + 1u);
        }
        if (init_n_ >= 8u)
        {
            const uint8_t drive = static_cast<uint8_t>((dcb_[1] >> 5) & 1u);
            const uint32_t max_cyl =
                (static_cast<uint32_t>(init_params_[0]) << 8) | init_params_[1];
            const uint32_t max_hd = init_params_[2];
            if ((max_cyl > 0u) && (max_cyl <= 1024u))
            {
                cylinders_ = max_cyl;
            }
            if ((max_hd > 0u) && (max_hd <= 16u))
            {
                heads_ = max_hd;
            }
            sector_count_ = cylinders_ * heads_ * spt_;
            const uint32_t need = sector_count_ * k_hdc_sector_size;
            if (image_.size() < need)
            {
                image_.resize(need, 0);
            }
            ok(drive);
        }
        else
        {
            status_ = static_cast<uint8_t>(k_hdc_stat_req | k_hdc_stat_bsy);
        }
        return;
    }
    if (phase_ == phase::data_out)
    {
        if (buf_pos_ < buf_.size())
        {
            buf_[buf_pos_] = value;
            buf_pos_++;
        }
        if (buf_pos_ >= buf_.size())
        {
            finish_write();
        }
    }
}

uint8_t hdc_xebec::read_data()
{
    if (phase_ == phase::send_status)
    {
        clear_irq();
        const uint8_t val = completion_;
        status_ = 0;
        phase_ = phase::idle;
        return val;
    }
    if (phase_ == phase::send_sense)
    {
        const uint8_t b = sense_[sense_idx_];
        sense_idx_ = static_cast<uint8_t>(sense_idx_ + 1u);
        if (sense_idx_ >= 4u)
        {
            clear_irq();
            set_idle();
        }
        return b;
    }
    if (phase_ == phase::data_in)
    {
        uint8_t b = 0xFFu;
        if (buf_pos_ < buf_.size())
        {
            b = buf_[buf_pos_];
            buf_pos_++;
            if (buf_pos_ >= buf_.size())
            {
                const uint8_t drive = static_cast<uint8_t>((dcb_[1] >> 5) & 1u);
                ok(drive);
            }
        }
        return b;
    }
    return 0xFFu;
}

bool hdc_xebec::parse_chs(uint8_t *drive, uint32_t *head, uint32_t *cyl, uint32_t *sector,
                         uint32_t *count)
{
    if ((drive == nullptr) || (head == nullptr) || (cyl == nullptr) || (sector == nullptr) ||
        (count == nullptr))
    {
        return false;
    }
    *drive = static_cast<uint8_t>((dcb_[1] >> 5) & 1u);
    *head = static_cast<uint32_t>(dcb_[1] & 0x1Fu);
    *cyl = (static_cast<uint32_t>(dcb_[2] & 0xC0u) << 2) | dcb_[3];
    *sector = static_cast<uint32_t>(dcb_[2] & 0x3Fu);
    *count = (dcb_[4] == 0) ? 256u : static_cast<uint32_t>(dcb_[4]);
    last_drive_ = *drive;
    last_head_ = *head;
    last_cyl_ = *cyl;
    last_sec_ = *sector;
    return true;
}

bool hdc_xebec::lba_ok(uint32_t cyl, uint32_t head, uint32_t sector, uint32_t count,
                      uint32_t *lba) const
{
    if ((sector >= spt_) || (head >= heads_) || (cyl >= cylinders_) || (count == 0))
    {
        return false;
    }
    const uint32_t start = iron86_hdc_chs_to_lba(cyl, heads_, head, spt_, sector);
    if ((start >= sector_count_) || ((start + count) > sector_count_))
    {
        return false;
    }
    if (lba != nullptr)
    {
        *lba = start;
    }
    return true;
}

void hdc_xebec::exec_dcb()
{
    const uint8_t opcode = static_cast<uint8_t>(dcb_[0] & 0x1Fu);
    const uint8_t cmd_class = static_cast<uint8_t>((dcb_[0] >> 5) & 7u);
    const uint8_t drive = static_cast<uint8_t>((dcb_[1] >> 5) & 1u);
    last_drive_ = drive;
    last_head_ = static_cast<uint32_t>(dcb_[1] & 0x1Fu);
    last_cyl_ = (static_cast<uint32_t>(dcb_[2] & 0xC0u) << 2) | dcb_[3];
    last_sec_ = static_cast<uint32_t>(dcb_[2] & 0x3Fu);

    if ((cmd_class != 0u) && (cmd_class != 7u))
    {
        fail(2, 0, drive);
        return;
    }
    /* Class 7 first: IBM ROS DISK_SETUP AH=14 is E4h (controller diag).
     * Masking to opcode 04h must not run Format Drive (would fill C: with 0xF6). */
    if (cmd_class == 7u)
    {
        if ((opcode == 0x00u) || (opcode == 0x03u) || (opcode == 0x04u))
        {
            ok(drive);
            return;
        }
        fail(2, 0, drive);
        return;
    }
    if (opcode == 0x00u)
    {
        ok(drive);
        return;
    }
    if (opcode == 0x01u)
    {
        cur_cyl_[drive] = 0;
        ok(drive);
        return;
    }
    if (opcode == 0x03u)
    {
        phase_ = phase::send_sense;
        sense_idx_ = 0;
        status_ = static_cast<uint8_t>(k_hdc_stat_bsy | k_hdc_stat_cd);
        defer_sense_ = true;
        post_dcb_status_reads_ = 0;
        return;
    }
    if (opcode == 0x04u)
    {
        format_range(0, sector_count_);
        ok(drive);
        return;
    }
    if (opcode == 0x05u)
    {
        start_verify();
        return;
    }
    if ((opcode == 0x06u) || (opcode == 0x07u))
    {
        uint8_t d = 0;
        uint32_t head = 0;
        uint32_t cyl = 0;
        uint32_t sector = 0;
        uint32_t count = 0;
        uint32_t lba = 0;
        (void)parse_chs(&d, &head, &cyl, &sector, &count);
        if (!lba_ok(cyl, head, 0, spt_, &lba))
        {
            fail(2, 1, drive);
            return;
        }
        format_range(lba, spt_);
        cur_cyl_[drive] = static_cast<uint16_t>(cyl);
        ok(drive);
        return;
    }
    if (opcode == 0x08u)
    {
        start_read();
        return;
    }
    if (opcode == 0x0Au)
    {
        start_write();
        return;
    }
    if (opcode == 0x0Bu)
    {
        uint8_t d = 0;
        uint32_t head = 0;
        uint32_t cyl = 0;
        uint32_t sector = 0;
        uint32_t count = 0;
        (void)parse_chs(&d, &head, &cyl, &sector, &count);
        if ((cyl >= cylinders_) || (head >= heads_))
        {
            fail(2, 1, drive);
            return;
        }
        cur_cyl_[drive] = static_cast<uint16_t>(cyl);
        ok(drive);
        return;
    }
    if (opcode == 0x0Cu)
    {
        phase_ = phase::recv_init;
        init_n_ = 0;
        defer_init_ = true;
        defer_completion_ = false;
        post_dcb_status_reads_ = 0;
        status_ = k_hdc_stat_bsy;
        return;
    }
    if ((opcode == 0x0Eu) || (opcode == 0x0Fu))
    {
        ok(drive);
        return;
    }
    fail(2, 0, drive);
}

void hdc_xebec::start_verify()
{
    uint8_t drive = 0;
    uint32_t head = 0;
    uint32_t cyl = 0;
    uint32_t sector = 0;
    uint32_t count = 0;
    uint32_t lba = 0;
    (void)parse_chs(&drive, &head, &cyl, &sector, &count);
    if (!lba_ok(cyl, head, sector, count, &lba))
    {
        fail(2, 1, drive);
        return;
    }
    cur_cyl_[drive] = static_cast<uint16_t>(cyl);
    ok(drive);
}

void hdc_xebec::start_read()
{
    uint8_t drive = 0;
    uint32_t head = 0;
    uint32_t cyl = 0;
    uint32_t sector = 0;
    uint32_t count = 0;
    uint32_t lba = 0;
    (void)parse_chs(&drive, &head, &cyl, &sector, &count);
    if (!lba_ok(cyl, head, sector, count, &lba))
    {
        fail(2, 1, drive);
        return;
    }
    const uint32_t off = lba * k_hdc_sector_size;
    const uint32_t nbytes = count * k_hdc_sector_size;
    buf_.assign(image_.begin() + static_cast<std::ptrdiff_t>(off),
                image_.begin() + static_cast<std::ptrdiff_t>(off + nbytes));
    buf_pos_ = 0;
    cur_cyl_[drive] = static_cast<uint16_t>(cyl);
    phase_ = phase::data_in;
    status_ = static_cast<uint8_t>(k_hdc_stat_bsy | k_hdc_stat_io | k_hdc_stat_req);
    if ((dma_irq_ & 0x01u) != 0)
    {
        status_ = static_cast<uint8_t>(status_ | k_hdc_stat_drq);
        dma_to_memory();
        ok(drive);
    }
}

void hdc_xebec::start_write()
{
    uint8_t drive = 0;
    uint32_t head = 0;
    uint32_t cyl = 0;
    uint32_t sector = 0;
    uint32_t count = 0;
    uint32_t lba = 0;
    (void)parse_chs(&drive, &head, &cyl, &sector, &count);
    if (!lba_ok(cyl, head, sector, count, &lba))
    {
        fail(2, 1, drive);
        return;
    }
    write_lba_ = lba;
    buf_.assign(count * k_hdc_sector_size, 0);
    buf_pos_ = 0;
    phase_ = phase::data_out;
    status_ = static_cast<uint8_t>(k_hdc_stat_bsy | k_hdc_stat_req);
    if ((dma_irq_ & 0x01u) != 0)
    {
        status_ = static_cast<uint8_t>(status_ | k_hdc_stat_drq);
        dma_from_memory();
        finish_write();
    }
}

void hdc_xebec::finish_write()
{
    const uint8_t drive = static_cast<uint8_t>((dcb_[1] >> 5) & 1u);
    const uint32_t off = write_lba_ * k_hdc_sector_size;
    const uint32_t n = static_cast<uint32_t>(buf_.size());
    if ((off + n) <= image_.size())
    {
        std::memcpy(image_.data() + off, buf_.data(), n);
        (void)persist(off, n);
    }
    uint8_t d = 0;
    uint32_t head = 0;
    uint32_t cyl = 0;
    uint32_t sector = 0;
    uint32_t count = 0;
    (void)parse_chs(&d, &head, &cyl, &sector, &count);
    cur_cyl_[drive] = static_cast<uint16_t>(cyl);
    ok(drive);
}

void hdc_xebec::format_range(uint32_t lba, uint32_t nsec)
{
    const uint32_t off = lba * k_hdc_sector_size;
    const uint32_t n = nsec * k_hdc_sector_size;
    if ((off + n) > image_.size())
    {
        return;
    }
    std::memset(image_.data() + off, 0xF6, n);
    (void)persist(off, n);
}

void hdc_xebec::dma_to_memory()
{
    if ((cpu_ == nullptr) || (dma_ == nullptr) || buf_.empty())
    {
        return;
    }
    const uint8_t ch = k_hdc_dma_ch;
    const uint32_t phys =
        (static_cast<uint32_t>(dma_->page[ch] & 0x0Fu) << 16) | dma_->curr_addr[ch];
    uint32_t remaining = static_cast<uint32_t>(dma_->curr_count[ch]) + 1u;
    if (remaining == 0)
    {
        remaining = 0x10000u;
    }
    const uint32_t n = std::min(static_cast<uint32_t>(buf_.size()), remaining);
    uint32_t i = 0;
    for (; i < n; i++)
    {
        cpu_->mem_write8((phys + i) & 0xFFFFFu, buf_[i]);
    }
    dma_->curr_addr[ch] = static_cast<uint16_t>(dma_->curr_addr[ch] + static_cast<uint16_t>(n));
    dma_->curr_count[ch] = static_cast<uint16_t>(dma_->curr_count[ch] - static_cast<uint16_t>(n));
    buf_pos_ = n;
}

void hdc_xebec::dma_from_memory()
{
    if ((cpu_ == nullptr) || (dma_ == nullptr) || buf_.empty())
    {
        return;
    }
    const uint8_t ch = k_hdc_dma_ch;
    const uint32_t phys =
        (static_cast<uint32_t>(dma_->page[ch] & 0x0Fu) << 16) | dma_->curr_addr[ch];
    const uint32_t n = static_cast<uint32_t>(buf_.size());
    uint32_t i = 0;
    for (; i < n; i++)
    {
        buf_[i] = cpu_->mem_read8((phys + i) & 0xFFFFFu);
    }
    dma_->curr_addr[ch] = static_cast<uint16_t>(dma_->curr_addr[ch] + static_cast<uint16_t>(n));
    dma_->curr_count[ch] = static_cast<uint16_t>(dma_->curr_count[ch] - static_cast<uint16_t>(n));
    buf_pos_ = n;
}

bool hdc_xebec::persist(uint32_t off, uint32_t n)
{
    if ((fp_ == nullptr) || (n == 0))
    {
        return true;
    }
    if ((off + n) > image_.size())
    {
        return false;
    }
    if (std::fseek(fp_, static_cast<long>(off), SEEK_SET) != 0)
    {
        return false;
    }
    if (std::fwrite(image_.data() + off, 1, n, fp_) != n)
    {
        return false;
    }
    return std::fflush(fp_) == 0;
}

} // namespace iron86
