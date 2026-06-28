
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "../common.h"
#include "client.h"

#define SOCKET_PATH_MAX (sizeof(((struct sockaddr_un *)0)->sun_path))

int client(int arg, char **argv, const char *const socket_path) {
  int socket_fd;
  struct sockaddr_un *socket_addr;
  int ret;

  socket_addr = malloc(sizeof(*socket_addr));
  if (socket_addr == NULL) {
    print_error("cannot allocate memory\n");
    return ENOMEM;
  }

  socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    print_error("failed to socket:");
    print_error(strerror(errno));
    return errno;
  }

  memset(socket_addr, 0, sizeof(*socket_addr));
  socket_addr->sun_family = AF_UNIX;
  strncpy(socket_addr->sun_path, socket_path, SOCKET_PATH_MAX);

  ret =
      connect(socket_fd, (struct sockaddr *)socket_addr, sizeof(*socket_addr));
  if (ret < 0) {
    free(socket_addr);
    print_error("failed to connect to socket:");
    print_error(strerror(errno));
    print_error("\n");
    return errno;
  }

  write_ushort(socket_fd, 509);
  printf("%ld", read_ushort(socket_fd));

  free(socket_addr);

  return 0;
};
