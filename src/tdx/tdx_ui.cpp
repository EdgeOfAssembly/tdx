/**
 * @file tdx_ui.cpp
 * @brief Borland-blue CPU window + CGA user screen (SDL2).
 */

#include "tdx/tdx_ui.h"

#include "dos/dos_cga.h"
#include "tdx/tdx_font.h"
#include "tdx/tdx_version.h"

#include <SDL.h>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
constexpr int k_cols = 80;
constexpr int k_rows = 25;
constexpr int k_cw = 8;
constexpr int k_ch = 16;
constexpr int k_foot0 = 23; /**< First key-legend row; slightly shorter glyphs. */
constexpr int k_foot_ch = 12;
constexpr int k_list_rows = 15;
constexpr int k_list_w = 66; /**< Leave a narrow Break box on the right. */
constexpr int k_box_x = 67;  /**< 13 cols: borders + 1010:0035 + sbar. */

/* CP850 box / scrollbar (not UTF-8 — the glyph atlas is CP850). */
constexpr uint8_t k_box_h = 0xC4;
constexpr uint8_t k_box_v = 0xB3;
constexpr uint8_t k_box_tl = 0xDA;
constexpr uint8_t k_box_tr = 0xBF;
constexpr uint8_t k_box_bl = 0xC0;
constexpr uint8_t k_box_br = 0xD9;
constexpr uint8_t k_sbar = 0xDB;
constexpr uint8_t k_strk = 0xB0;

int row_height(int y)
{
    return (y >= k_foot0) ? k_foot_ch : k_ch;
}

int row_top(int y)
{
    if (y < k_foot0)
    {
        return y * k_ch;
    }
    return k_foot0 * k_ch + (y - k_foot0) * k_foot_ch;
}

int cpu_tex_h(void)
{
    return k_foot0 * k_ch + (k_rows - k_foot0) * k_foot_ch;
}

const uint32_t k_vga[16] = {
    0xFF000000, 0xFF0000A8, 0xFF00A800, 0xFF00A8A8, 0xFFA80000, 0xFFA800A8, 0xFFA85400, 0xFFA8A8A8,
    0xFF545454, 0xFF5454FC, 0xFF54FC54, 0xFF54FCFC, 0xFFFC5454, 0xFFFC54FC, 0xFFFCFC54, 0xFFFCFCFC};

const uint32_t k_cga[4] = {0xFF000000, 0xFF55FFFF, 0xFFFF55FF, 0xFFFFFFFF};

struct cell
{
    uint8_t ch = 0x20;
    uint8_t fg = 15;
    uint8_t bg = 1;
};

struct tdx_ui
{
    SDL_Window *cpu_win = nullptr;
    SDL_Window *game_win = nullptr;
    SDL_Renderer *cpu_ren = nullptr;
    SDL_Renderer *game_ren = nullptr;
    SDL_Texture *cpu_tex = nullptr;
    SDL_Texture *game_tex = nullptr;
    int scale = 2;
    int game_scale = 2;
    bool running = false;
    bool quit = false;
    bool help = false;
    int bp_scroll = 0;
    cell grid[k_rows][k_cols]{};
};

void vcr_pages(rex_session *s, bool fwd)
{
    int i = 0;
    for (i = 0; i < k_list_rows; i++)
    {
        if (fwd)
        {
            (void)rex_session_vcr_forward(s, false);
        }
        else
        {
            (void)rex_session_vcr_back(s);
        }
    }
}

void put_cell(tdx_ui *ui, int x, int y, char ch, uint8_t fg, uint8_t bg)
{
    if ((x < 0) || (y < 0) || (x >= k_cols) || (y >= k_rows) || (ui == nullptr))
    {
        return;
    }
    ui->grid[y][x].ch = (uint8_t)ch;
    ui->grid[y][x].fg = fg;
    ui->grid[y][x].bg = bg;
}

void put_str(tdx_ui *ui, int x, int y, const char *s, uint8_t fg, uint8_t bg)
{
    int i = 0;
    if (s == nullptr)
    {
        return;
    }
    for (i = 0; s[i] != 0; i++)
    {
        put_cell(ui, x + i, y, s[i], fg, bg);
    }
}

void fill_row(tdx_ui *ui, int y, uint8_t bg)
{
    int x = 0;
    for (x = 0; x < k_cols; x++)
    {
        put_cell(ui, x, y, ' ', 15, bg);
    }
}

