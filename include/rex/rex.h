/**
 * @file rex.h
 * @brief Stable C ABI for librex — reusable debugger core.
 *
 * @note Link with `-lrex` (or the in-tree static library). DOS 8086 is the
 *       first target; the same session API is intended for other backends.
 *
 * Thread safety: a session is not thread-safe. Guard with an external mutex
 * if UI and socket threads share one session.
 */
#ifndef REX_H
#define REX_H

#include "rex_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rex_session rex_session;

/**
 * @brief Library version string (same as TDX_VERSION_STRING in v0.1).
 */
REX_API const char *rex_version(void);

/**
 * @brief Human-readable status name.
 */
REX_API const char *rex_status_str(rex_status st);

/**
 * @brief Create an empty session (no program loaded).
 * @return Heap session; caller must @c rex_session_destroy. NULL on OOM.
 */
REX_API rex_session *rex_session_create(void);

/**
 * @brief Destroy a session and its target (idempotent on NULL).
 */
REX_API void rex_session_destroy(rex_session *s);

/**
 * @brief Load a DOS MZ EXE or .COM into a fresh 8086 machine.
 *
 * @param[in] s    Session.
 * @param[in] path Host path to the binary.
 * @param[in] cwd  DOS current directory for INT 21 files; NULL = dirname(path).
 *
 * @retval REX_OK      Loaded; CS:IP at entry; not yet executed.
 * @retval REX_ERR_IO  Cannot read file.
 * @retval REX_ERR_FMT Not a COM/MZ image we understand.
 */
REX_API rex_status rex_session_load(rex_session *s, const char *path, const char *cwd);

/**
 * @brief Boot a floppy image at 0000:7C00 on iron86 (opt-in; Unicorn EXE path unchanged).
 *
 * @param[in] s     Session.
 * @param[in] image Host path to a 360K (or larger) raw image.
 */
REX_API rex_status rex_session_load_floppy(rex_session *s, const char *image);

/**
 * @brief Boot a floppy image at 0000:7C00 on Unicorn (INT 13h from the image).
 */
REX_API rex_status rex_session_load_floppy_uc(rex_session *s, const char *image);

/** @brief Load IBM 5150 8K BIOS on iron86; CS=FFFF IP=0000. */
REX_API rex_status rex_session_load_bios(rex_session *s, const char *path);

/** @brief After --bios: MDA 80×25 DIP (BIOS mode 7, B000). Default CGA. */
REX_API void rex_session_set_mda(rex_session *s, int on);

/**
 * @brief 1 MiB executed-opcode map (iron86). Same linear layout as RAM; zeros
 *        where nothing was fetched. Enable-only; default off.
 */
REX_API rex_status rex_session_set_exec_map(rex_session *s, const char *path);

/** @brief Attach a floppy image or host directory (FlopFS pack) as A:. */
REX_API rex_status rex_session_attach_floppy(rex_session *s, const char *image);

/**
 * @brief Attach a floppy image or host directory as B: (unit 1).
 *
 * Directories are packed to a 360K FlopFS data disk. Does not replace
 * @c rex_session_attach_floppy (A:).
 */
REX_API rex_status rex_session_attach_floppy_b(rex_session *s, const char *image);

/**
 * @brief Reload the same image at entry (Turbo Debugger Ctrl-F2).
 *
 * Breakpoints are kept. Keyboard buffer and CON output are cleared.
 */
REX_API rex_status rex_session_reset(rex_session *s);

/**
 * @brief Execute one instruction (F7 Trace).
 */
REX_API rex_status rex_session_step(rex_session *s);

/**
 * @brief Step over CALL / INT / REP / LOOP (F8).
 *
 * LOOP at the loop instruction runs remaining iterations until fall-through.
 * Inner breakpoints still fire.
 *
 * @param[in] s         Session.
 * @param[in] max_insns Safety cap (0 = default 10 million).
 */
REX_API rex_status rex_session_step_over(rex_session *s, uint64_t max_insns);

/**
 * @brief VCR tape: Down/F8 steps over CALL/INT/REP/LOOP as one frame; F7 traces in.
 * Jcc/JMP follow live flags. Home/End are tape start/end. Game RAM (CGA) rewinds too.
 */
REX_API void rex_session_vcr_seed(rex_session *s);
REX_API rex_status rex_session_vcr_forward(rex_session *s, bool step_into);
REX_API rex_status rex_session_vcr_back(rex_session *s);
REX_API rex_status rex_session_vcr_home(rex_session *s);
REX_API rex_status rex_session_vcr_end(rex_session *s);

/**
 * @brief Run until break, halt, fault, wait-key, or @p max_insns.
 *
 * @param[in] max_insns 0 = default 50 million.
 */
