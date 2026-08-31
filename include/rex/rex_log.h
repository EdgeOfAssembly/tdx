/**
 * @file rex_log.h
 * @brief stderr + optional file logging for librex / TDX.
 */
#ifndef REX_LOG_H
#define REX_LOG_H

#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum rex_log_level
{
    REX_LOG_ERROR = 0,
    REX_LOG_INFO = 1,
    REX_LOG_DEBUG = 2
} rex_log_level;

void rex_log_set_file(FILE *fp);
void rex_log_set_level(rex_log_level level);
void rex_log_close_file(void);
void rex_logf(rex_log_level level, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* REX_LOG_H */