void fill_row_attr(tdx_ui *ui, int y, uint8_t fg, uint8_t bg)
{
    int x = 0;
    for (x = 0; x < k_cols; x++)
    {
        put_cell(ui, x, y, ' ', fg, bg);
    }
}

void paint_dump16(tdx_ui *ui, int y, const char *tag, uint16_t seg, uint16_t off, rex_session *s,
                  const char *tail)
{
    uint8_t dump[16];
    char hex[80];
    int p = 0;
    size_t i = 0;
    if ((ui == nullptr) || (tag == nullptr) || (s == nullptr))
    {
        return;
    }
    if (rex_session_read_mem(s, rex_segoff_to_linear(seg, off), dump, sizeof(dump)) != REX_OK)
    {
        return;
    }
    p = std::snprintf(hex, sizeof(hex), "%s ", tag);
    for (i = 0; i < sizeof(dump); i++)
    {
        p += std::snprintf(hex + p, sizeof(hex) - (size_t)p, "%02X ", dump[i]);
    }
    if ((tail != nullptr) && (tail[0] != '\0'))
    {
        std::snprintf(hex + p, sizeof(hex) - (size_t)p, " %s", tail);
    }
    put_str(ui, 1, y, hex, 7, 1);
}

void render_cpu(tdx_ui *ui)
{
    uint32_t *pix = nullptr;
    int pitch = 0;
    int y = 0;
    int x = 0;
    assert(ui != nullptr);
    if (SDL_LockTexture(ui->cpu_tex, nullptr, (void **)&pix, &pitch) != 0)
    {
        return;
    }
    for (y = 0; y < k_rows; y++)
    {
        const int rh = row_height(y);
        const int y0 = row_top(y);
        for (x = 0; x < k_cols; x++)
        {
            const cell c = ui->grid[y][x];
            const uint8_t *g = tdx_font_glyph(c.ch);
            const uint32_t fg = k_vga[c.fg & 15];
            const uint32_t bg = k_vga[c.bg & 15];
            int row = 0;
            int col = 0;
            for (row = 0; row < rh; row++)
            {
                uint8_t bits = 0;
                uint32_t *dst = pix + (y0 + row) * (pitch / 4) + x * k_cw;
                if (rh == k_ch)
                {
                    bits = g[row];
                }
                else
                {
                    /* 8×16 cropped to 12 rows (drop 2px top and bottom). */
                    bits = g[row + 2];
                }
                for (col = 0; col < k_cw; col++)
                {
                    dst[col] = (bits & (uint8_t)(0x80 >> col)) ? fg : bg;
                }
            }
        }
    }
    SDL_UnlockTexture(ui->cpu_tex);
    SDL_RenderClear(ui->cpu_ren);
    SDL_RenderCopy(ui->cpu_ren, ui->cpu_tex, nullptr, nullptr);
    SDL_RenderPresent(ui->cpu_ren);
}

void render_game(tdx_ui *ui, rex_session *s)
{
    uint8_t fb[DOS_CGA_PIXELS];
    uint32_t *pix = nullptr;
    int pitch = 0;
    int y = 0;
    int x = 0;
    if ((ui == nullptr) || (ui->game_tex == nullptr) || (s == nullptr))
    {
        return;
    }
    if (rex_session_cga_decode(s, fb, sizeof(fb)) != REX_OK)
    {
        return;
    }
    if (SDL_LockTexture(ui->game_tex, nullptr, (void **)&pix, &pitch) != 0)
    {
        return;
    }
    for (y = 0; y < 200; y++)
    {
        uint32_t *dst = pix + y * (pitch / 4);
        for (x = 0; x < 320; x++)
        {
            dst[x] = k_cga[fb[y * 320 + x] & 3];
        }
    }
    SDL_UnlockTexture(ui->game_tex);
    SDL_RenderClear(ui->game_ren);
    SDL_RenderCopy(ui->game_ren, ui->game_tex, nullptr, nullptr);
    SDL_RenderPresent(ui->game_ren);
}