REX_API rex_status rex_session_run(rex_session *s, uint64_t max_insns);

/**
 * @brief Request stop of a long run (F9 toggle / agent STOP).
 */
REX_API rex_status rex_session_request_stop(rex_session *s);

/** UI command posted by the agent socket (F9 / keys / listing nav). */
enum
{
    REX_UI_NONE = 0,
    REX_UI_TOGGLE_RUN = 1,
    REX_UI_STOP = 2,
    REX_UI_START_RUN = 3,
    REX_UI_LIST_UP = 4,
    REX_UI_LIST_DOWN = 5,
    REX_UI_LIST_HOME = 6,
    REX_UI_LIST_END = 7,
    REX_UI_LIST_PGUP = 8,
    REX_UI_LIST_PGDN = 9,
    REX_UI_HELP = 10
};

/**
 * @brief Queue a UI command for the SDL loop (does not run the CPU).
 */
REX_API void rex_session_post_ui_cmd(rex_session *s, int cmd);

/**
 * @brief Pop one queued UI command (0 if none).
 */
REX_API int rex_session_take_ui_cmd(rex_session *s);

/**
 * @brief Milliseconds the CPU window parks after each F9 run slice.
 *
 * @return 0 if @p s is NULL. 0 means no extra wait (fastest).
 */
REX_API uint32_t rex_session_run_delay_ms(const rex_session *s);

/**
 * @brief Set F9 slice park. Clamped to 0..200 ms.
 *
 * @param[in] ms 0 = fastest. Independent of run/pause.
 */
REX_API void rex_session_set_run_delay_ms(rex_session *s, uint32_t ms);

/**
 * @brief Step F9 slice park by 5 ms (+ = slower, − = faster, floor 0).
 *
 * @param[in] dir Positive increases delay; negative decreases. Zero is a no-op.
 * @return New delay in milliseconds, or 0 if @p s is NULL.
 */
REX_API uint32_t rex_session_nudge_run_delay(rex_session *s, int dir);

REX_API rex_stop rex_session_stop_reason(const rex_session *s);
REX_API bool rex_session_halted(const rex_session *s);
REX_API int rex_session_exit_code(const rex_session *s);
REX_API rex_arch rex_session_arch(const rex_session *s);

REX_API rex_status rex_session_get_regs_i8086(const rex_session *s, rex_regs_i8086 *out);
REX_API rex_status rex_session_set_regs_i8086(rex_session *s, const rex_regs_i8086 *in);

REX_API rex_status rex_session_read_mem(const rex_session *s, uint64_t linear, void *dst, size_t n);
REX_API rex_status rex_session_write_mem(rex_session *s, uint64_t linear, const void *src, size_t n);

/**
 * @brief Disassemble @p count instructions starting at CS:IP or @p linear.
 *
 * @param[in]  linear  UINT64_MAX = use current CS:IP.
 * @param[out] out     Array of @p cap entries.
 * @param[in]  cap     Capacity.
 * @param[out] wrote   Optional count written.
 */
REX_API rex_status rex_session_disasm(const rex_session *s, uint64_t linear, rex_insn *out,
                                      size_t cap, size_t *wrote);

/**
 * @brief Add an execution breakpoint.
 * @param[out] id Optional assigned id.
 */
REX_API rex_status rex_bp_add_linear(rex_session *s, uint64_t linear, uint32_t *id);
REX_API rex_status rex_bp_add_segoff(rex_session *s, uint16_t seg, uint16_t off, uint32_t *id);
REX_API rex_status rex_bp_del(rex_session *s, uint32_t id);
REX_API rex_status rex_bp_del_linear(rex_session *s, uint64_t linear);
REX_API rex_status rex_bp_clear(rex_session *s);
REX_API size_t rex_bp_count(const rex_session *s);
REX_API bool rex_bp_at(const rex_session *s, uint64_t linear);

/** One execution breakpoint for the CPU panel / agents. */
struct rex_bp
{
    uint32_t id;
    uint16_t seg;
    uint16_t off;
    uint64_t linear;
};

/**
 * @brief Copy up to @p cap breakpoints into @p out (stable id order).
 * @return Number of entries written (not total count).
 */
REX_API size_t rex_bp_list(const rex_session *s, rex_bp *out, size_t cap);

/** Break on software interrupt @p intno (TD BPINT). Stops before the handler. */
REX_API rex_status rex_bp_int(rex_session *s, uint8_t intno);
REX_API rex_status rex_bp_int_del(rex_session *s, uint8_t intno);

/**
 * @brief BPINT with a hit count.
 *
 * @param hits 0 = every INT @p intno, 1 = first only, N = next N then auto-clear.
 */
