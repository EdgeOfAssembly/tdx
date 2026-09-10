/**
 * @file log.h
 * @brief Unbuffered write(2) diagnostics and optional --log file.
 *
 * Default path (no --log) never touches the log fd. Event lines always go
 * to stderr; with --log they are duplicated to the file. No FILE* log I/O.
 */
#ifndef IRON86_LOG_H
#define IRON86_LOG_H

#include <cstddef>
#include <cstdint>

namespace iron86
{

/**
 * @brief setvbuf(_IONBF) on stdin, stdout, and stderr.
 *
 * Our diagnostics use write(2). This only covers leftover libc/ASAN FILE*.
 */
void log_init_stdio();

/**
 * @brief Open @p path for append (O_CLOEXEC).
 * @param[in] path Host file (must not be NULL or empty).
 * @return false if open fails.
 */
bool log_open(const char *path);

/** @brief fsync + close. Safe if no log is open. */
void log_close();

/** @brief True after a successful @ref log_open. */
bool log_active();

/**
 * @brief write(2) the whole buffer.
 * @param[in] fd Host descriptor.
 * @param[in] s  Bytes; may be NULL only when @p n is 0.
 * @param[in] n  Length.
 */
void fd_write(int fd, const char *s, size_t n);

/** @brief write(2) a NUL-terminated string. */
void fd_puts(int fd, const char *s);

/** @brief snprintf into a stack buffer, then one write(2). */
void fd_printf(int fd, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

/**
 * @brief Event line: stderr always, log file if @ref log_open succeeded.
 */
void log_event(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief Hot-path trace: log file only (never stderr). No-op if no --log.
 */
void log_trace(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief mlock @p n bytes at @p p. Logs ok/fail via @ref log_event.
 * @param[in] p    Region (NULL or n==0 is a no-op success).
 * @param[in] n    Length in bytes.
 * @param[in] name Tag for the log line (e.g. "ram", "hdc").
 * @return true if mlock succeeded or there was nothing to lock.
 */
bool lock_region(void *p, size_t n, const char *name);

/**
 * @brief 1024-based size into @p out (B, KB, MB, GB, TB).
 *
 * Integer when the value divides the unit, or when MB/GB/TB ≥ 10
 * (Type-1 10653696 → 10MB). Below 10MB, two decimal places (1.44M floppy).
 *
 * @param[in]  n   Byte count.
 * @param[out] out Destination (NUL-terminated on success).
 * @param[in]  cap Size of @p out in bytes.
 */
void format_human_size(uint64_t n, char *out, size_t cap);

} // namespace iron86

#endif
