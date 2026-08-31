/**
 * @file tdx_agent_sock.h
 * @brief Keep-alive UNIX JSON-line server (Xmux ctl-style, no reconnect).
 */
#ifndef TDX_AGENT_SOCK_H
#define TDX_AGENT_SOCK_H

#include <string>

struct tdx_agent_sock;

/** One request line in, one reply string out (include trailing newline). */
typedef std::string (*tdx_agent_fn)(void *user, const std::string &line);

tdx_agent_sock *tdx_agent_listen(const char *path);
void tdx_agent_close(tdx_agent_sock *sk);
int tdx_agent_poll(tdx_agent_sock *sk, tdx_agent_fn fn, void *user);

#endif /* TDX_AGENT_SOCK_H */
