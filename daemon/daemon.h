#ifndef _DAEMON_DAEMON_H
#define _DAEMON_DAEMON_H

#include <sys/types.h>

// main daemon program
int start_daemon(const char *const socket_path, const char *const server,
                 const char *const port, mode_t sock_mode, uid_t sock_own,
                 gid_t sock_grp, const char *const con);

#endif
