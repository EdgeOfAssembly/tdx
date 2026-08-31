/**
 * @file rex_sock.cpp
 * @brief UNIX-domain agent socket (JSON lines or bare words).
 */

#include "rex/rex_sock.h"

#include "dos/dos_cga.h"
#include "rex/rex_log.h"
#include "tdx/tdx_shot.h"

#include <nlohmann/json.hpp>

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <sstream>
#include <fcntl.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>

using json = nlohmann::json;

struct rex_client
{
    int fd = -1;
    std::string inbuf;
};

struct rex_sock
{
    int listen_fd = -1;
    std::vector<rex_client> clients;
    std::string path;
    std::string last_cpu;
    std::string last_game;
    rex_shot_fn cpu_shot = nullptr;
    rex_shot_fn game_shot = nullptr;
    void *shot_user = nullptr;
    bool quit_req = false;
};

static std::string b64_encode(const uint8_t *data, size_t n)
{
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    size_t i = 0;
    assert(data != nullptr || n == 0);
    out.reserve(((n + 2) / 3) * 4);
    for (i = 0; i + 2 < n; i += 3)
    {
        const unsigned v = ((unsigned)data[i] << 16) | ((unsigned)data[i + 1] << 8) | data[i + 2];
        out.push_back(tbl[(v >> 18) & 63]);
        out.push_back(tbl[(v >> 12) & 63]);
        out.push_back(tbl[(v >> 6) & 63]);
        out.push_back(tbl[v & 63]);
    }
    if (i < n)
    {
        unsigned v = (unsigned)data[i] << 16;
        if (i + 1 < n)
        {
            v |= (unsigned)data[i + 1] << 8;
        }
        out.push_back(tbl[(v >> 18) & 63]);
        out.push_back(tbl[(v >> 12) & 63]);
        out.push_back((i + 1 < n) ? tbl[(v >> 6) & 63] : '=');
        out.push_back('=');
    }
    return out;
}

static bool parse_addr(const std::string &s, uint64_t *lin)
{
    unsigned seg = 0;
    unsigned off = 0;
    unsigned long v = 0;
    assert(lin != nullptr);
    if (std::sscanf(s.c_str(), "%x:%x", &seg, &off) == 2)
    {
        *lin = rex_segoff_to_linear((uint16_t)seg, (uint16_t)off);
        return true;
    }
    if (std::sscanf(s.c_str(), "%li", (long *)&v) == 1)
    {
        *lin = v;
        return true;
    }
    if (std::sscanf(s.c_str(), "%lx", &v) == 1)
    {
        *lin = v;
        return true;
    }
    return false;
}

static void send_all(int fd, const std::string &s)
{
    const char *p = s.c_str();
    size_t left = s.size();
    while (left > 0)
    {
        const ssize_t n = write(fd, p, left);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }
        p += n;
        left -= (size_t)n;
    }
}

static json regs_json(rex_session *s)
{
    rex_regs_i8086 r{};
    json j;
    rex_session_get_regs_i8086(s, &r);
    j["ax"] = r.ax;
    j["bx"] = r.bx;
    j["cx"] = r.cx;
    j["dx"] = r.dx;
    j["si"] = r.si;
    j["di"] = r.di;
    j["bp"] = r.bp;
    j["sp"] = r.sp;
    j["cs"] = r.cs;
    j["ds"] = r.ds;
    j["es"] = r.es;
    j["ss"] = r.ss;
    j["ip"] = r.ip;
    j["flags"] = r.flags;
    j["csip"] = json::array({r.cs, r.ip});
    return j;
}

