/**
 * @file test_log.cpp
 * @brief format_human_size and --log open/write.
 */
#include "iron86/log.h"

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <unistd.h>

static int g_fail = 0;

static void expect(bool ok, const char *msg)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL %s\n", msg);
        g_fail = 1;
    }
}

static void expect_streq(const char *got, const char *want, const char *msg)
{
    if (std::strcmp(got, want) != 0)
    {
        std::fprintf(stderr, "FAIL %s got='%s' want='%s'\n", msg, got, want);
        g_fail = 1;
    }
}

int main()
{
    char s[32];
    iron86::format_human_size(0, s, sizeof(s));
    expect_streq(s, "0B", "0B");
    iron86::format_human_size(10, s, sizeof(s));
    expect_streq(s, "10B", "10B");
    iron86::format_human_size(1024, s, sizeof(s));
    expect_streq(s, "1KB", "1KB");
    iron86::format_human_size(368640, s, sizeof(s));
    expect_streq(s, "360KB", "360KB");
    iron86::format_human_size(1048576, s, sizeof(s));
    expect_streq(s, "1.00MB", "1MB exact via 2dp <10");
    iron86::format_human_size(10653696, s, sizeof(s));
    expect_streq(s, "10MB", "type-1 10MB");
    iron86::format_human_size(1474560, s, sizeof(s));
    expect_streq(s, "1.41MB", "1.44M floppy");
    iron86::format_human_size(1073741824ull, s, sizeof(s));
    expect_streq(s, "1.00GB", "1GB");
    iron86::format_human_size(1099511627776ull, s, sizeof(s));
    expect_streq(s, "1.00TB", "1TB");
    iron86::format_human_size(2048, s, sizeof(s));
    expect_streq(s, "2KB", "2KB");

    expect(!iron86::log_active(), "closed");
    expect(!iron86::log_open(nullptr), "null path");
    expect(!iron86::log_open(""), "empty path");

    char path[] = "/tmp/iron86-test-log-XXXXXX";
    const int tfd = ::mkstemp(path);
    expect(tfd >= 0, "mkstemp");
    if (tfd >= 0)
    {
        (void)::close(tfd);
        (void)::unlink(path);
        expect(iron86::log_open(path), "open");
        expect(iron86::log_active(), "active");
        iron86::log_event("HDC0  test\n");
        iron86::log_close();
        expect(!iron86::log_active(), "closed after");
        std::FILE *fp = std::fopen(path, "r");
        expect(fp != nullptr, "read log");
        if (fp != nullptr)
        {
            char line[64];
            expect(std::fgets(line, sizeof(line), fp) != nullptr, "fgets");
            expect(std::strstr(line, "HDC0") != nullptr, "HDC0 in file");
            std::fclose(fp);
        }
        (void)::unlink(path);
    }

    expect(iron86::lock_region(nullptr, 0, "empty"), "empty lock");
    {
        char page[4096];
        std::memset(page, 0, sizeof(page));
        expect(iron86::lock_region(page, sizeof(page), "test"), "mlock 4K");
    }

    {
        char tpath[] = "/tmp/iron86-test-trace-XXXXXX";
        const int trace_fd = ::mkstemp(tpath);
        expect(trace_fd >= 0, "mkstemp trace");
        if (trace_fd >= 0)
        {
            (void)::close(trace_fd);
            (void)::unlink(tpath);
            expect(iron86::log_open(tpath), "open trace");
            iron86::log_trace("OUT 322,00\n");
            iron86::log_close();
            std::FILE *fp = std::fopen(tpath, "r");
            expect(fp != nullptr, "read trace");
            if (fp != nullptr)
            {
                char line[64];
                expect(std::fgets(line, sizeof(line), fp) != nullptr, "trace fgets");
                expect(std::strstr(line, "OUT 322") != nullptr, "OUT 322 in file");
                std::fclose(fp);
            }
            (void)::unlink(tpath);
        }
    }

    if (g_fail == 0)
    {
        std::fprintf(stderr, "iron86 log tests ok\n");
    }
    return g_fail;
}
