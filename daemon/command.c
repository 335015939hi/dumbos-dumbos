
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "../common.h"
#include "command.h"

static const char *const commands_list[] = {
    "ok",
    "notok",
    "code",
};
// the enum and commands_list must match!
enum {
  CMD_OK = 0,
  CMD_NOTOK,
  CMD_CODE,
  // this must be the last one!
  CMDLIST_SIZE
} commands;

char *do_command(int argc, char **argv, int *ret_val) {
  printf("recieved command %s\n", argv[0]);
  char *ret_str = NULL;

  int command = -1;
  for (int i = 0; i < CMDLIST_SIZE; i++) {
    if (0 == strcmp(commands_list[i], argv[0])) {
      command = i;
      break;
    }
  }
  if (command < 0) {
    print_errno("Bad command", EINVAL);
    *ret_val = EINVAL;
    return NULL;
  }

  int ret;

  switch (command) {
  case CMD_OK:
    ret = 0;
    break;
  case CMD_NOTOK:
    ret = 1;
    break;
  default:
    fprintf(stderr, "Bad command code:%d\n", command);
    ret = EINVAL;
    break;
  }

  *ret_val = ret;
  return ret_str;
}