static std::string handle_line(rex_sock *sk, rex_session *s, const std::string &line)
{
    json req;
    json resp;
    std::string cmd;
    resp["ok"] = true;

    if (line.empty())
    {
        resp["ok"] = false;
        resp["error"] = "empty";
        return resp.dump() + "\n";
    }
    if (line[0] == '{')
    {
        try
        {
            req = json::parse(line);
            cmd = req.value("cmd", "");
        }
        catch (const std::exception &ex)
        {
            resp["ok"] = false;
            resp["error"] = ex.what();
            return resp.dump() + "\n";
        }
    }
    else
    {
        std::istringstream is(line);
        is >> cmd;
        std::string rest;
        std::getline(is, rest);
        req["cmd"] = cmd;
        req["_rest"] = rest;
    }

    if ((cmd == "step") || (cmd == "t") || (cmd == "F7"))
    {
        rex_session_step(s);
    }
    else if ((cmd == "over") || (cmd == "p") || (cmd == "F8"))
    {
        rex_session_step_over(s, 0);
    }
    else if ((cmd == "run") || (cmd == "F9"))
    {
        /* Do not run the CPU here — that blocks the SDL loop. Toggle F9. */
        rex_session_post_ui_cmd(s, REX_UI_TOGGLE_RUN);
    }
    else if (cmd == "stop")
    {
        rex_session_post_ui_cmd(s, REX_UI_STOP);
        rex_session_request_stop(s);
    }
    else if (cmd == "regs")
    {
        resp["regs"] = regs_json(s);
    }
    else if (cmd == "disasm")
    {
        rex_insn ins[16];
        size_t n = 0;
        size_t i = 0;
        json arr = json::array();
        rex_session_disasm(s, UINT64_MAX, ins, 16, &n);
        for (i = 0; i < n; i++)
        {
            json e;
            e["linear"] = ins[i].linear;
            e["seg"] = ins[i].seg;
            e["off"] = ins[i].off;
            e["text"] = ins[i].text;
            e["size"] = ins[i].size;
            arr.push_back(e);
        }
        resp["insns"] = arr;
    }
    else if (cmd == "mem")
    {
        std::string addr = req.value("addr", "");
        size_t len = req.value("len", 64);
        uint64_t lin = 0;
        if (addr.empty() && req.contains("_rest"))
        {
            std::istringstream is(req["_rest"].get<std::string>());
            is >> addr >> len;
        }
        if (!parse_addr(addr, &lin))
        {
            resp["ok"] = false;
            resp["error"] = "bad addr";
        }
        else
        {
            if (len > 4096)
            {
                len = 4096;
            }
            std::vector<uint8_t> buf(len);
            if (rex_session_read_mem(s, lin, buf.data(), len) != REX_OK)
            {
                resp["ok"] = false;
                resp["error"] = "read";
            }
            else
            {
                resp["linear"] = lin;
                resp["hex"] = json::binary(buf); /* fallback below if dump */
                std::string hex;
                size_t i = 0;
                hex.reserve(len * 3);
                for (i = 0; i < len; i++)
                {
                    char tmp[4];
                    std::snprintf(tmp, sizeof(tmp), "%02X", buf[i]);
                    if (i)
                    {
                        hex.push_back(' ');
                    }
                    hex += tmp;
                }
                resp.erase("hex");
                resp["data"] = hex;
            }
        }
    }
    else if ((cmd == "bp") || (cmd == "bp_set"))
    {
        std::string addr = req.value("addr", "");
        uint64_t lin = 0;
        uint32_t id = 0;
        if (addr.empty() && req.contains("_rest"))
        {
            std::istringstream is(req["_rest"].get<std::string>());
            is >> addr;
        }
        if (!parse_addr(addr, &lin))
        {
            resp["ok"] = false;
            resp["error"] = "bad addr";
        }
        else
        {
            rex_bp_add_linear(s, lin, &id);
            resp["id"] = id;
            resp["linear"] = lin;
        }
    }
    else if ((cmd == "bpdel") || (cmd == "bp_del"))
    {
        uint32_t id = req.value("id", 0);
        if ((id == 0) && req.contains("_rest"))
        {
            id = (uint32_t)std::strtoul(req["_rest"].get<std::string>().c_str(), nullptr, 0);
        }
        if (rex_bp_del(s, id) != REX_OK)
        {
            resp["ok"] = false;
            resp["error"] = "no such bp";
        }
    }
    else if (cmd == "bplist")
    {
        resp["count"] = rex_bp_count(s);
    }
    else if ((cmd == "cga") || (cmd == "video") || (cmd == "frame"))
    {
        uint8_t px[DOS_CGA_PIXELS];
        const uint8_t mode = rex_session_video_mode(s);
        resp["mode"] = mode;
        resp["w"] = DOS_CGA_WIDTH;
        resp["h"] = DOS_CGA_HEIGHT;
        resp["con"] = rex_session_con_out(s);
        if (rex_session_cga_decode(s, px, sizeof(px)) == REX_OK)
        {
            resp["pixels_b64"] = b64_encode(px, sizeof(px));
        }
        else
        {
            resp["ok"] = false;
            resp["error"] = "cga decode";
        }
    }
    else if ((cmd == "shot") || (cmd == "screenshot"))
    {
        const std::string base = req.value("path", "/tmp/tdx-cpu.bmp");
        const std::string vpath = tdx_shot_versioned_path(base);
        if (sk->cpu_shot != nullptr)
        {
            if (sk->cpu_shot(sk->shot_user, vpath.c_str()) == 0)
            {
                sk->last_cpu = vpath;
                resp["cpu"] = vpath;
            }
            else
            {
                resp["ok"] = false;
                resp["error"] = "cpu shot";
            }
        }
        else
        {
            resp["ok"] = false;
            resp["error"] = "no cpu window";
        }
        if (sk->game_shot != nullptr)
        {
            const std::string gpath = tdx_shot_versioned_path("/tmp/tdx-game.bmp");
            if (sk->game_shot(sk->shot_user, gpath.c_str()) == 0)
            {
                sk->last_game = gpath;
                resp["game"] = gpath;
            }
        }
    }
    else if ((cmd == "ping") || (cmd == "PING"))
    {
        resp["pong"] = true;
    }
    else if (cmd == "key")
    {
        std::string k = req.value("key", "");
        if (k.empty() && req.contains("_rest"))
        {
            std::istringstream is(req["_rest"].get<std::string>());
            is >> k;
        }
        if (k.size() == 1)
        {
            rex_session_push_key(s, (uint8_t)k[0], 0);
        }
        else if ((k == "Enter") || (k == "Return"))
        {
            rex_session_push_key(s, 13, 0x1C);
        }
        else if (k == "Esc")
        {
            rex_session_push_key(s, 27, 0x01);
        }
        else if ((k == "Space") || (k == "space"))
        {
            rex_session_push_key(s, 32, 0x39);
        }
        else if ((k == "Left") || (k == "left"))
        {
            rex_session_push_key(s, 0, 0x4B);
        }
        else if ((k == "Right") || (k == "right"))
        {
            rex_session_push_key(s, 0, 0x4D);
        }
        else if ((k == "Up") || (k == "up"))
        {
            rex_session_push_key(s, 0, 0x48);
        }
        else if ((k == "Down") || (k == "down"))
        {
            rex_session_push_key(s, 0, 0x50);
        }
        rex_session_post_ui_cmd(s, REX_UI_START_RUN);
        resp["key"] = k;
    }
    else if (cmd == "nav")
    {
        std::string k = req.value("key", "");
        int nav = REX_UI_NONE;
        if (k.empty() && req.contains("_rest"))
        {
            std::istringstream is(req["_rest"].get<std::string>());
            is >> k;
        }
        if ((k == "Up") || (k == "up"))
        {
            nav = REX_UI_LIST_UP;
        }
        else if ((k == "Down") || (k == "down"))
        {
            nav = REX_UI_LIST_DOWN;
        }
        else if ((k == "Home") || (k == "home"))
        {
            nav = REX_UI_LIST_HOME;
        }
        else if ((k == "End") || (k == "end"))
        {
            nav = REX_UI_LIST_END;
        }
        else if ((k == "PgUp") || (k == "PageUp") || (k == "pgup"))
        {
            nav = REX_UI_LIST_PGUP;
        }
        else if ((k == "PgDn") || (k == "PageDown") || (k == "pgdn"))
        {
            nav = REX_UI_LIST_PGDN;
        }
        if (nav != REX_UI_NONE)
        {
            rex_session_post_ui_cmd(s, nav);
        }
        else
        {
            resp["ok"] = false;
            resp["error"] = "bad nav";
        }
        resp["nav"] = k;
    }
    else if (cmd == "status")
    {
        resp["halted"] = rex_session_halted(s);
        resp["stop"] = (int)rex_session_stop_reason(s);
        resp["regs"] = regs_json(s);
        resp["con"] = rex_session_con_out(s);
    }
    else if (cmd == "reset")
    {
        if (rex_session_reset(s) != REX_OK)
        {
            resp["ok"] = false;
            resp["error"] = "reset";
        }
    }
    else if (cmd == "quit")
    {
        resp["quit"] = true;
        sk->quit_req = true;
    }
    else if ((cmd == "help") || (cmd == "?"))
    {
        resp["cmds"] =
            "step over run stop reset regs disasm mem bp bpdel bplist shot key nav status cga ping quit";
    }
    else
    {
        resp["ok"] = false;
        resp["error"] = "unknown cmd";
        resp["cmd"] = cmd;
    }
    resp["stop"] = (int)rex_session_stop_reason(s);
    resp["halted"] = rex_session_halted(s);
    if (!resp.contains("regs"))
    {
        resp["ip"] = regs_json(s)["ip"];
        resp["cs"] = regs_json(s)["cs"];
    }
    return resp.dump() + "\n";
}