REX_API rex_status rex_bp_int_hits(rex_session *s, uint8_t intno, uint32_t hits);

/** One BPINT entry for agents (`remain` 0 = every). */
struct rex_int_bp
{
    uint8_t intno;
    uint32_t remain;
};

REX_API size_t rex_int_bp_list(const rex_session *s, rex_int_bp *out, size_t cap);

/**
 * @brief Break when CS:IP matches Capstone text @p pat (any instruction).
 *
 * @param pat  e.g. `"int 0x10"`, `"int 10"`, `"call"`, `"out"`. Case-insensitive.
 *             Bare numbers are hex (DOS): `"int 10"` is INT 10h.
 * @param hits 0 = every match, 1 = first only, N = next N then auto-clear.
 * @param[out] id Optional assigned id (same space as exec BPs; `bpdel` works).
 */
REX_API rex_status rex_bp_insn(rex_session *s, const char *pat, uint32_t hits, uint32_t *id);

/** One mnemonic/opcode breakpoint. */
struct rex_insn_bp
{
    uint32_t id;
    uint32_t remain;
    char text[96];
};

REX_API size_t rex_insn_bp_list(const rex_session *s, rex_insn_bp *out, size_t cap);

/** Set remaining hits on an exec, insn, or range BP id (0 = every). */
REX_API rex_status rex_bp_set_hits(rex_session *s, uint32_t id, uint32_t hits);

/**
 * @brief Break when CS:IP's linear address is in [@p lo, @p hi] inclusive.
 *
 * For a procedure/overlay window without planting a BP on every insn.
 * @param hits 0 = every entry, 1 = first only.
 */
REX_API rex_status rex_bp_add_range(rex_session *s, uint64_t lo, uint64_t hi, uint32_t hits,
                                    uint32_t *id);
REX_API rex_status rex_bp_add_segoff_range(rex_session *s, uint16_t seg0, uint16_t off0,
                                           uint16_t seg1, uint16_t off1, uint32_t hits,
                                           uint32_t *id);

struct rex_range_bp
{
    uint32_t id;
    uint32_t remain;
    uint16_t seg0;
    uint16_t off0;
    uint16_t seg1;
    uint16_t off1;
    uint64_t lo;
    uint64_t hi;
};

REX_API size_t rex_range_bp_list(const rex_session *s, rex_range_bp *out, size_t cap);

/**
 * @brief Break when the guest writes any byte in [@p lo, @p hi] (TD BPM).
 *
 * Catches CPU stores (MOV/STOS to CGA `B800:0000` …). BIOS INT 10 that pokes
 * host RAM directly does not go through Unicorn and will not fire.
 * @param hits 0 = every write, 1 = first only.
 */
REX_API rex_status rex_bp_add_write(rex_session *s, uint64_t lo, uint64_t hi, uint32_t hits,
                                    uint32_t *id);
REX_API rex_status rex_bp_add_segoff_write(rex_session *s, uint16_t seg0, uint16_t off0,
                                           uint16_t seg1, uint16_t off1, uint32_t hits,
                                           uint32_t *id);
REX_API size_t rex_mem_bp_list(const rex_session *s, rex_range_bp *out, size_t cap);

/** Load a TSV/MAP symbol file (`seg:off\\tname` or `linear\\tname`). */
REX_API rex_status rex_symbols_load(rex_session *s, const char *path);
REX_API const char *rex_symbols_lookup(const rex_session *s, uint64_t linear);

/** Inject a DOS key (ASCII + optional scancode) for INT 16. */
REX_API rex_status rex_session_push_key(rex_session *s, uint8_t ascii, uint8_t scan);

/** Video: BIOS mode byte (BDA 0x449). */
REX_API uint8_t rex_session_video_mode(const rex_session *s);

/**
 * @brief Short guest label for window titles (basename of EXE, B:, A:, or BIOS).
 * @return Static for the session; never NULL (`"-"` if unknown).
 */
REX_API const char *rex_session_guest(const rex_session *s);

/**
 * @brief Decode CGA mode-4/5 VRAM into 320×200 indices 0..3.
 * @param[out] px  64000 bytes.
 */
REX_API rex_status rex_session_cga_decode(const rex_session *s, uint8_t *px, size_t px_size);

/** Host directory used as DOS cwd (INT 21 files). */
REX_API const char *rex_session_dos_cwd(const rex_session *s);

/** Linear entry point after load. */
REX_API uint64_t rex_session_entry_linear(const rex_session *s);

/** Bytes written to DOS CON (INT 21 AH=02/09/40 handles 1/2). */
REX_API const char *rex_session_con_out(const rex_session *s);

#ifdef __cplusplus
}
#endif

#endif /* REX_H */
