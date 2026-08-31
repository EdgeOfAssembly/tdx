/**
 * @file tdx_shot.cpp
 * @brief Auto-versioned screenshot paths (Xmux naming).
 */

#include "tdx/tdx_shot.h"

#include <cstdio>
#include <ctime>
#include <string>
#include <unistd.h>

namespace
{
bool file_exists(const std::string &path)
{
    return access(path.c_str(), F_OK) == 0;
}

std::string timestamp_suffix(void)
{
    struct timespec ts{};
    struct tm tm{};
    char out[40];
    int ms = 0;
    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tm);
    ms = (int)(ts.tv_nsec / 1000000L);
    std::snprintf(out, sizeof(out), "%04d%02d%02dT%02d%02d%02d.%03d", tm.tm_year + 1900,
                  tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ms);
    return std::string(out);
}
} // namespace

std::string tdx_shot_versioned_path(const std::string &path)
{
    std::string dir;
    std::string base;
    std::string stem;
    std::string ext;
    std::string ts;
    std::string candidate;
    const auto slash = path.find_last_of('/');
    int n = 0;

    if (path.empty())
    {
        base = "tdx.bmp";
        dir = "/tmp/";
    }
    else if (slash == std::string::npos)
    {
        base = path;
    }
    else
    {
        dir = path.substr(0, slash + 1);
        base = path.substr(slash + 1);
    }

    {
        const auto dot = base.find_last_of('.');
        if ((dot != std::string::npos) && (dot > 0))
        {
            stem = base.substr(0, dot);
            ext = base.substr(dot);
            if ((ext != ".bmp") && (ext != ".BMP") && (ext != ".png") && (ext != ".PNG") &&
                (ext != ".ppm") && (ext != ".PPM"))
            {
                stem = base;
                ext = ".bmp";
            }
        }
        else
        {
            stem = base.empty() ? "tdx" : base;
            ext = ".bmp";
        }
    }
    if (stem.empty())
    {
        stem = "tdx";
    }
    ts = timestamp_suffix();
    candidate = dir + stem + "-" + ts + ext;
    if (!file_exists(candidate))
    {
        return candidate;
    }
    for (n = 2; n < 10000; n++)
    {
        candidate = dir + stem + "-" + ts + "-" + std::to_string(n) + ext;
        if (!file_exists(candidate))
        {
            return candidate;
        }
    }
    return dir + stem + "-" + ts + "-x" + ext;
}
