/**
 * @file dos_machine.h
 * @brief Unicorn 8086 real-mode machine + tiny DOS/BIOS (internal C++ API).
 */
#ifndef DOS_MACHINE_H
#define DOS_MACHINE_H

#include "dos_cga.h"
#include "mz_parse.h"
#include "pic8259.h"
#include "pit8253.h"
#include "rex/rex.h"

#include <cstdint>
#ifndef UINT64_MAX
#include <limits>
#endif
#include <cstdio>
#include <deque>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct uc_struct;
typedef struct uc_struct uc_engine;

enum
{
    DOS_RAM_SIZE = 0x200000, /**< 2 MiB host map (A20 on). */
    DOS_PSP_SEG = 0x1000,
    DOS_LOAD_SEG = 0x1010, /**< EXE image; COM uses PSP_SEG:0100. */
    DOS_ENV_SEG = 0x0F80,
    DOS_MEM_END_PARA = 0xA000, /**< 640 KiB conventional; PSP word at offset 2. */
    DOS_MAX_FILES = 32,
    DOS_MAX_RELOCS = 8192
};

struct dos_file
{
    FILE *fp = nullptr;
    bool used = false;
};

struct dos_kbd_ev
{
    uint8_t ascii = 0;
    uint8_t scan = 0;
};

/** One byte changed during a VCR tape frame (undo/redo). */
struct vcr_delta
{
    uint32_t lin = 0;
    uint8_t oldv = 0;
    uint8_t newv = 0;
};

/** CPU + memory diffs after one VCR unit (F8-over or F7-into). */
struct vcr_frame
{
    rex_regs_i8086 regs{};
    uint8_t video_mode = 3;
    bool halted = false;
    std::vector<vcr_delta> undos;
};

struct dos_machine
{
    uc_engine *uc = nullptr;
    uint8_t *ram = nullptr;
    size_t ram_size = 0;

    std::string image_path;
    std::string dos_cwd;
    std::string con_out;

    bool is_com = false;
    bool halted = false;
    bool wait_key = false;
    bool video_dirty = false;
    bool stop_req = false;
    int exit_code = 0;
    uint8_t video_mode = 0x03;
    uint8_t cga_3da = 0; /**< CGA status (port 3DAh); toggled on each IN. */
    uint16_t cursor_x = 0;
    uint16_t cursor_y = 0;
    uint32_t dta = 0x10080; /**< Default DTA inside PSP. */
    uint64_t last_code_linear = 0; /**< Current on_code address. */
    uint64_t prev_code_linear = 0; /**< Prior insn linear (fault traceback). */
    uint64_t code_ring[8]{};
    unsigned code_ring_i = 0;
    uint64_t entry_linear = 0;
    uint64_t image_base = 0;
    uint32_t image_bytes = 0;
    std::map<uint64_t, rex_insn> decode; /**< Capstone once at load. */
    uint64_t pit_last_ns = 0;

    pic8259 pic;           /**< 8259A PIC: timer/keyboard hardware IRQs. */
    pit8253 pit{pic};      /**< 8253 PIT: ch0 terminal count drives IRQ0. */
    uint32_t pit_ticks_acc = 0; /**< Sub-burst tick accumulator. */
    unsigned intr_inhibit = 0;  /**< STI/POP-SS one-instruction IF inhibit. */
    unsigned intr_depth = 0;    /**< >0 while inside a hardware ISR (no nesting). */
    uint32_t alloc_bump = 0; /**< Next free paragraph for INT 21 AH=48. */
    bool skip_bp = false;
    bool at_break = false;
    uint64_t run_ignore_bp = UINT64_MAX;
    rex_stop last_stop = REX_STOP_NONE;
    uint32_t next_bp_id = 1;
    std::unordered_map<uint64_t, uint32_t> bps;
    std::unordered_map<uint32_t, uint64_t> bp_by_id;
    std::unordered_map<uint32_t, uint32_t> bp_segoff; /**< id -> (seg<<16)|off */
    std::unordered_set<uint8_t> int_bps;
    bool skip_int_bp = false;

    dos_file files[DOS_MAX_FILES]{};
    std::deque<dos_kbd_ev> kbd;

    bool vcr_rec = false;
    std::vector<vcr_delta> vcr_pending;
    std::deque<vcr_frame> vcr_tape;
    size_t vcr_pos = 0;

    size_t hh_intr = 0;
    size_t hh_memw = 0;
    size_t hh_unmapped = 0;

    ~dos_machine();
    dos_machine() = default;
    dos_machine(const dos_machine &) = delete;
    dos_machine &operator=(const dos_machine &) = delete;

    rex_status init_cpu();
    rex_status load_path(const char *path, const char *cwd);
    rex_status step_one();
    rex_status run_until(uint64_t until_linear, uint64_t max_insns, bool until_valid);

    void get_regs(rex_regs_i8086 *out) const;
    void set_regs(const rex_regs_i8086 *in);
    uint64_t linear_ip() const;
    rex_status read_mem(uint64_t linear, void *dst, size_t n) const;
    rex_status write_mem(uint64_t linear, const void *src, size_t n);

    void push_key(uint8_t ascii, uint8_t scan);
    void handle_intr(uint32_t intno);
    /** IBM BIOS SET_MODE regen fill: spaces+07 in alpha, zeros in graphics. */
    void blank_regen(void);

    uint16_t reg16(int uc_reg) const;
    void set_reg16(int uc_reg, uint16_t v);
    uint32_t eip32(void) const;
    void sync_ip_from_eip(void);
    void set_cf(bool carry);
    void set_zf(bool zero);
    uint8_t *ptr_segoff(uint16_t seg, uint16_t off);
    void write_dos_string(const char *text);
    std::string dos_to_host_path(const char *dos_path) const;
    int alloc_handle(void);
    void close_handle(int fd);

    rex_status bp_add(uint64_t linear, uint32_t *id, uint16_t seg, uint16_t off);
    rex_status bp_del(uint32_t id);
    void bp_clear(void);

    void rebuild_decode(void);
    void pit_poll(void);
    void tick_pit(size_t insns);
    void deliver_pending_irq(void);
    bool irq_pending_if_on(void) const;
    void vcr_seed(void);
    rex_status vcr_forward(bool step_into);
    rex_status vcr_back(void);
    rex_status vcr_home(void);
    rex_status vcr_end(void);
};

rex_status dos_machine_disasm(const dos_machine *m, uint64_t linear, rex_insn *out, size_t cap,
                              size_t *wrote);

#endif /* DOS_MACHINE_H */
