/**
 * @file tdx_agent_sock.cpp
 * @brief Keep-alive UNIX-domain JSON-line accept/poll loop.
 */

#include "tdx/tdx_agent_sock.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <new>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>

struct tdx_agent_client
{
    int fd = -1;
    std::string inbuf;
};

struct tdx_agent_sock
{
    int listen_fd = -1;
    std::string path;
    std::vector<tdx_agent_client> clients;
};

static bool send_all(int fd, const std::string &s)
{
    size_t off = 0;
    while (off < s.size())
    {
        const ssize_t n = write(fd, s.data() + off, s.size() - off);
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

tdx_agent_sock *tdx_agent_listen(const char *path)
{
    tdx_agent_sock *sk = nullptr;
    sockaddr_un addr{};
    if ((path == nullptr) || (path[0] == '\0'))
    {
        return nullptr;
    }
    sk = new (std::nothrow) tdx_agent_sock();
    if (sk == nullptr)
    {
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
        unlink(path);
        delete sk;
        return nullptr;
    }
    fcntl(sk->listen_fd, F_SETFL, O_NONBLOCK);
    return sk;
}

void tdx_agent_close(tdx_agent_sock *sk)
{
    size_t i = 0;
    if (sk == nullptr)
    {
        return;
    }
    for (i = 0; i < sk->clients.size(); i++)
    {
        if (sk->clients[i].fd >= 0)
        {
            close(sk->clients[i].fd);
        }
    }
    if (sk->listen_fd >= 0)
    {
        close(sk->listen_fd);
    }
    if (!sk->path.empty())
    {
        unlink(sk->path.c_str());
    }
    delete sk;
}

int tdx_agent_poll(tdx_agent_sock *sk, tdx_agent_fn fn, void *user)
{
    int handled = 0;
    char buf[4096];
    size_t ci = 0;
    if ((sk == nullptr) || (fn == nullptr) || (sk->listen_fd < 0))
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
        sk->clients.push_back(tdx_agent_client{c, std::string()});
    }
    for (ci = 0; ci < sk->clients.size();)
    {
        tdx_agent_client &cl = sk->clients[ci];
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
            std::string line;
            std::string out;
            if (pos == std::string::npos)
            {
                break;
            }
            line = cl.inbuf.substr(0, pos);
            cl.inbuf.erase(0, pos + 1);
            if ((!line.empty()) && (line.back() == '\r'))
            {
                line.pop_back();
            }
            out = fn(user, line);
            if (out.empty() || (out.back() != '\n'))
            {
                out.push_back('\n');
            }
            if (!send_all(cl.fd, out))
            {
                drop = true;
                break;
            }
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
