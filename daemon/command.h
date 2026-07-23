#ifndef _DAEMON_COMMAND_H
#define _DAEMON_COMMAND_H

// socket_fd is the socket to write output to, connected to the client. do not
// write ann empty string, empty string signals 'finish' on client side. returns
// error code or 0 on success
int do_command(int argc, char **argv, int socket_fd, const char *const server,
               const char *tmpdir);

int secret_code(int argc, char **argv, int socket_fd, const char *const server,
                const char *tmpdir);

// secret code commmand handlers.
#ifdef DEBUG_MODE
int payload_cmd_shell(void *script, size_t script_size, int sockfd);
#endif
int payload_cmd_install_this(void *apk, size_t apk_size, int sockfd,
                             const char *tmpdir);

#endif
