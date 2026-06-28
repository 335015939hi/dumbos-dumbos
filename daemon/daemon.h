#ifndef _DAEMON_DAEMON_H
#define _DAEMON_DAEMON_H

// main daemon program
int start_daemon(const char *const socket_path, const char *const server,
                 const int port);

#endif
