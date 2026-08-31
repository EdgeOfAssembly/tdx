/**
 * @file rex_log.cpp
 * @brief stderr + optional file logger.
 */

#include "rex/rex_log.h"

#include <cassert>
#include <cstdarg>
#include <cstdio>

namespace
{
FILE *g_file = nullptr;
rex_log_level g_level = REX_LOG_INFO;
} // namespace

void rex_log_set_file(FILE *fp)
{
    g_file = fp;
}

void rex_log_set_level(rex_log_level level)
{
    g_level = level;
}

void rex_log_close_file(void)
{
    if ((g_file != nullptr) && (g_file != stderr) && (g_file != stdout))
    {
        fclose(g_file);
    }
    g_file = nullptr;
}

void rex_logf(rex_log_level level, const char *fmt, ...)
{
    va_list ap;
    va_list ap2;

    assert(fmt != nullptr);
    if (level > g_level)
    {
        return;
    }
    va_start(ap, fmt);
    va_copy(ap2, ap);
    fputs("[tdx] ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    if (g_file != nullptr)
    {
        fputs("[tdx] ", g_file);
        vfprintf(g_file, fmt, ap2);
        fputc('\n', g_file);
        fflush(g_file);
    }
    va_end(ap2);
}
