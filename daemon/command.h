#ifndef _DAEMON_COMMAND_H
#define _DAEMON_COMMAND_H

// socket_fd is the socket to write output to, connected to the client. do not
// write ann empty string, empty string signals 'finish' on client side. returns
// error code or 0 on success
int do_command(int argc, char **argv, int socket_fd, const char *const server,
               const char *tmpdir);

int secret_code(int argc, char **argv, int socket_fd, const char *const server,
                const char *tmpdir);

#endif
