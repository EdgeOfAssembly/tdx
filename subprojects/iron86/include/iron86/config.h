/**
 * @file config.h
 * @brief iron86 INI machine config (no JSON/XML).
 *
 * Precedence: CLI flags override INI override preset defaults.
 * Default file: `iron86.ini` in the current working directory if present.
 */
#ifndef IRON86_CONFIG_H
#define IRON86_CONFIG_H

#include "iron86/cpu.h"
#include "iron86/hdc.h"

#include <cstdint>
#include <string>

namespace iron86
{

/** @brief CWD default INI name (loaded only if the file exists). */
inline constexpr const char *k_default_ini_name = "iron86.ini";

/**
 * @brief Named machine preset.
 *
 * `ibm-pc` is the 5150 default. `ibm-xt` is the same 8086/8088 plus Xebec.
 * `ibm-at` / `8286` are reserved (5170 AT, not XT).
 */
enum class machine_preset : uint8_t
{
    ibm_pc = 0,
    ibm_xt = 1,
    ibm_at = 2,
    cpu_8286 = 3
};

/**
 * @brief Resolved machine configuration.
 *
 * Path fields empty mean “unset”. Integer `_set` flags distinguish
 * “absent from INI/CLI” from an explicit zero (prefetch 0 = queue off).
 */
struct machine_config
{
    machine_preset preset{machine_preset::ibm_pc};
    bool preset_set{false};
    std::string bios_rom;
    std::string floppy_a;
    std::string floppy_b;
    std::string hdc_image;
    uint32_t hdc_cylinders{k_hdc_cyl_xt};
    uint32_t hdc_heads{k_hdc_heads_xt};
    uint32_t hdc_spt{k_hdc_spt_xt};
    bool hdc_cylinders_set{false};
    bool hdc_heads_set{false};
    bool hdc_spt_set{false};
    unsigned prefetch_bytes{k_prefetch_default};
    bool prefetch_set{false};
};

/**
 * @brief Parse `--preset` / `[machine] preset=` name.
 *
 * @param[in]  name ASCII name (`ibm-pc`, `ibm-xt`, `ibm-at`, `8286`).
 * @param[out] out  Preset id on @retval 0 or 1.
 * @return 0 known and implemented, 1 reserved (not yet), -1 unknown.
 * @retval 0 `ibm-pc` or `ibm-xt`
 * @retval 1 `ibm-at` or `8286`
 * @retval -1 anything else (including NULL)
 */
int parse_preset_name(const char *name, machine_preset *out);

/** @brief True for `ibm-at` and `8286`. */
bool preset_is_reserved(machine_preset p);

/**
 * @brief Canonical CLI/INI name for @p p.
 * @return NUL-terminated static string (`ibm-pc`, `ibm-xt`, `ibm-at`, `8286`).
 */
const char *machine_preset_name(machine_preset p);

/**
 * @brief Apply preset CPU/HDC defaults without clearing paths.
 *
 * Prefetch and HDC geometry are written only when the corresponding
 * `_set` flag is still false, so INI/CLI values survive a later preset.
 *
 * @param[in]     p   Preset (reserved ids still set the enum; caller errors).
 * @param[in,out] cfg Config to update (must not be NULL).
 */
void apply_preset(machine_preset p, machine_config *cfg);

/**
 * @brief Copy set fields from @p src onto @p dst.
 *
 * Empty path strings in @p src are skipped (unset). Integer fields use
 * their `_set` flags.
 *
 * @param[in,out] dst Destination (must not be NULL).
 * @param[in]     src Overlay.
 */
void overlay_config(machine_config *dst, const machine_config &src);

/**
 * @brief Parse INI text into @p cfg (only keys present are written).
 *
 * Sections: `[machine]` `[bios]` `[floppy]` `[hdc]` `[cpu]`.
 * Comments: `;` or `#` to end of line (not inside double quotes).
 * Unknown sections and keys are ignored.
 *
 * @param[in]     text INI body (UTF-8; optional BOM). May be empty.
 * @param[in,out] cfg  Config to fill (must not be NULL).
 * @param[out]    err  Optional error string.
 * @return false on malformed lines or invalid integers.
 */
bool parse_ini_text(const char *text, machine_config *cfg, std::string *err);

/**
 * @brief Load and parse an INI file.
 *
 * @param[in]     path Host path (must not be NULL).
 * @param[in,out] cfg  Config to fill.
 * @param[out]    err  Optional error string.
 * @return false if the file cannot be read or parse fails.
 */
bool load_ini_file(const char *path, machine_config *cfg, std::string *err);

/** @brief True if `iron86.ini` exists as a regular file in cwd. */
bool default_ini_present();

/**
 * @brief Build config: ibm-pc defaults, then INI, then CLI overlay.
 *
 * @param[in]  config_path Explicit `--config PATH`, or NULL to load cwd
 *             `iron86.ini` when that file exists.
 * @param[in]  cli         CLI overlay (only set fields).
 * @param[out] out         Result (must not be NULL).
 * @param[out] err         Optional error (`not yet` for reserved presets).
 * @return false on missing explicit file, parse error, or reserved preset
 *         that is not overridden by a valid CLI `--preset`.
 */
bool merge_machine_config(const char *config_path, const machine_config &cli,
                          machine_config *out, std::string *err);

} // namespace iron86

#endif /* IRON86_CONFIG_H */
