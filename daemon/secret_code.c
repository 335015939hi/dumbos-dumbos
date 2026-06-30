
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../common.h"
#include "command.h"

#define MSG "secret_code: expected exactly ONE code\n"

char *secret_code(int argc, char **argv, int *ret_val, const char *const host,
                  const char *const port) {
  if (argc != 1) {
    char *ret_str = malloc(sizeof(MSG));
    strcpy(ret_str, MSG);
    print_error(ret_str);
    *ret_val = EINVAL;
    return ret_str;
  }

  return NULL;
}
