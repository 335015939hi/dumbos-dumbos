#ifndef _DAEMON_HANDLER_H
#define _DAEMON_HANDLER_H

int handler(const int client_fd, const char *const server,
            const char *const port);

#endif
