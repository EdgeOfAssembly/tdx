/**
 * @file tdx_view.cpp
 * @brief tdxview — CGA user-screen in its own SDL2 window / Xmux session.
 *
 * Connects to a running tdx agent socket and polls `cga` frames. No Unicorn
 * in this process: one DISPLAY per session.
 */

#include "dos/dos_cga.h"
#include "tdx/tdx_agent_sock.h"
#include "tdx/tdx_font.h"
#include "tdx/tdx_shot.h"
#include "tdx/tdx_version.h"

#include <SDL.h>
#include <nlohmann/json.hpp>

#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>

using json = nlohmann::json;

namespace
{
const uint32_t k_cga[4] = {0xFF000000, 0xFF55FFFF, 0xFFFF55FF, 0xFFFFFFFF};
const uint32_t k_vga[16] = {
    0xFF000000, 0xFF0000AA, 0xFF00AA00, 0xFF00AAAA, 0xFFAA0000, 0xFFAA00AA, 0xFFAA5500, 0xFFAAAAAA,
    0xFF555555, 0xFF5555FF, 0xFF55FF55, 0xFF55FFFF, 0xFFFF5555, 0xFFFF55FF, 0xFFFFFF55, 0xFFFFFFFF,
};

struct view_cli
{
    bool help = false;
    bool version = false;
    bool usage_error = false;
    std::string sock_path = "/tmp/tdx.sock";
    std::string listen_path = "/tmp/tdxview.sock";
    bool no_listen = false;
    int scale = 2; /**< Graphics only; text is always 640×400. */
};

void print_usage(FILE *fp)
{
    std::fputs(
        "Usage: tdxview [options]\n"
        "\n"
        "  CGA user-screen viewer for TDX. Connects to tdx's UNIX socket and\n"
        "  shows the guest framebuffer. Listens on /tmp/tdxview.sock so agents\n"
        "  can SHOT this window and send keys without Xmux.\n"
        "  Alt-X quits. No args still starts (needs tdx on --sock).\n"
        "\n"
        "Options:\n"
        "  -h, --help           Show this help and exit\n"
        "  -v, --version        Show version and exit\n"
        "      --sock PATH      tdx socket (default: /tmp/tdx.sock)\n"
        "      --listen PATH    agent socket (default: /tmp/tdxview.sock)\n"
        "      --no-listen      Do not listen for agents\n"
        "      --scale N        Graphics scale (default: 2 → 640×400). Text is 640×400.\n"
        "\n"
        "tdxview " TDX_VERSION_STRING "\n",
        fp);
}

bool parse_cli(int argc, char **argv, view_cli *out)
{
    int i = 1;
    if (out == nullptr)
    {
        return false;
    }
    *out = view_cli{};
    for (i = 1; i < argc; i++)
    {
        const char *a = argv[i];
        if ((std::strcmp(a, "-h") == 0) || (std::strcmp(a, "--help") == 0))
        {
            out->help = true;
        }
        else if ((std::strcmp(a, "-v") == 0) || (std::strcmp(a, "--version") == 0))
        {
            out->version = true;
        }
        else if (std::strcmp(a, "--sock") == 0)
        {
            if (i + 1 >= argc)
            {
                out->usage_error = true;
                return false;
            }
            out->sock_path = argv[++i];
        }
        else if (std::strncmp(a, "--sock=", 7) == 0)
        {
            out->sock_path = a + 7;
        }
        else if (std::strcmp(a, "--listen") == 0)
        {
            if (i + 1 >= argc)
            {
                out->usage_error = true;
                return false;
            }
            out->listen_path = argv[++i];
        }
        else if (std::strncmp(a, "--listen=", 9) == 0)
        {
            out->listen_path = a + 9;
        }
        else if (std::strcmp(a, "--no-listen") == 0)
        {
            out->no_listen = true;
        }
        else if (std::strcmp(a, "--scale") == 0)
        {
            if (i + 1 >= argc)
            {
                out->usage_error = true;
                return false;
            }
            out->scale = std::atoi(argv[++i]);
            if (out->scale < 1)
            {
                out->scale = 1;
            }
        }
        else if (a[0] == '-')
        {
            std::fprintf(stderr, "tdxview: unknown option %s\n", a);
            out->usage_error = true;
            return false;
        }
        else
        {
            std::fprintf(stderr, "tdxview: extra operand %s\n", a);
            out->usage_error = true;
            return false;
        }
    }
    return true;
}

int b64_val(char c)
{
    if ((c >= 'A') && (c <= 'Z'))
    {
        return c - 'A';
    }
    if ((c >= 'a') && (c <= 'z'))
    {
        return c - 'a' + 26;
    }
    if ((c >= '0') && (c <= '9'))
    {
        return c - '0' + 52;
    }
    if (c == '+')
    {
        return 62;
    }
    if (c == '/')
    {
        return 63;
    }
    return -1;
}

bool b64_decode(const std::string &in, std::vector<uint8_t> *out)
{
    size_t i = 0;
    unsigned acc = 0;
    int nbits = 0;
    if (out == nullptr)
    {
        return false;
    }
    out->clear();
    out->reserve(in.size() * 3 / 4);
    for (i = 0; i < in.size(); i++)
    {
        const int v = b64_val(in[i]);
        if (in[i] == '=')
        {
            break;
        }
        if (v < 0)
        {
            continue;
        }
        acc = (acc << 6) | (unsigned)v;
        nbits += 6;
        if (nbits >= 8)
        {
            nbits -= 8;
            out->push_back((uint8_t)((acc >> nbits) & 0xFFu));
        }
    }
    return true;
}

int connect_sock(const std::string &path)
{
    sockaddr_un addr{};
    const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        return -1;
    }
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path.c_str());
    if (connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
    {
        close(fd);
        return -1;
    }
    return fd;
}