void paint_bp_box(tdx_ui *ui, rex_session *s, uint16_t cs, uint16_t ip)
{
    const int x0 = k_box_x;
    const int w = k_cols - k_box_x;
    const int h = k_list_rows;
    const int inner = h - 2;
    const int sbar_x = x0 + w - 2;
    rex_bp bps[64];
    size_t n = 0;
    size_t i = 0;
    int y = 0;
    int x = 0;
    int scroll = 0;
    bool bar = false;

    if ((ui == nullptr) || (s == nullptr) || (w < 8) || (inner < 1))
    {
        return;
    }
    n = rex_bp_list(s, bps, sizeof(bps) / sizeof(bps[0]));
    bar = n > (size_t)inner;
    scroll = ui->bp_scroll;
    if (scroll < 0)
    {
        scroll = 0;
    }
    if (bar && (scroll > (int)n - inner))
    {
        scroll = (int)n - inner;
    }
    if (!bar)
    {
        scroll = 0;
    }
    ui->bp_scroll = scroll;

    for (y = 0; y < h; y++)
    {
        for (x = 0; x < w; x++)
        {
            uint8_t ch = (uint8_t)' ';
            if (y == 0)
            {
                ch = (x == 0) ? k_box_tl : ((x == w - 1) ? k_box_tr : k_box_h);
            }
            else if (y == h - 1)
            {
                ch = (x == 0) ? k_box_bl : ((x == w - 1) ? k_box_br : k_box_h);
            }
            else if ((x == 0) || (x == w - 1))
            {
                ch = k_box_v;
            }
            put_cell(ui, x0 + x, y, (char)ch, 14, 1);
        }
    }
    put_str(ui, x0 + 1, 0, "Break", 14, 1);

    for (i = 0; i < (size_t)inner; i++)
    {
        const size_t idx = (size_t)scroll + i;
        char lab[20];
        if (idx >= n)
        {
            break;
        }
        std::snprintf(lab, sizeof(lab), "%04X:%04X", bps[idx].seg, bps[idx].off);
        {
            const bool here = (bps[idx].seg == cs) && (bps[idx].off == ip);
            put_str(ui, x0 + 2, 1 + (int)i, lab, here ? 0 : 15, here ? 14 : 1);
        }
    }
    if (bar)
    {
        int thumb = 0;
        if ((int)n > inner)
        {
            thumb = (scroll * (inner - 1)) / ((int)n - inner);
        }
        for (i = 0; i < (size_t)inner; i++)
        {
            put_cell(ui, sbar_x, 1 + (int)i, (char)k_strk, 8, 1);
        }
        put_cell(ui, sbar_x, 1 + thumb, (char)k_sbar, 15, 1);
    }
}

void paint_cpu(tdx_ui *ui, rex_session *s)
{
    rex_regs_i8086 r{};
    rex_insn ins[16];
    size_t n = 0;
    size_t i = 0;
    char line[160];
    int y = 0;
    const char *stop = "";

    assert(ui != nullptr);
    assert(s != nullptr);
    for (y = 0; y < k_rows; y++)
    {
        fill_row(ui, y, 1);
    }
    rex_session_get_regs_i8086(s, &r);
    rex_session_disasm(s, UINT64_MAX, ins, 16, &n);
    for (i = 0; i < n && (int)i < k_list_rows; i++)
    {
        const bool cur = (i == 0);
        const bool bp = rex_bp_at(s, ins[i].linear);
        uint8_t fg = 15;
        uint8_t bg = 1;
        char bytes[24];
        int b = 0;
        if (bp)
        {
            bg = 12; /* light red — stays until F2 toggles it off */
            fg = 15;
        }
        else if (cur)
        {
            bg = 14;
            fg = 0;
        }
        bytes[0] = 0;
        for (b = 0; b < ins[i].size && b < 6; b++)
        {
            char t[4];
            std::snprintf(t, sizeof(t), "%s%02X", (b == 0) ? "" : " ", ins[i].bytes[b]);
            std::strcat(bytes, t);
        }
        std::snprintf(line, sizeof(line), "%04X:%04X  %-17s %s", ins[i].seg, ins[i].off, bytes,
                      ins[i].text);
        line[k_list_w] = 0;
        put_str(ui, 0, (int)i, line, fg, bg);
        {
            const char *sym = rex_symbols_lookup(s, ins[i].linear);
            if (sym != nullptr)
            {
                put_str(ui, 48, (int)i, sym, 11, bg);
            }
        }
    }
    paint_bp_box(ui, s, r.cs, r.ip);

    std::snprintf(line, sizeof(line), "AX %04X  BX %04X  CX %04X  DX %04X", r.ax, r.bx, r.cx, r.dx);
    put_str(ui, 1, 16, line, 15, 1);
    std::snprintf(line, sizeof(line), "SI %04X  DI %04X  BP %04X  SP %04X", r.si, r.di, r.bp, r.sp);
    put_str(ui, 1, 17, line, 15, 1);
    std::snprintf(line, sizeof(line), "CS %04X  DS %04X  ES %04X  SS %04X  IP %04X", r.cs, r.ds, r.es,
                  r.ss, r.ip);
    put_str(ui, 1, 18, line, 15, 1);
    std::snprintf(line, sizeof(line), "FLAGS %04X  O=%s D=%s I=%s S=%s Z=%s A=%s P=%s C=%s", r.flags,
                  (r.flags & 0x800u) ? "OV" : "NV", (r.flags & 0x400u) ? "DN" : "UP",
                  (r.flags & 0x200u) ? "EI" : "DI", (r.flags & 0x80u) ? "NG" : "PL",
                  (r.flags & 0x40u) ? "ZR" : "NZ", (r.flags & 0x10u) ? "AC" : "NA",
                  (r.flags & 0x04u) ? "PE" : "PO", (r.flags & 0x01u) ? "CY" : "NC");
    put_str(ui, 1, 19, line, 11, 1);

    {
        char cga[12];
        std::snprintf(cga, sizeof(cga), "CGA %02X", rex_session_video_mode(s));
        paint_dump16(ui, 20, "DS:SI", r.ds, r.si, s, cga);
        paint_dump16(ui, 21, "ES:DI", r.es, r.di, s, nullptr);
    }

    switch (rex_session_stop_reason(s))
    {
    case REX_STOP_BREAK:
        stop = "breakpoint";
        break;
    case REX_STOP_HALTED:
        stop = "terminated";
        break;
    case REX_STOP_WAIT_KEY:
        stop = "waiting for key";
        break;
    case REX_STOP_FAULT:
        stop = "cpu fault";
        break;
    default:
        stop = ui->running ? "running" : "stopped";
        break;
    }
    fill_row_attr(ui, 23, 0, 7);
    fill_row_attr(ui, 24, 0, 7);
    put_str(ui, 0, 23, " F1 Help  F2 Break  F7 Into  F8 Over  F9 Run  Ctrl-F2 Reset  Alt-X Quit", 0,
            7);
    put_str(ui, 0, 24, " Up Back  Down Fwd  PgUp  PgDn  Home  End", 0, 7);
    {
        const int sl = (int)std::strlen(stop);
        put_str(ui, k_cols - sl - 1, 24, stop, 0, 7);
    }
}

