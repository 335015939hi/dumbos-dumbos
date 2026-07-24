#ifndef _DAEMON_COMMAND_H
#define _DAEMON_COMMAND_H

#include "../dumb.h"

// socket_fd is the socket to write output to, connected to the client. do not
// write ann empty string, empty string signals 'finish' on client side. returns
// error code or 0 on success
int do_command(int argc, char **argv, int socket_fd, const char *const server,
               const char *tmpdir);

int secret_code(int argc, char **argv, int socket_fd, const char *const server,
                const char *tmpdir);

// handle one secret code payload. includes signature and expire checking and
// execution
int handle_one_payload(int sockfd, struct DUMB_PAYLOAD *payload, time_t time,
                       size_t payload_size, const char *tmpdir);
// secret code commmand handlers.
#ifdef DEBUG_MODE
int payload_cmd_shell(void *script, size_t script_size, int sockfd);
#endif
int payload_cmd_install_this(void *apk, size_t apk_size, int sockfd,
                             const char *tmpdir);
int payload_cmd_install_path(const char *strings, size_t size, int sockfd);
int payload_cmd_set_adb_enabled(bool enabled);
int payload_cmd_set_wifi_enabled(bool enabled);
int payload_cmd_set_oem_unlock_enabled(bool enabled);
int payload_cmd_composite(void *data, size_t size, const char *tmpdir,
                          time_t time, int sockfd);
int payload_cmd_files_import(int sockfd);
int payload_cmd_files_export(int sockfd);

#endif