bool send_line(int fd, const std::string &line)
{
    const std::string msg = line + "\n";
    size_t off = 0;
    while (off < msg.size())
    {
        const ssize_t n = send(fd, msg.data() + off, msg.size() - off, MSG_NOSIGNAL);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return false;
        }
        off += (size_t)n;
    }
    return true;
}

bool recv_line(int fd, std::string *acc, std::string *line)
{
    char buf[8192];
    if ((acc == nullptr) || (line == nullptr))
    {
        return false;
    }
    for (;;)
    {
        const auto pos = acc->find('\n');
        if (pos != std::string::npos)
        {
            *line = acc->substr(0, pos);
            acc->erase(0, pos + 1);
            if ((!line->empty()) && (line->back() == '\r'))
            {
                line->pop_back();
            }
            return true;
        }
        {
            pollfd pfd{};
            int pr = 0;
            pfd.fd = fd;
            pfd.events = POLLIN;
            pr = poll(&pfd, 1, 5000);
            if (pr == 0)
            {
                return false;
            }
            if (pr < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                return false;
            }
        }
        const ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return false;
        }
        if (n == 0)
        {
            return false;
        }
        acc->append(buf, (size_t)n);
        if (acc->size() > 400000u)
        {
            return false;
        }
    }
}

void blit_glyph(uint32_t *pix, int pitch_px, int x, int y, char ch, uint32_t fg, uint32_t bg)
{
    const uint8_t *g = tdx_font_glyph((uint8_t)ch);
    int row = 0;
    int col = 0;
    if ((x < 0) || (y < 0) || (x + 8 > DOS_CGA_WIDTH) || (y + 16 > DOS_CGA_HEIGHT))
    {
        return;
    }
    for (row = 0; row < 16; row++)
    {
        const uint8_t bits = g[row];
        uint32_t *dst = pix + (y + row) * pitch_px + x;
        for (col = 0; col < 8; col++)
        {
            dst[col] = (bits & (uint8_t)(0x80 >> col)) ? fg : bg;
        }
    }
}

void blit_str(uint32_t *pix, int pitch_px, int x, int y, const char *s, uint32_t fg, uint32_t bg)
{
    int i = 0;
    if (s == nullptr)
    {
        return;
    }
    for (i = 0; s[i] != 0; i++)
    {
        blit_glyph(pix, pitch_px, x + i * 8, y, s[i], fg, bg);
    }
}