void paint_help(tdx_ui *ui)
{
    static const char *const lines[] = {
        "F1       This help",
        "F2       Breakpoint at CS:IP",
        "F7       Trace into one instruction",
        "F8       Step over CALL, INT, REP, LOOP",
        "F9       Run / pause",
        "Ctrl-F2  Reset program (keep breakpoints)",
        "Alt-X    Quit",
        "Up       VCR back one unit",
        "Down     VCR forward (step over)",
        "PgUp     Back 14 units",
        "PgDn     Forward 14 units",
        "Home     Tape start",
        "End      Tape end",
    };
    const int n = (int)(sizeof(lines) / sizeof(lines[0]));
    const int y0 = 2;
    const int x0 = 12;
    int i = 0;
    int y = 0;
    if (ui == nullptr)
    {
        return;
    }
    for (y = 0; y <= 22; y++)
    {
        fill_row_attr(ui, y, 0, 7);
    }
    put_str(ui, x0, y0, "Help  (F1 closes)", 14, 7);
    for (i = 0; i < n; i++)
    {
        put_str(ui, x0, y0 + 2 + i, lines[i], 0, 7);
    }
}

int save_texture_bmp(SDL_Renderer *ren, SDL_Texture *tex, int w, int h, const char *path)
{
    SDL_Surface *surf = nullptr;
    SDL_Texture *tgt = nullptr;
    int rc = -1;
    int tw = w;
    int th = h;
    if ((ren == nullptr) || (tex == nullptr) || (path == nullptr))
    {
        return -1;
    }
    SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
    if ((tw < 1) || (th < 1))
    {
        tw = w;
        th = h;
    }
    surf = SDL_CreateRGBSurfaceWithFormat(0, tw, th, 32, SDL_PIXELFORMAT_ARGB8888);
    tgt = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, tw, th);
    if ((surf == nullptr) || (tgt == nullptr))
    {
        if (surf != nullptr)
        {
            SDL_FreeSurface(surf);
        }
        if (tgt != nullptr)
        {
            SDL_DestroyTexture(tgt);
        }
        return -1;
    }
    if (SDL_SetRenderTarget(ren, tgt) == 0)
    {
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, nullptr, nullptr);
        rc = SDL_RenderReadPixels(ren, nullptr, SDL_PIXELFORMAT_ARGB8888, surf->pixels, surf->pitch);
        SDL_SetRenderTarget(ren, nullptr);
        if (rc == 0)
        {
            rc = SDL_SaveBMP(surf, path);
        }
    }
    SDL_DestroyTexture(tgt);
    SDL_FreeSurface(surf);
    return rc;
}
} // namespace

