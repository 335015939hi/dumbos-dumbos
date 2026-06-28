
#include <stdio.h>

#include "../common.h"
#include "handler.h"

int handler(const int client_fd) {
  int a;
  a = read_ushort(client_fd);
  printf("daemon:%d\n", a);
  write_ushort(client_fd, 145);
  return 0;
};
