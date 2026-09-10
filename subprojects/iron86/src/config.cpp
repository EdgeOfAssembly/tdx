/**
 * @file config.cpp
 * @brief INI parser and preset merge for iron86 0.17.
 */
#include "iron86/config.h"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <sys/stat.h>

namespace iron86
{
namespace
{

static bool ascii_ieq(const char *a, const char *b)
{
    if ((a == nullptr) || (b == nullptr))
    {
        return a == b;
    }
    while ((*a != '\0') && (*b != '\0'))
    {
        unsigned char ca = static_cast<unsigned char>(*a);
        unsigned char cb = static_cast<unsigned char>(*b);
        if ((ca >= 'A') && (ca <= 'Z'))
        {
            ca = static_cast<unsigned char>(ca - 'A' + 'a');
        }
        if ((cb >= 'A') && (cb <= 'Z'))
        {
            cb = static_cast<unsigned char>(cb - 'A' + 'a');
        }
        if (ca != cb)
        {
            return false;
        }
        a++;
        b++;
    }
    return (*a == '\0') && (*b == '\0');
}

static void trim_inplace(std::string *s)
{
    if (s == nullptr)
    {
        return;
    }
    size_t a = 0;
    while ((a < s->size()) &&
           (((*s)[a] == ' ') || ((*s)[a] == '\t') || ((*s)[a] == '\r')))
    {
        a++;
    }
    size_t b = s->size();
    while ((b > a) &&
           (((*s)[b - 1u] == ' ') || ((*s)[b - 1u] == '\t') || ((*s)[b - 1u] == '\r')))
    {
        b--;
    }
    if ((a != 0u) || (b != s->size()))
    {
        s->assign(s->data() + a, b - a);
    }
}

static void strip_comment(std::string *s)
{
    if (s == nullptr)
    {
        return;
    }
    bool in_quote = false;
    size_t i = 0;
    for (; i < s->size(); i++)
    {
        const char c = (*s)[i];
        if (c == '"')
        {
            in_quote = !in_quote;
            continue;
        }
        if (!in_quote && ((c == ';') || (c == '#')))
        {
            s->resize(i);
            return;
        }
    }
}

static void unquote(std::string *s)
{
    if ((s == nullptr) || (s->size() < 2u))
    {
        return;
    }
    if (((*s)[0] == '"') && ((*s)[s->size() - 1u] == '"'))
    {
        s->assign(s->data() + 1u, s->size() - 2u);
    }
}

static bool parse_u32(const std::string &v, uint32_t *out, std::string *err, unsigned line,
                      const char *key)
{
    if ((out == nullptr) || v.empty() || (v[0] == '-') || (v[0] == '+'))
    {
        if (err != nullptr)
        {
            *err = "line " + std::to_string(line) + ": bad " + key;
        }
        return false;
    }
    char *end = nullptr;
    const unsigned long n = std::strtoul(v.c_str(), &end, 10);
    if ((end == v.c_str()) || (*end != '\0'))
    {
        if (err != nullptr)
        {
            *err = "line " + std::to_string(line) + ": bad " + key;
        }
        return false;
    }
    *out = static_cast<uint32_t>(n);
    return true;
}

static bool apply_key(const std::string &section, const std::string &key, const std::string &val,
                      machine_config *cfg, std::string *err, unsigned line)
{
    if (ascii_ieq(section.c_str(), "machine"))
    {
        if (ascii_ieq(key.c_str(), "preset"))
        {
            machine_preset p = machine_preset::ibm_pc;
            const int r = parse_preset_name(val.c_str(), &p);
            if (r < 0)
            {
                if (err != nullptr)
                {
                    *err = "line " + std::to_string(line) + ": unknown preset " + val;
                }
                return false;
            }
            cfg->preset = p;
            cfg->preset_set = true;
            return true;
        }
        return true;
    }
    if (ascii_ieq(section.c_str(), "bios"))
    {
        if (ascii_ieq(key.c_str(), "rom"))
        {
            cfg->bios_rom = val;
        }
        return true;
    }
    if (ascii_ieq(section.c_str(), "floppy"))
    {
        if (ascii_ieq(key.c_str(), "a"))
        {
            cfg->floppy_a = val;
        }
        else if (ascii_ieq(key.c_str(), "b"))
        {
            cfg->floppy_b = val;
        }
        return true;
    }
    if (ascii_ieq(section.c_str(), "hdc"))
    {
        if (ascii_ieq(key.c_str(), "image"))
        {
            cfg->hdc_image = val;
            return true;
        }
        uint32_t n = 0;
        if (ascii_ieq(key.c_str(), "cylinders"))
        {
            if (!parse_u32(val, &n, err, line, "cylinders"))
            {
                return false;
            }
            cfg->hdc_cylinders = n;
            cfg->hdc_cylinders_set = true;
            return true;
        }
        if (ascii_ieq(key.c_str(), "heads"))
        {
            if (!parse_u32(val, &n, err, line, "heads"))
            {
                return false;
            }
            cfg->hdc_heads = n;
            cfg->hdc_heads_set = true;
            return true;
        }
        if (ascii_ieq(key.c_str(), "spt"))
        {
            if (!parse_u32(val, &n, err, line, "spt"))
            {
                return false;
            }
            cfg->hdc_spt = n;
            cfg->hdc_spt_set = true;
            return true;
        }
        return true;
    }
    if (ascii_ieq(section.c_str(), "cpu"))
    {
        if (ascii_ieq(key.c_str(), "prefetch_bytes"))
        {
            uint32_t n = 0;
            if (!parse_u32(val, &n, err, line, "prefetch_bytes"))
            {
                return false;
            }
            cfg->prefetch_bytes = static_cast<unsigned>(n);
            cfg->prefetch_set = true;
        }
        return true;
    }
    return true;
}

} // namespace

int parse_preset_name(const char *name, machine_preset *out)
{
    if ((name == nullptr) || (name[0] == '\0') || (out == nullptr))
    {
        return -1;
    }
    if (ascii_ieq(name, "ibm-pc") || ascii_ieq(name, "5150") || ascii_ieq(name, "ibm-5150"))
    {
        *out = machine_preset::ibm_pc;
        return 0;
    }
    if (ascii_ieq(name, "ibm-xt") || ascii_ieq(name, "5160") || ascii_ieq(name, "ibm-5160"))
    {
        *out = machine_preset::ibm_xt;
        return 0;
    }
    if (ascii_ieq(name, "ibm-at"))
    {
        *out = machine_preset::ibm_at;
        return 1;
    }
    if (ascii_ieq(name, "8286"))
    {
        *out = machine_preset::cpu_8286;
        return 1;
    }
    return -1;
}

bool preset_is_reserved(machine_preset p)
{
    return (p == machine_preset::ibm_at) || (p == machine_preset::cpu_8286);
}

const char *machine_preset_name(machine_preset p)
{
    switch (p)
    {
    case machine_preset::ibm_pc:
        return "ibm-pc";
    case machine_preset::ibm_xt:
        return "ibm-xt";
    case machine_preset::ibm_at:
        return "ibm-at";
    case machine_preset::cpu_8286:
        return "8286";
    }
    return "unknown";
}

static bool is_regular_file(const char *path)
{
    struct stat st{};
    return (path != nullptr) && (path[0] != '\0') && (stat(path, &st) == 0) &&
           S_ISREG(st.st_mode);
}

static std::string first_regular_file(const char *const *cands, size_t n, const char *env_name)
{
    const char *env = (env_name != nullptr) ? std::getenv(env_name) : nullptr;
    if (is_regular_file(env))
    {
        return std::string(env);
    }
    size_t i = 0;
    for (; i < n; i++)
    {
        if (is_regular_file(cands[i]))
        {
            return std::string(cands[i]);
        }
    }
    return {};
}

std::string find_default_bios_rom(machine_preset p)
{
    if (p == machine_preset::ibm_xt)
    {
        static const char *const k_xt[] = {
            "/mnt/TurboDebugger/subprojects/iron86/tests/images/hdc-proof/xt5160_10jan86_64k.bin",
            "subprojects/iron86/tests/images/hdc-proof/xt5160_10jan86_64k.bin",
            "tests/images/hdc-proof/xt5160_10jan86_64k.bin",
            "../tests/images/hdc-proof/xt5160_10jan86_64k.bin",
        };
        return first_regular_file(k_xt, sizeof(k_xt) / sizeof(k_xt[0]), "IRON86_XT_BIOS");
    }
    static const char *const k_pc[] = {
        "/mnt/TurboDebugger/Py86/ROM/IBM/PC/5150/BIOS_IBM5150_24APR81_5700051_U33.BIN",
        "Py86/ROM/IBM/PC/5150/BIOS_IBM5150_24APR81_5700051_U33.BIN",
        "../Py86/ROM/IBM/PC/5150/BIOS_IBM5150_24APR81_5700051_U33.BIN",
        "../../Py86/ROM/IBM/PC/5150/BIOS_IBM5150_24APR81_5700051_U33.BIN",
    };
    return first_regular_file(k_pc, sizeof(k_pc) / sizeof(k_pc[0]), "IRON86_PC_BIOS");
}

std::string find_default_hdc_rom()
{
    static const char *const k_ros[] = {
        k_hdc_ros_default,
        "Py86/ROM/IBM/XT/FixedDiskAdapter/IBM_XEBEC_5000059_1982.BIN",
        "../Py86/ROM/IBM/XT/FixedDiskAdapter/IBM_XEBEC_5000059_1982.BIN",
        "../../Py86/ROM/IBM/XT/FixedDiskAdapter/IBM_XEBEC_5000059_1982.BIN",
    };
    return first_regular_file(k_ros, sizeof(k_ros) / sizeof(k_ros[0]), "IRON86_XEBEC_ROM");
}

void apply_preset(machine_preset p, machine_config *cfg)
{
    if (cfg == nullptr)
    {
        return;
    }
    cfg->preset = p;
    cfg->preset_set = true;
    if (!cfg->prefetch_set)
    {
        cfg->prefetch_bytes = k_prefetch_default;
    }
    if (!cfg->hdc_cylinders_set)
    {
        cfg->hdc_cylinders = k_hdc_cyl_xt;
    }
    if (!cfg->hdc_heads_set)
    {
        cfg->hdc_heads = k_hdc_heads_xt;
    }
    if (!cfg->hdc_spt_set)
    {
        cfg->hdc_spt = k_hdc_spt_xt;
    }
}

void overlay_config(machine_config *dst, const machine_config &src)
{
    if (dst == nullptr)
    {
        return;
    }
    if (src.preset_set)
    {
        dst->preset = src.preset;
        dst->preset_set = true;
    }
    if (!src.bios_rom.empty())
    {
        dst->bios_rom = src.bios_rom;
    }
    if (!src.floppy_a.empty())
    {
        dst->floppy_a = src.floppy_a;
    }
    if (!src.floppy_b.empty())
    {
        dst->floppy_b = src.floppy_b;
    }
    if (!src.hdc_image.empty())
    {
        dst->hdc_image = src.hdc_image;
    }
    if (src.hdc_cylinders_set)
    {
        dst->hdc_cylinders = src.hdc_cylinders;
        dst->hdc_cylinders_set = true;
    }
    if (src.hdc_heads_set)
    {
        dst->hdc_heads = src.hdc_heads;
        dst->hdc_heads_set = true;
    }
    if (src.hdc_spt_set)
    {
        dst->hdc_spt = src.hdc_spt;
        dst->hdc_spt_set = true;
    }
    if (src.prefetch_set)
    {
        dst->prefetch_bytes = src.prefetch_bytes;
        dst->prefetch_set = true;
    }
}

bool parse_ini_text(const char *text, machine_config *cfg, std::string *err)
{
    if ((text == nullptr) || (cfg == nullptr))
    {
        if (err != nullptr)
        {
            *err = "null ini text or config";
        }
        return false;
    }
    if ((static_cast<unsigned char>(text[0]) == 0xEFu) &&
        (static_cast<unsigned char>(text[1]) == 0xBBu) &&
        (static_cast<unsigned char>(text[2]) == 0xBFu))
    {
        text += 3;
    }

    std::string section;
    unsigned line_no = 0;
    const char *p = text;
    while (*p != '\0')
    {
        const char *start = p;
        while ((*p != '\0') && (*p != '\n'))
        {
            p++;
        }
        std::string line(start, static_cast<size_t>(p - start));
        if (*p == '\n')
        {
            p++;
        }
        line_no++;
        strip_comment(&line);
        trim_inplace(&line);
        if (line.empty())
        {
            continue;
        }
        if (line.front() == '[')
        {
            if ((line.size() < 3u) || (line.back() != ']'))
            {
                if (err != nullptr)
                {
                    *err = "line " + std::to_string(line_no) + ": bad section";
                }
                return false;
            }
            section.assign(line.data() + 1u, line.size() - 2u);
            trim_inplace(&section);
            if (section.empty())
            {
                if (err != nullptr)
                {
                    *err = "line " + std::to_string(line_no) + ": empty section";
                }
                return false;
            }
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos)
        {
            if (err != nullptr)
            {
                *err = "line " + std::to_string(line_no) + ": expected key=value";
            }
            return false;
        }
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1u);
        trim_inplace(&key);
        trim_inplace(&val);
        unquote(&val);
        if (key.empty())
        {
            if (err != nullptr)
            {
                *err = "line " + std::to_string(line_no) + ": empty key";
            }
            return false;
        }
        if (section.empty())
        {
            if (err != nullptr)
            {
                *err = "line " + std::to_string(line_no) + ": key outside section";
            }
            return false;
        }
        if (!apply_key(section, key, val, cfg, err, line_no))
        {
            return false;
        }
    }
    return true;
}

