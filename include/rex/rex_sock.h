/**
 * @file rex_sock.h
 * @brief Bidirectional UNIX-domain JSON/text control socket for LLM agents.
 */
#ifndef REX_SOCK_H
#define REX_SOCK_H

#include "rex.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rex_sock rex_sock;

/**
 * @brief Listen on a UNIX socket path (unlinks stale path first).
 *
 * @param[in] path  e.g. `/tmp/tdx.sock`.
 * @return Handle or NULL.
 */
REX_API rex_sock *rex_sock_listen(const char *path);

REX_API void rex_sock_close(rex_sock *sk);
REX_API const char *rex_sock_path(const rex_sock *sk);

/**
 * @brief Accept + read one line + dispatch. Non-blocking.
 *
 * @return Number of commands handled this call (0 if idle).
 */
REX_API int rex_sock_poll(rex_sock *sk, rex_session *s);

/**
 * @brief Last screenshot paths written by a `shot` command (may be empty).
 */
REX_API const char *rex_sock_last_cpu_shot(const rex_sock *sk);
REX_API const char *rex_sock_last_game_shot(const rex_sock *sk);

/**
 * @brief Optional UI screenshot callbacks (SDL). NULL = skip that window.
 *
 * Signature: write a BMP/PPM to @p path, return 0 on success.
 */
typedef int (*rex_shot_fn)(void *user, const char *path);

REX_API void rex_sock_set_shotters(rex_sock *sk, rex_shot_fn cpu, rex_shot_fn game, void *user);
REX_API bool rex_sock_quit_requested(const rex_sock *sk);

#ifdef __cplusplus
}
#endif

#endif /* REX_SOCK_H */