void send_key(int fd, std::string *acc, SDL_Keycode k)
{
    char line[80];
    std::string ignore;
    const char *named = nullptr;
    if (acc == nullptr)
    {
        return;
    }
    if (k == SDLK_F9)
    {
        send_line(fd, "{\"cmd\":\"run\"}");
        recv_line(fd, acc, &ignore);
        return;
    }
    if (k == SDLK_F7)
    {
        send_line(fd, "{\"cmd\":\"step\"}");
        recv_line(fd, acc, &ignore);
        return;
    }
    if (k == SDLK_F8)
    {
        send_line(fd, "{\"cmd\":\"over\"}");
        recv_line(fd, acc, &ignore);
        return;
    }
    if ((k == SDLK_F2) && (SDL_GetModState() & KMOD_CTRL))
    {
        send_line(fd, "{\"cmd\":\"reset\"}");
        recv_line(fd, acc, &ignore);
        return;
    }
    if (k == SDLK_LEFT)
    {
        named = "Left";
    }
    else if (k == SDLK_RIGHT)
    {
        named = "Right";
    }
    else if (k == SDLK_UP)
    {
        named = "Up";
    }
    else if (k == SDLK_DOWN)
    {
        named = "Down";
    }
    else if (k == SDLK_SPACE)
    {
        named = "Space";
    }
    else if (k == SDLK_RETURN)
    {
        named = "Enter";
    }
    else if (k == SDLK_ESCAPE)
    {
        named = "Esc";
    }
    if (named != nullptr)
    {
        std::snprintf(line, sizeof(line), "{\"cmd\":\"key\",\"key\":\"%s\"}", named);
        send_line(fd, line);
        recv_line(fd, acc, &ignore);
        return;
    }
    if ((k >= 32) && (k < 127) && (k != '"') && (k != '\\'))
    {
        std::snprintf(line, sizeof(line), "{\"cmd\":\"key\",\"key\":\"%c\"}", (char)k);
        send_line(fd, line);
        recv_line(fd, acc, &ignore);
    }
}

int save_tex_bmp(SDL_Renderer *ren, SDL_Texture *tex, const char *path)
{
    SDL_Surface *surf = nullptr;
    int rc = -1;
    int w = DOS_CGA_WIDTH;
    int h = DOS_CGA_HEIGHT;
    (void)tex;
    if ((ren == nullptr) || (path == nullptr))
    {
        return -1;
    }
    SDL_GetRendererOutputSize(ren, &w, &h);
    if ((w < 1) || (h < 1))
    {
        w = DOS_CGA_WIDTH;
        h = DOS_CGA_HEIGHT;
    }
    surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (surf == nullptr)
    {
        return -1;
    }
    rc = SDL_RenderReadPixels(ren, nullptr, SDL_PIXELFORMAT_ARGB8888, surf->pixels, surf->pitch);
    if (rc == 0)
    {
        rc = SDL_SaveBMP(surf, path);
    }
    SDL_FreeSurface(surf);
    return rc;
}

struct view_state
{
    SDL_Renderer *ren = nullptr;
    SDL_Texture *tex = nullptr;
    int *tdx_fd = nullptr;
    std::string *acc = nullptr;
    bool *quit = nullptr;
};

std::string view_handle(void *user, const std::string &line)
{
    view_state *st = static_cast<view_state *>(user);
    json req;
    json resp;
    std::string cmd;
    resp["ok"] = true;
    if (st == nullptr)
    {
        return "{\"ok\":false,\"error\":\"state\"}\n";
    }
    try
    {
        if ((!line.empty()) && (line[0] == '{'))
        {
            req = json::parse(line);
            cmd = req.value("cmd", "");
        }
        else
        {
            cmd = line;
        }
    }
    catch (const std::exception &)
    {
        return "{\"ok\":false,\"error\":\"json\"}\n";
    }
    if ((cmd == "shot") || (cmd == "screenshot") || (cmd == "SHOT"))
    {
        const std::string base = req.value("path", "/tmp/tdx-game.bmp");
        const std::string vpath = tdx_shot_versioned_path(base);
        if (save_tex_bmp(st->ren, st->tex, vpath.c_str()) != 0)
        {
            resp["ok"] = false;
            resp["error"] = "shot";
        }
        else
        {
            resp["game"] = vpath;
        }
        return resp.dump() + "\n";
    }
    if ((cmd == "ping") || (cmd == "PING"))
    {
        resp["pong"] = true;
        return resp.dump() + "\n";
    }
    if (cmd == "quit")
    {
        if (st->quit != nullptr)
        {
            *st->quit = true;
        }
        resp["quit"] = true;
        return resp.dump() + "\n";
    }
    if ((cmd == "key") || (cmd == "run") || (cmd == "F9") || (cmd == "stop") || (cmd == "pause") ||
        (cmd == "unpause") || (cmd == "delay") || (cmd == "faster") || (cmd == "slower") ||
        (cmd == "step") || (cmd == "step-in") || (cmd == "stepin") || (cmd == "over") ||
        (cmd == "step-over") || (cmd == "stepover") || (cmd == "reset") || (cmd == "nav") ||
        (cmd == "bp") ||
        (cmd == "bpint") || (cmd == "bpinsn") || (cmd == "bpm") || (cmd == "bpdel") ||
        (cmd == "bplist"))
    {
        std::string reply;
        std::string wire = line;
        if ((st->tdx_fd == nullptr) || (*st->tdx_fd < 0) || (st->acc == nullptr))
        {
            resp["ok"] = false;
            resp["error"] = "no tdx";
            return resp.dump() + "\n";
        }
        if (wire.empty() || (wire[0] != '{'))
        {
            wire = std::string("{\"cmd\":\"") + cmd + "\"}";
        }
        if (!send_line(*st->tdx_fd, wire) || !recv_line(*st->tdx_fd, st->acc, &reply))
        {
            resp["ok"] = false;
            resp["error"] = "tdx";
            return resp.dump() + "\n";
        }
        if (reply.empty() || (reply.back() != '\n'))
        {
            reply.push_back('\n');
        }
        return reply;
    }
    resp["ok"] = false;
    resp["error"] = "unknown cmd";
    return resp.dump() + "\n";
}
} // namespace