bool load_ini_file(const char *path, machine_config *cfg, std::string *err)
{
    if ((path == nullptr) || (path[0] == '\0') || (cfg == nullptr))
    {
        if (err != nullptr)
        {
            *err = "null config path";
        }
        return false;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        if (err != nullptr)
        {
            *err = std::string("cannot read ") + path;
        }
        return false;
    }
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return parse_ini_text(text.c_str(), cfg, err);
}

bool default_ini_present()
{
    struct stat st{};
    if (stat(k_default_ini_name, &st) != 0)
    {
        return false;
    }
    return S_ISREG(st.st_mode) != 0;
}

bool merge_machine_config(const char *config_path, const machine_config &cli, machine_config *out,
                          std::string *err)
{
    if (out == nullptr)
    {
        if (err != nullptr)
        {
            *err = "null config";
        }
        return false;
    }
    *out = machine_config{};
    apply_preset(machine_preset::ibm_pc, out);

    const char *path = config_path;
    if (path == nullptr)
    {
        if (default_ini_present())
        {
            path = k_default_ini_name;
        }
    }
    if (path != nullptr)
    {
        machine_config ini{};
        if (!load_ini_file(path, &ini, err))
        {
            return false;
        }
        if (ini.preset_set && preset_is_reserved(ini.preset) && !cli.preset_set)
        {
            if (err != nullptr)
            {
                *err = std::string("preset ") + machine_preset_name(ini.preset) +
                       " is not yet implemented";
            }
            return false;
        }
        if (ini.preset_set && !preset_is_reserved(ini.preset))
        {
            apply_preset(ini.preset, out);
        }
        overlay_config(out, ini);
    }
    if (cli.preset_set)
    {
        if (preset_is_reserved(cli.preset))
        {
            if (err != nullptr)
            {
                *err = std::string("preset ") + machine_preset_name(cli.preset) +
                       " is not yet implemented";
            }
            return false;
        }
        apply_preset(cli.preset, out);
    }
    overlay_config(out, cli);
    return true;
}

} // namespace iron86
