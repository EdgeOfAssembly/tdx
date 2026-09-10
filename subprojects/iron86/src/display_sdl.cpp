/**
 * @file display_sdl.cpp
 * @brief Fast SDL2 present for standalone iron86 (not tdx slices).
 */
#include "iron86/display_sdl.h"

#include <SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstring>


namespace iron86
{
namespace
{

const uint32_t k_vga[16] = {
    0xFF000000, 0xFF0000AA, 0xFF00AA00, 0xFF00AAAA, 0xFFAA0000, 0xFFAA00AA, 0xFFAA5500, 0xFFAAAAAA,
    0xFF555555, 0xFF5555FF, 0xFF55FF55, 0xFF55FFFF, 0xFFFF5555, 0xFFFF55FF, 0xFFFFFF55, 0xFFFFFFFF,
};
const uint32_t k_mda_fg = 0xFF33FF33;
const uint32_t k_mda_hi = 0xFF66FF66;
const uint32_t k_mda_bg = 0xFF000000;

SDL_Window *win_ = nullptr;
SDL_Renderer *ren_ = nullptr;
SDL_Texture *tex_ = nullptr;
int scale_ = 2;
uint8_t cga8_[256][8]{};
int font_ok_ = 0;
uint32_t pix_[640 * 400];
uint32_t last_fp_ = 0;

int load_font(const char *path)
{
    FILE *fp = std::fopen(path, "rb");
    uint8_t rom[8192];
    unsigned ch = 0;
    int r = 0;
    if (fp == nullptr)
    {
        return 0;
    }
    if (std::fread(rom, 1, 8192, fp) != 8192)
    {
        std::fclose(fp);
        return 0;
    }
    std::fclose(fp);
    for (ch = 0; ch < 256u; ch++)
    {
        for (r = 0; r < 8; r++)
        {
            cga8_[ch][r] = rom[0x1800u + ch * 8u + static_cast<unsigned>(r)];
        }
    }
    font_ok_ = 1;
    return 1;
}

uint8_t glyph_row(uint8_t ch, int row)
{
    if ((font_ok_ != 0) && (row >= 0) && (row < 8))
    {
        return cga8_[ch][row];
    }
    /* Crude fallback: ASCII as filled block / empty. */
    if ((ch > 32u) && (row >= 1) && (row <= 6))
    {
        return 0x7Eu;
    }
    return 0;
}

void blit_text(cpu &c, uint32_t base, int cols, bool mda)
{
    const uint8_t *vram = c.ram() + (base & 0xFFFFFu);
    const int cell_w = (cols == 40) ? 16 : 8;
    int row = 0;
    int col = 0;
    for (row = 0; row < 25; row++)
    {
        for (col = 0; col < cols; col++)
        {
            const uint8_t ch = vram[(row * cols + col) * 2];
            const uint8_t at = vram[(row * cols + col) * 2 + 1];
            const uint32_t fg = mda ? (((at & 8u) != 0) ? k_mda_hi : k_mda_fg) : k_vga[at & 15u];
            const uint32_t bg = mda ? k_mda_bg : k_vga[(at >> 4) & 7u];
            int gy = 0;
            int gx = 0;
            for (gy = 0; gy < 16; gy++)
            {
                const uint8_t bits = glyph_row(ch, gy / 2);
                uint32_t *dst = pix_ + (row * 16 + gy) * 640 + col * cell_w;
                for (gx = 0; gx < 8; gx++)
                {
                    const uint32_t px = (bits & static_cast<uint8_t>(0x80u >> gx)) ? fg : bg;
                    if (cell_w == 16)
                    {
                        dst[gx * 2] = px;
                        dst[gx * 2 + 1] = px;
                    }
                    else
                    {
                        dst[gx] = px;
                    }
                }
            }
        }
    }
}

void blit_mode6(cpu &c)
{
    const uint8_t *vram = c.ram() + 0xB8000u;
    int y = 0;
    int x = 0;
    for (y = 0; y < 200; y++)
    {
        const uint32_t bank = ((y & 1) != 0) ? 0x2000u : 0u;
        const uint32_t row = bank + static_cast<uint32_t>((y / 2) * 80);
        for (x = 0; x < 640; x++)
        {
            const uint8_t b = vram[row + static_cast<uint32_t>(x / 8)];
            const uint8_t bit = static_cast<uint8_t>(0x80u >> (x & 7));
            const uint32_t px = ((b & bit) != 0) ? 0xFFFFFFFF : 0xFF000000;
            pix_[(y * 2) * 640 + x] = px;
            pix_[(y * 2 + 1) * 640 + x] = px;
        }
    }
}

void blit_mode4(cpu &c, uint8_t pal)
{
    /* pal bit 5 = cyan/magenta vs green/red; bit 4 = intensity; bits 0-3 bg */
    const uint32_t bg = k_vga[pal & 15u];
    const int intense = ((pal & 0x10u) != 0) ? 8 : 0;
    const uint32_t p1 = ((pal & 0x20u) != 0) ? k_vga[static_cast<unsigned>(3 + intense)]
                                             : k_vga[static_cast<unsigned>(2 + intense)];
    const uint32_t p2 = ((pal & 0x20u) != 0) ? k_vga[static_cast<unsigned>(5 + intense)]
                                             : k_vga[static_cast<unsigned>(4 + intense)];
    const uint32_t p3 = k_vga[static_cast<unsigned>(7 + intense)];
    const uint32_t pal4[4] = {bg, p1, p2, p3};
    const uint8_t *vram = c.ram() + 0xB8000u;
    int y = 0;
    int x = 0;
    for (y = 0; y < 200; y++)
    {
        const uint32_t bank = ((y & 1) != 0) ? 0x2000u : 0u;
        const uint32_t row = bank + static_cast<uint32_t>((y / 2) * 80);
        for (x = 0; x < 320; x++)
        {
            const uint8_t b = vram[row + static_cast<uint32_t>(x / 4)];
            const int sh = 6 - ((x & 3) * 2);
            const uint8_t idx = static_cast<uint8_t>((b >> sh) & 3u);
            const uint32_t px = pal4[idx];
            pix_[(y * 2) * 640 + x * 2] = px;
            pix_[(y * 2) * 640 + x * 2 + 1] = px;
            pix_[(y * 2 + 1) * 640 + x * 2] = px;
            pix_[(y * 2 + 1) * 640 + x * 2 + 1] = px;
        }
    }
}

void inject_key(pc &p, SDL_Keycode k)
{
    /* ROM INT 16 is stolen: AH=00 reads kbd_ ASCII, not PPI scancodes.
     * type_scan() pushes NUL and DOS ignores it at the date prompt. */
    char s[2] = {0, 0};
    if ((k == SDLK_RETURN) || (k == SDLK_KP_ENTER))
    {
        s[0] = '\r';
        p.type_keys(s);
        return;
    }
    if (k == SDLK_ESCAPE)
    {
        s[0] = '\x1b';
        p.type_keys(s);
        return;
    }
    if (k == SDLK_BACKSPACE)
    {
        s[0] = '\b';
        p.type_keys(s);
    }
}

} // namespace

bool display_sdl_open(int scale)
{
    if (scale < 1)
    {
        scale = 1;
    }
    if (scale > 8)
    {
        scale = 8;
    }
    scale_ = scale;
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
    {
        std::fprintf(stderr, "iron86: SDL_Init: %s\n", SDL_GetError());
        return false;
    }
    if (!load_font("/mnt/TurboDebugger/ROM/IBM_5788005_AM9264_1981_CGA_MDA_CARD.BIN"))
    {
        (void)load_font("ROM/IBM_5788005_AM9264_1981_CGA_MDA_CARD.BIN");
    }
    /* 4:3 window; 640×400 content letterboxed. */
    win_ = SDL_CreateWindow("iron86", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640 * scale_,
                            480 * scale_, 0);
    if (win_ == nullptr)
    {
        std::fprintf(stderr, "iron86: SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }
    /* Accelerated + vsync off. Software Present was sluggish; vsync-on GPU
     * Present can poll-block if the window is unmapped. */
    ren_ = SDL_CreateRenderer(win_, -1, SDL_RENDERER_ACCELERATED);
    if (ren_ == nullptr)
    {
        std::fprintf(stderr, "iron86: SDL_CreateRenderer: %s\n", SDL_GetError());
        display_sdl_close();
        return false;
    }
    /* Vsync Present can block forever if the window is unmapped/obscured
     * (do_sys_poll); then the CPU never reaches the date prompt. */
#if SDL_VERSION_ATLEAST(2, 0, 18)
    (void)SDL_RenderSetVSync(ren_, 0);
#endif
    (void)SDL_GL_SetSwapInterval(0);
    tex_ = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 640, 400);
    SDL_StartTextInput();
    return tex_ != nullptr;
}

void display_sdl_present(cpu &c, bool mda)
{
    const uint8_t mode = c.mem_read8(0x449u);
    const uint8_t pal = c.mem_read8(0x466u);
    const uint8_t *vram = c.ram() + ((mda || (mode == 7u)) ? 0xB0000u : 0xB8000u);
    uint32_t fp = static_cast<uint32_t>(mode) | (static_cast<uint32_t>(pal) << 8);
    unsigned i = 0;
    if (tex_ == nullptr)
    {
        return;
    }
    for (i = 0; i < 4000u; i++)
    {
        fp = fp * 16777619u + vram[i];
    }
    if (fp == last_fp_)
    {
        SDL_RenderPresent(ren_);
        return;
    }
    last_fp_ = fp;
    if ((mode == 6u) || (mode == 0x06u))
    {
        blit_mode6(c);
    }
    else if ((mode == 4u) || (mode == 5u))
    {
        blit_mode4(c, pal);
    }
    else if (mode == 1u)
    {
        blit_text(c, 0xB8000u, 40, false);
    }
    else if (mode == 0u)
    {
        /* BDA 40:49 is 0 until BIOS AH=00; treat as 80×25 CGA, not mode 0. */
        blit_text(c, 0xB8000u, 80, false);
    }
    else if (mda || (mode == 7u))
    {
        blit_text(c, 0xB0000u, 80, true);
    }
    else
    {
        blit_text(c, 0xB8000u, 80, false);
    }
    SDL_UpdateTexture(tex_, nullptr, pix_, 640 * 4);
    SDL_SetRenderDrawColor(ren_, 0, 0, 0, 255);
    SDL_RenderClear(ren_);
    {
        SDL_Rect dst{};
        /* 640×400 into 640×480 letterbox (4:3). */
        dst.x = 0;
        dst.w = 640 * scale_;
        dst.h = 400 * scale_;
        dst.y = (480 * scale_ - dst.h) / 2;
        SDL_RenderCopy(ren_, tex_, nullptr, &dst);
    }
    SDL_RenderPresent(ren_);
}

bool display_sdl_poll(pc &p)
{
    SDL_Event ev{};
    while (SDL_PollEvent(&ev) != 0)
    {
        if (ev.type == SDL_QUIT)
        {
            return false;
        }
        if (ev.type == SDL_TEXTINPUT)
        {
            const char *t = ev.text.text;
            if ((t != nullptr) && (t[0] >= 32) && (t[0] < 127))
            {
                char s[2] = {t[0], 0};
                p.type_keys(s);
            }
        }
        else if (ev.type == SDL_KEYDOWN)
        {
            if (ev.key.repeat)
            {
                continue;
            }
            if ((SDL_GetModState() & KMOD_ALT) && (ev.key.keysym.sym == SDLK_F4))
            {
                return false;
            }
            const SDL_Keycode k = ev.key.keysym.sym;
            if ((k < 32) || (k >= 127))
            {
                inject_key(p, k);
            }
        }
    }
    return true;
}

void display_sdl_close()
{
    if (tex_ != nullptr)
    {
        SDL_DestroyTexture(tex_);
        tex_ = nullptr;
    }
    if (ren_ != nullptr)
    {
        SDL_DestroyRenderer(ren_);
        ren_ = nullptr;
    }
    if (win_ != nullptr)
    {
        SDL_DestroyWindow(win_);
        win_ = nullptr;
    }
    SDL_Quit();
}

} // namespace iron86