int main(int argc, char **argv)
{
    view_cli cli{};
    (void)std::signal(SIGPIPE, SIG_IGN);
    SDL_Window *win = nullptr;
    SDL_Renderer *ren = nullptr;
    SDL_Texture *tex = nullptr;
    int fd = -1;
    bool quit = false;
    int wait_ticks = 0;
    uint8_t last_mode = 0xFF;
    bool tex_gfx = false; /* start as 640×400 text */
    std::vector<uint8_t> fb;
    std::vector<uint8_t> b800;
    std::string sock_acc;
    tdx_agent_sock *agent = nullptr;
    view_state vst{};

    if (!parse_cli(argc, argv, &cli) || cli.usage_error)
    {
        print_usage(stderr);
        return 2;
    }
    if (cli.help)
    {
        print_usage(stdout);
        return 0;
    }
    if (cli.version)
    {
        std::printf("tdxview %s\n", TDX_VERSION_STRING);
        return 0;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
    {
        std::fprintf(stderr, "tdxview: SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    /* 80×25 CGA/VGA-ish text is 640×400 (8×16 cells). Graphics uses --scale
     * on 320×200 (default 2 → also 640×400). */
    {
        char title[256];
        std::snprintf(title, sizeof(title), "TDXView %s - PID:%d", TDX_VERSION_STRING,
                      (int)getpid());
        win = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 400,
                               SDL_WINDOW_RESIZABLE);
    }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 640, 400);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    vst.ren = ren;
    vst.tex = tex;
    vst.tdx_fd = &fd;
    vst.acc = &sock_acc;
    vst.quit = &quit;
    if (!cli.no_listen)
    {
        agent = tdx_agent_listen(cli.listen_path.c_str());
        if (agent == nullptr)
        {
            std::fprintf(stderr, "tdxview: listen %s failed\n", cli.listen_path.c_str());
        }
        else
        {
            std::fprintf(stderr, "tdxview: agent socket %s\n", cli.listen_path.c_str());
        }
    }

    while (!quit)
    {
        SDL_Event ev{};
        uint32_t *pix = nullptr;
        int pitch = 0;
        int y = 0;
        int x = 0;
        uint8_t mode = 0;
        uint8_t palreg = 0x30;

        while (SDL_PollEvent(&ev))
        {
            if (ev.type == SDL_QUIT)
            {
                quit = true;
            }
            else if (ev.type == SDL_KEYDOWN)
            {
                if (ev.key.repeat)
                {
                    continue;
                }
                if ((SDL_GetModState() & KMOD_ALT) && (ev.key.keysym.sym == SDLK_x))
                {
                    quit = true;
                }
                else if (fd >= 0)
                {
                    send_key(fd, &sock_acc, ev.key.keysym.sym);
                }
            }
        }

        if (fd < 0)
        {
            wait_ticks++;
            if ((wait_ticks % 12) == 1)
            {
                fd = connect_sock(cli.sock_path);
                if (fd >= 0)
                {
                    sock_acc.clear();
                    std::fprintf(stderr, "tdxview: connected %s\n", cli.sock_path.c_str());
                }
            }
        }

        fb.assign((size_t)DOS_CGA_PIXELS, 0);
        if (fd >= 0)
        {
            std::string reply;
            if (!send_line(fd, "{\"cmd\":\"cga\"}") || !recv_line(fd, &sock_acc, &reply))
            {
                close(fd);
                fd = -1;
            }
            else
            {
                try
                {
                    const json j = json::parse(reply);
                    mode = (uint8_t)j.value("mode", 0);
                    last_mode = mode;
                    if (j.contains("b800_b64"))
                    {
                        b64_decode(j["b800_b64"].get<std::string>(), &b800);
                    }
                    if (j.contains("pixels_b64"))
                    {
                        b64_decode(j["pixels_b64"].get<std::string>(), &fb);
                    }
                    palreg = (uint8_t)j.value("cga3d9", 0x30);
                    if (j.contains("guest") && win != nullptr)
                    {
                        char title[256];
                        const std::string guest = j.value("guest", "-");
                        std::snprintf(title, sizeof(title), "TDXView %s %s PID:%d",
                                      TDX_VERSION_STRING, guest.c_str(), (int)getpid());
                        SDL_SetWindowTitle(win, title);
                    }
                }
                catch (const std::exception &)
                {
                    close(fd);
                    fd = -1;
                }
            }
        }

        {
            const bool gfx = (last_mode == 0x04) || (last_mode == 0x05) || (last_mode == 0x06) ||
                             (last_mode == 0x13);
            if (gfx != tex_gfx)
            {
                const int tw = gfx ? DOS_CGA_WIDTH : 640;
                const int th = gfx ? DOS_CGA_HEIGHT : 400;
                SDL_DestroyTexture(tex);
                tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                        tw, th);
                SDL_SetWindowSize(win, gfx ? (tw * cli.scale) : tw, gfx ? (th * cli.scale) : th);
                vst.tex = tex;
                tex_gfx = gfx;
            }
        }

        if (SDL_LockTexture(tex, nullptr, (void **)&pix, &pitch) == 0)
        {
            const int pitch_px = pitch / 4;
            const bool gfx = tex_gfx;
            if (gfx)
            {
                uint32_t pal[4] = {k_cga[0], k_cga[1], k_cga[2], k_cga[3]};
                dos_cga_palette_argb(palreg, pal);
                for (y = 0; y < DOS_CGA_HEIGHT; y++)
                {
                    uint32_t *dst = pix + y * pitch_px;
                    for (x = 0; x < DOS_CGA_WIDTH; x++)
                    {
                        uint8_t c = 0;
                        if ((size_t)y * (size_t)DOS_CGA_WIDTH + (size_t)x < fb.size())
                        {
                            c = fb[(size_t)y * (size_t)DOS_CGA_WIDTH + (size_t)x] & 3u;
                        }
                        dst[x] = pal[c];
                    }
                }
            }
            else if (b800.size() >= 4000u)
            {
                /* 80×25 text at 640×400 — full 8×16 glyphs (CPU window already
                 * does this; 320×200 4×8 downsample was unreadable). */
                int row = 0;
                int col = 0;
                for (row = 0; row < 25; row++)
                {
                    for (col = 0; col < 80; col++)
                    {
                        const uint8_t ch = b800[(size_t)(row * 80 + col) * 2u];
                        const uint8_t at = b800[(size_t)(row * 80 + col) * 2u + 1u];
                        const uint32_t fg = k_vga[at & 15u];
                        const uint32_t bg = k_vga[(at >> 4) & 7u];
                        const uint8_t *g = tdx_font_glyph(ch);
                        int gy = 0;
                        int gx = 0;
                        for (gy = 0; gy < 16; gy++)
                        {
                            const uint8_t bits = g[gy];
                            uint32_t *dst = pix + (row * 16 + gy) * pitch_px + col * 8;
                            for (gx = 0; gx < 8; gx++)
                            {
                                dst[gx] = (bits & (uint8_t)(0x80 >> gx)) ? fg : bg;
                            }
                        }
                    }
                }
            }
            else
            {
                const int th = gfx ? DOS_CGA_HEIGHT : 400;
                const int tw = gfx ? DOS_CGA_WIDTH : 640;
                for (y = 0; y < th; y++)
                {
                    uint32_t *dst = pix + y * pitch_px;
                    for (x = 0; x < tw; x++)
                    {
                        dst[x] = 0xFF000000;
                    }
                }
                if (fd < 0)
                {
                    blit_str(pix, pitch_px, 8, 8, " waiting for tdx ", 0xFF000000, 0xFF55FFFF);
                }
            }
            SDL_UnlockTexture(tex);
        }
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, nullptr, nullptr);
        SDL_RenderPresent(ren);
        if (agent != nullptr)
        {
            (void)tdx_agent_poll(agent, view_handle, &vst);
        }
        /* ~30 Hz present. Jerky animation is tdx running the guest ahead of
         * this poll (F9 slice delay: CPU +/- or tdxctl delay/faster/slower). */
        SDL_Delay(33);
    }

    tdx_agent_close(agent);
    if (fd >= 0)
    {
        close(fd);
    }
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
