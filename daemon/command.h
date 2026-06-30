#ifndef _DAEMON_COMMAND_H
#define _DAEMON_COMMAND_H

// returns a string. the string is either NULL (no message) or malloc()'ed
// (print then free it)'
char *do_command(int argc, char **argv, int *ret_val, const char *const server,
                 const char *const port, const char *tmpdir);

char *secret_code(int argc, char **argv, int *ret_val, const char *const server,
                  const char *const port, const char *tmpdir);

#endif