rex_sock *rex_sock_listen(const char *path)
{
    rex_sock *sk = new (std::nothrow) rex_sock();
    sockaddr_un addr{};
    if ((sk == nullptr) || (path == nullptr) || (path[0] == '\0'))
    {
        delete sk;
        return nullptr;
    }
    sk->path = path;
    unlink(path);
    sk->listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sk->listen_fd < 0)
    {
        delete sk;
        return nullptr;
    }
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
    if (bind(sk->listen_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
    {
        close(sk->listen_fd);
        delete sk;
        return nullptr;
    }
    if (listen(sk->listen_fd, 16) < 0)
    {
        close(sk->listen_fd);
        delete sk;
        return nullptr;
    }
    fcntl(sk->listen_fd, F_SETFL, O_NONBLOCK);
    rex_logf(REX_LOG_INFO, "agent socket %s", path);
    return sk;
}

void rex_sock_close(rex_sock *sk)
{
    if (sk == nullptr)
    {
        return;
    }
    for (rex_client &c : sk->clients)
    {
        if (c.fd >= 0)
        {
            close(c.fd);
            c.fd = -1;
        }
    }
    sk->clients.clear();
    if (sk->listen_fd >= 0)
    {
        close(sk->listen_fd);
        sk->listen_fd = -1;
    }
    if (!sk->path.empty())
    {
        unlink(sk->path.c_str());
    }
    delete sk;
}

const char *rex_sock_path(const rex_sock *sk)
{
    return (sk != nullptr) ? sk->path.c_str() : "";
}

void rex_sock_set_shotters(rex_sock *sk, rex_shot_fn cpu, rex_shot_fn game, void *user)
{
    if (sk == nullptr)
    {
        return;
    }
    sk->cpu_shot = cpu;
    sk->game_shot = game;
    sk->shot_user = user;
}

const char *rex_sock_last_cpu_shot(const rex_sock *sk)
{
    return (sk != nullptr) ? sk->last_cpu.c_str() : "";
}

const char *rex_sock_last_game_shot(const rex_sock *sk)
{
    return (sk != nullptr) ? sk->last_game.c_str() : "";
}

bool rex_sock_quit_requested(const rex_sock *sk)
{
    return (sk != nullptr) && sk->quit_req;
}

int rex_sock_poll(rex_sock *sk, rex_session *s)
{
    int handled = 0;
    char buf[4096];
    size_t ci = 0;
    if ((sk == nullptr) || (s == nullptr) || (sk->listen_fd < 0))
    {
        return 0;
    }
    for (;;)
    {
        const int c = accept(sk->listen_fd, nullptr, nullptr);
        if (c < 0)
        {
            break;
        }
        fcntl(c, F_SETFL, O_NONBLOCK);
        sk->clients.push_back(rex_client{c, std::string()});
        rex_logf(REX_LOG_INFO, "agent connected fd=%d n=%zu", c, sk->clients.size());
    }
    for (ci = 0; ci < sk->clients.size();)
    {
        rex_client &cl = sk->clients[ci];
        bool drop = false;
        for (;;)
        {
            const ssize_t n = read(cl.fd, buf, sizeof(buf));
            if (n > 0)
            {
                cl.inbuf.append(buf, (size_t)n);
                if (n < (ssize_t)sizeof(buf))
                {
                    break;
                }
            }
            else if (n == 0)
            {
                drop = true;
                break;
            }
            else
            {
                if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR))
                {
                    break;
                }
                drop = true;
                break;
            }
        }
        while (!drop)
        {
            const auto pos = cl.inbuf.find('\n');
            if (pos == std::string::npos)
            {
                break;
            }
            std::string line = cl.inbuf.substr(0, pos);
            cl.inbuf.erase(0, pos + 1);
            if ((!line.empty()) && (line.back() == '\r'))
            {
                line.pop_back();
            }
            const std::string out = handle_line(sk, s, line);
            send_all(cl.fd, out);
            handled++;
        }
        if (drop)
        {
            close(cl.fd);
            sk->clients.erase(sk->clients.begin() + static_cast<std::ptrdiff_t>(ci));
        }
        else
        {
            ci++;
        }
    }
    return handled;
}
