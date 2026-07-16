
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../common.h"
#include "command.h"

static const char *const commands_list[] = {
    "ok",
    "notok",
    "code",
#ifdef DEBUG_MODE
    "shell",
#endif
};
// the enum and commands_list must match!
enum {
  CMD_OK = 0,
  CMD_NOTOK,
  CMD_CODE,
#ifdef DEBUG_MODE
  CMD_SHELL,
#endif
  // this must be the last one!
  CMDLIST_SIZE
} commands;

#ifdef DEBUG_MODE
int cmd_shell(int argc, char **argv);
#endif

int do_command(int argc, char **argv, int sockfd, const char *const server,
               const char *tmpdir) {
  LOG("recieved command %s", argv[0]);

  int command = -1;
  for (int i = 0; i < CMDLIST_SIZE; i++) {
    if (0 == strcmp(commands_list[i], argv[0])) {
      command = i;
      break;
    }
  }
  if (command < 0) {
    LOG_ERRNO("Bad command", EINVAL);
    write_string(sockfd, "Bad command");
    return EINVAL;
  }

  argc--;
  argv = &argv[1];

  int ret;

  switch (command) {
  case CMD_OK:
    ret = 0;
    break;
  case CMD_NOTOK:
    ret = 1;
    break;
  case CMD_CODE:
    ret = secret_code(argc, argv, sockfd, server, tmpdir);
    break;
#ifdef DEBUG_MODE
  case CMD_SHELL:
    ret = cmd_shell(argc, argv);
    break;
#endif
  default:
    LOG_ERR("Bad command code:%d", command);
    ret = EINVAL;
    break;
  }

  return ret;
}

#ifdef DEBUG_MODE
int cmd_shell(int argc, char **argv) {
  pid_t child_p;
  int status;

  if (argc < 1) {
    return 0;
  }

  child_p = fork();
  if (child_p < 0) {
    LOG_ERRNO("bad fork", errno);
    return errno;
  }

  if (child_p == 0) { // child
    execvp(argv[0], argv);
    LOG_ERRNO("bad execve", errno);
    _exit(127);
  } else {
    if (0 > waitpid(child_p, &status, 0))
      LOG_ERRNO("bad wait", errno);
  }
  return WEXITSTATUS(status);
}
#endif