int tdx_ui_shot_cpu(void *user, const char *path)
{
    tdx_ui *ui = static_cast<tdx_ui *>(user);
    if ((ui == nullptr) || (ui->cpu_ren == nullptr))
    {
        return -1;
    }
    return save_texture_bmp(ui->cpu_ren, ui->cpu_tex, k_cols * k_cw, k_rows * k_ch, path);
}

int tdx_ui_shot_game(void *user, const char *path)
{
    tdx_ui *ui = static_cast<tdx_ui *>(user);
    if ((ui == nullptr) || (ui->game_ren == nullptr))
    {
        return -1;
    }
    return save_texture_bmp(ui->game_ren, ui->game_tex, 320, 200, path);
}

int tdx_ui_run(rex_session *session, rex_sock *sock, const tdx_cli *cli)
{
    tdx_ui ui{};
    const int scale = (cli != nullptr && cli->scale > 0) ? cli->scale : 2;
    const int cpu_w = k_cols * k_cw * scale;
    const int cpu_h = cpu_tex_h() * scale;
    const int game_w = 320 * 2;
    const int game_h = 200 * 2;
    SDL_Event ev{};

    assert(session != nullptr);
    ui.scale = scale;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
    {
        std::fprintf(stderr, "tdx: SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    ui.cpu_win = SDL_CreateWindow("TDX " TDX_VERSION_STRING, SDL_WINDOWPOS_UNDEFINED,
                                 SDL_WINDOWPOS_UNDEFINED, cpu_w, cpu_h,
                                 SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);
    ui.cpu_ren = SDL_CreateRenderer(ui.cpu_win, -1, SDL_RENDERER_ACCELERATED);
    ui.cpu_tex = SDL_CreateTexture(ui.cpu_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                   k_cols * k_cw, cpu_tex_h());
    SDL_MaximizeWindow(ui.cpu_win);
    if ((cli != nullptr) && cli->game)
    {
        ui.game_win = SDL_CreateWindow("TDX — User screen", SDL_WINDOWPOS_UNDEFINED,
                                       SDL_WINDOWPOS_UNDEFINED, game_w, game_h, SDL_WINDOW_RESIZABLE);
        ui.game_ren = SDL_CreateRenderer(ui.game_win, -1, SDL_RENDERER_ACCELERATED);
        ui.game_tex = SDL_CreateTexture(ui.game_ren, SDL_PIXELFORMAT_ARGB8888,
                                        SDL_TEXTUREACCESS_STREAMING, 320, 200);
    }
    if (sock != nullptr)
    {
        rex_sock_set_shotters(sock, tdx_ui_shot_cpu, tdx_ui_shot_game, &ui);
    }

    while (!ui.quit)
    {
        while (SDL_PollEvent(&ev))
        {
            if (ev.type == SDL_QUIT)
            {
                ui.quit = true;
            }
            else if (ev.type == SDL_KEYDOWN)
            {
                const SDL_Keymod mod = SDL_GetModState();
                const SDL_Keycode k = ev.key.keysym.sym;
                if (ev.key.repeat)
                {
                    continue;
                }
                if (k == SDLK_F1)
                {
                    ui.help = !ui.help;
                    continue;
                }
                if (ui.help)
                {
                    if (k == SDLK_ESCAPE)
                    {
                        ui.help = false;
                    }
                    continue;
                }
                if (((mod & KMOD_ALT) && (k == SDLK_x)) || (k == SDLK_ESCAPE))
                {
                    ui.quit = true;
                }
                else if (k == SDLK_F7)
                {
                    ui.running = false;
                    (void)rex_session_vcr_forward(session, true);
                }
                else if (k == SDLK_F8)
                {
                    ui.running = false;
                    (void)rex_session_vcr_forward(session, false);
                }
                else if (k == SDLK_F9)
                {
                    ui.running = !ui.running;
                    if (!ui.running)
                    {
                        rex_session_request_stop(session);
                    }
                    else
                    {
                        rex_session_vcr_seed(session);
                    }
                }
                else if ((mod & KMOD_CTRL) && (k == SDLK_F2))
                {
                    ui.running = false;
                    rex_session_reset(session);
                }
                else if (k == SDLK_F2)
                {
                    rex_regs_i8086 r{};
                    uint32_t id = 0;
                    rex_session_get_regs_i8086(session, &r);
                    {
                        const uint64_t lin = rex_segoff_to_linear(r.cs, r.ip);
                        if (rex_bp_at(session, lin))
                        {
                            rex_bp_del_linear(session, lin);
                        }
                        else
                        {
                            rex_bp_add_segoff(session, r.cs, r.ip, &id);
                        }
                    }
                }
                else if (k == SDLK_UP)
                {
                    ui.running = false;
                    (void)rex_session_vcr_back(session);
                }
                else if (k == SDLK_DOWN)
                {
                    ui.running = false;
                    (void)rex_session_vcr_forward(session, false);
                }
                else if (k == SDLK_PAGEUP)
                {
                    ui.running = false;
                    vcr_pages(session, false);
                }
                else if (k == SDLK_PAGEDOWN)
                {
                    ui.running = false;
                    vcr_pages(session, true);
                }
                else if (k == SDLK_HOME)
                {
                    ui.running = false;
                    (void)rex_session_vcr_home(session);
                }
                else if (k == SDLK_END)
                {
                    ui.running = false;
                    (void)rex_session_vcr_end(session);
                }
                else if (k == SDLK_SPACE)
                {
                    rex_session_push_key(session, 32, 0x39);
                }
                else if ((k >= 32) && (k < 127))
                {
                    rex_session_push_key(session, (uint8_t)k, 0);
                }
                else if (k == SDLK_RETURN)
                {
                    rex_session_push_key(session, 13, 0x1C);
                }
            }
        }
        if (sock != nullptr)
        {
            rex_sock_poll(sock, session);
        }
        {
            const int uic = rex_session_take_ui_cmd(session);
            if (uic == REX_UI_TOGGLE_RUN)
            {
                ui.running = !ui.running;
                if (!ui.running)
                {
                    rex_session_request_stop(session);
                }
                else
                {
                    rex_session_vcr_seed(session);
                }
            }
            else if (uic == REX_UI_STOP)
            {
                ui.running = false;
                rex_session_request_stop(session);
            }
            else if (uic == REX_UI_START_RUN)
            {
                if (!ui.running)
                {
                    rex_session_vcr_seed(session);
                }
                ui.running = true;
            }
            else if (uic == REX_UI_LIST_UP)
            {
                ui.running = false;
                (void)rex_session_vcr_back(session);
            }
            else if (uic == REX_UI_LIST_DOWN)
            {
                ui.running = false;
                (void)rex_session_vcr_forward(session, false);
            }
            else if (uic == REX_UI_LIST_HOME)
            {
                ui.running = false;
                (void)rex_session_vcr_home(session);
            }
            else if (uic == REX_UI_LIST_END)
            {
                ui.running = false;
                (void)rex_session_vcr_end(session);
            }
            else if (uic == REX_UI_LIST_PGUP)
            {
                ui.running = false;
                vcr_pages(session, false);
            }
            else if (uic == REX_UI_LIST_PGDN)
            {
                ui.running = false;
                vcr_pages(session, true);
            }
            else if (uic == REX_UI_HELP)
            {
                ui.help = !ui.help;
            }
        }
        if (ui.running && (!rex_session_halted(session)))
        {
            rex_session_run(session, 250000);
            if (rex_session_stop_reason(session) == REX_STOP_BREAK)
            {
                ui.running = false;
            }
            if (rex_session_halted(session))
            {
                ui.running = false;
            }
        }
        paint_cpu(&ui, session);
        if (ui.help)
        {
            paint_help(&ui);
        }
        render_cpu(&ui);
        if (ui.game_win != nullptr)
        {
            render_game(&ui, session);
        }
        SDL_Delay(16);
    }

    if (ui.game_tex != nullptr)
    {
        SDL_DestroyTexture(ui.game_tex);
    }
    if (ui.game_ren != nullptr)
    {
        SDL_DestroyRenderer(ui.game_ren);
    }
    if (ui.game_win != nullptr)
    {
        SDL_DestroyWindow(ui.game_win);
    }
    SDL_DestroyTexture(ui.cpu_tex);
    SDL_DestroyRenderer(ui.cpu_ren);
    SDL_DestroyWindow(ui.cpu_win);
    SDL_Quit();
    return 0;
}
