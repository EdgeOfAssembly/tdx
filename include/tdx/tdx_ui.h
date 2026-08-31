/**
 * @file tdx_ui.h
 * @brief SDL2 Turbo Debugger-like CPU view + CGA user screen.
 */
#ifndef TDX_UI_H
#define TDX_UI_H

#include "rex/rex.h"
#include "rex/rex_sock.h"
#include "tdx/tdx_cli.h"

int tdx_ui_run(rex_session *session, rex_sock *sock, const tdx_cli *cli);

/** Write CPU window to BMP @p path; 0 on success. */
int tdx_ui_shot_cpu(void *user, const char *path);
int tdx_ui_shot_game(void *user, const char *path);

#endif /* TDX_UI_H */
