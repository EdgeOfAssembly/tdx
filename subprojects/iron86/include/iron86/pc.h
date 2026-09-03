/**
 * @file pc.h
 * @brief Tiny IBM PC: 360K/720K floppy + INT 10h/13h/16h + INT 1Ah ticks.
 */
#ifndef IRON86_PC_H
#define IRON86_PC_H

#include "iron86/cpu.h"
#include "iron86/crtc.h"
#include "iron86/hw.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace iron86
{

class pc
{
public:
    cpu c;

    /** @brief Load a 360K (or any) image; boot sector copied to 0000:7C00. */
    bool load_floppy(const uint8_t *img, size_t n);

    /** @brief CS:IP = 0000:7C00, DL = 00h, BIOS hooks installed. */
    void boot();

    /**
     * @brief Queue ASCII as INT 16h keys (AH=00/01). Used to type at A>.
     * Also injects XT make/break into the 8255 (Py86 type_scancodes) so
     * real 5150 BIOS INT 9 / INT 16h see the key.
     * @param[in] s Bytes to type (CR = 0x0D).
     */
    void type_keys(const char *s);

    /** @brief Queue one XT set-1 make + break (IRQ1 / INT 9). */
    void type_scan(uint8_t make);

    /** @brief True when the INT 16h type-ahead buffer is empty. */
    bool keys_empty() const { return kbd_.empty(); }

    /** @brief INT 16h AH=00 with empty buffer: IP rewound, tdx should wait. */
    bool waiting_key() const { return waiting_key_; }
    void clear_wait_key() { waiting_key_ = false; }

    const std::string &tty() const { return tty_; }

    /** @brief BIOS video mode (INT 10h AH=00). Default 03h text. */
    uint8_t video_mode() const { return video_mode_; }

    bool bios_int(uint8_t vector);

    /**
     * @brief Wire PPI/PIT/PIC/DMA I/O (Py86). Call after load_bios_5150_8k.
     */
    void wire_pc_hw();

    /**
     * @brief BDA RESET_FLAG=1234h (Py86 --fast-post). Skips long STGTST.
     */
    void enable_fast_post();

    /**
     * @brief SW1 video bits: MDA 80×25 (0x3D family) vs CGA 80×25 (0x2D).
     * Call after wire_pc_hw. Keeps floppy-count bits.
     */
    void set_mda(bool on);

    /**
     * @brief Host PC speaker (default on). CLI `--no-audio` passes false.
     * @param[in] on false: no BEL and no beep events.
     */
    void enable_audio(bool on);

    /**
     * @brief Rising-edge speaker beeps while audio is enabled.
     * @return Count since construct (0 if audio was off for those edges).
     */
    unsigned beep_count() const { return beep_count_; }

    /**
     * @brief Mount a raw sector image as drive A: (unit 0). No reset.
     */
    bool attach_floppy(const uint8_t *img, size_t n);

    /**
     * @brief Mount a raw sector image on FDC unit 0 (A:) or 1 (B:).
     * @param[in] unit 0 or 1.
     * @param[in] img Image bytes.
     * @param[in] n Byte length (minimum 512).
     * @return false if unit>1, img is NULL, or n<512.
     */
    bool attach_floppy(uint8_t unit, const uint8_t *img, size_t n);

    /**
     * @brief Mount PATH as unit 0/1: directory packs 360K/720K FlopFS, file is raw.
     * @param[in] unit 0 or 1.
     * @param[in] path Host file or directory (non-recursive 8.3 pack).
     * @return false on missing path, pack failure, or short image.
     */
    bool attach_floppy_path(uint8_t unit, const char *path);

    /** @brief Mount a raw sector image as drive B: (unit 1). */
    bool attach_floppy_b(const uint8_t *img, size_t n);

private:
    bool int10();
    bool int13();
    bool int16();
    bool int1a();
    uint32_t chs_lba(uint8_t cyl, uint8_t head, uint8_t sec, uint8_t unit = 0) const;
    uint32_t ticks_18hz() const;

    void tty_cell(uint8_t ch);
    void tty_scroll();

    std::vector<uint8_t> floppy_[2];
    std::string tty_;
    std::deque<uint8_t> kbd_;
    std::deque<uint8_t> scanq_;
    std::chrono::steady_clock::time_point t0_{};
    uint8_t video_mode_ = 0x03;
    uint8_t cur_x_ = 0;
    uint8_t cur_y_ = 0;
    bool waiting_key_ = false;

    pc_hw hw_{};
    mc6845 mda_{}; /**< MDA 6845 + LPT1 (3B4/3B5/3B8/3BA, 3BC–3BE). */
    mc6845 cga_{}; /**< CGA 6845 (3D4/3D5/3D8/3DA). */
    bool audio_enabled_ = true;
    unsigned beep_count_ = 0;

    uint8_t in_port(uint16_t port);
    void out_port(uint16_t port, uint8_t v);
    void after_insn();
    void pit_tick();
    void ppi_tick();
    void ppi_on_port_b();
    void speaker_rising();
    void pic_assert(uint8_t irq);
    void pic_deassert(uint8_t irq);
    void pic_write_cmd(uint8_t v);
    void pic_write_data(uint8_t v);
    uint8_t dma_in(uint16_t port);
    void dma_out(uint16_t port, uint8_t v);
    void pit_write(uint16_t port, uint8_t v);
    uint8_t pit_read(uint16_t port);
    void fdc_write_dor(uint8_t v);
    void fdc_write_cmd(uint8_t v);
    uint8_t fdc_read_res();
    void fdc_exec();
    void fdc_dma_to_mem(const uint8_t *data, size_t n);
    const std::vector<uint8_t> *floppy_media(uint8_t drv) const;
    void sync_fdd_dip();
    void queue_scan(uint8_t sc);
    static uint8_t ascii_make(uint8_t ch);
};

} // namespace iron86

#endif
