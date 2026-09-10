/**
 * @file log.cpp
 * @brief write(2) diagnostics; optional append log file.
 */
#include "iron86/log.h"

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace iron86
{

static int g_log_fd = -1;

void log_init_stdio()
{
    (void)std::setvbuf(stdin, nullptr, _IONBF, 0);
    (void)std::setvbuf(stdout, nullptr, _IONBF, 0);
    (void)std::setvbuf(stderr, nullptr, _IONBF, 0);
}

bool log_open(const char *path)
{
    if ((path == nullptr) || (path[0] == '\0'))
    {
        return false;
    }
    log_close();
    const int fd = ::open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if (fd < 0)
    {
        return false;
    }
    g_log_fd = fd;
    return true;
}

void log_close()
{
    if (g_log_fd < 0)
    {
        return;
    }
    (void)::fsync(g_log_fd);
    (void)::close(g_log_fd);
    g_log_fd = -1;
}

bool log_active()
{
    return g_log_fd >= 0;
}

void fd_write(int fd, const char *s, size_t n)
{
    if ((s == nullptr) && (n > 0u))
    {
        return;
    }
    while (n > 0u)
    {
        const ssize_t w = ::write(fd, s, n);
        if (w <= 0)
        {
            return;
        }
        s += static_cast<size_t>(w);
        n -= static_cast<size_t>(w);
    }
}

void fd_puts(int fd, const char *s)
{
    if (s == nullptr)
    {
        return;
    }
    fd_write(fd, s, std::strlen(s));
}

void fd_printf(int fd, const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    const int n = std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n <= 0)
    {
        return;
    }
    const size_t cap = sizeof(buf) - 1u;
    const size_t w = (static_cast<size_t>(n) < cap) ? static_cast<size_t>(n) : cap;
    fd_write(fd, buf, w);
}

void log_event(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    const int n = std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n <= 0)
    {
        return;
    }
    const size_t cap = sizeof(buf) - 1u;
    const size_t w = (static_cast<size_t>(n) < cap) ? static_cast<size_t>(n) : cap;
    fd_write(STDERR_FILENO, buf, w);
    if (g_log_fd >= 0)
    {
        fd_write(g_log_fd, buf, w);
    }
}

void log_trace(const char *fmt, ...)
{
    if (g_log_fd < 0)
    {
        return;
    }
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    const int n = std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n <= 0)
    {
        return;
    }
    const size_t cap = sizeof(buf) - 1u;
    const size_t w = (static_cast<size_t>(n) < cap) ? static_cast<size_t>(n) : cap;
    fd_write(g_log_fd, buf, w);
}

bool lock_region(void *p, size_t n, const char *name)
{
    const char *tag = (name != nullptr) ? name : "?";
    if ((p == nullptr) || (n == 0u))
    {
        return true;
    }
    if (::mlock(p, n) == 0)
    {
        char hs[32];
        format_human_size(n, hs, sizeof(hs));
        log_event("lock-mem %s %s ok\n", tag, hs);
        return true;
    }
    const int e = errno;
    log_event("lock-mem %s failed errno=%d\n", tag, e);
    return false;
}

void format_human_size(uint64_t n, char *out, size_t cap)
{
    if ((out == nullptr) || (cap == 0u))
    {
        return;
    }
    if (n < 1024ull)
    {
        (void)std::snprintf(out, cap, "%lluB", static_cast<unsigned long long>(n));
        return;
    }
    if (n < (1024ull * 1024ull))
    {
        if ((n % 1024ull) == 0ull)
        {
            (void)std::snprintf(out, cap, "%lluKB",
                                static_cast<unsigned long long>(n / 1024ull));
        }
        else
        {
            (void)std::snprintf(out, cap, "%.1fKB", static_cast<double>(n) / 1024.0);
        }
        return;
    }
    if (n < (1024ull * 1024ull * 1024ull))
    {
        const double mb = static_cast<double>(n) / 1048576.0;
        if (mb >= 10.0)
        {
            (void)std::snprintf(out, cap, "%.0fMB", mb);
        }
        else
        {
            (void)std::snprintf(out, cap, "%.2fMB", mb);
        }
        return;
    }
    if (n < (1024ull * 1024ull * 1024ull * 1024ull))
    {
        const double gb = static_cast<double>(n) / 1073741824.0;
        if (gb >= 10.0)
        {
            (void)std::snprintf(out, cap, "%.0fGB", gb);
        }
        else
        {
            (void)std::snprintf(out, cap, "%.2fGB", gb);
        }
        return;
    }
    const double tb = static_cast<double>(n) / 1099511627776.0;
    if (tb >= 10.0)
    {
        (void)std::snprintf(out, cap, "%.0fTB", tb);
    }
    else
    {
        (void)std::snprintf(out, cap, "%.2fTB", tb);
    }
}

} // namespace iron86
