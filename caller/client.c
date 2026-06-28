
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

int client(int argc, char **argv, const char *const socket_path) {
  int socket_fd;
  struct sockaddr_un *socket_addr;
  int ret;

  socket_addr = malloc(sizeof(*socket_addr));
  if (socket_addr == NULL) {
    print_errno("cannot allocate memory", errno);
    return errno;
  }

  socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    print_errno("failed to socket", errno);
    return errno;
  }

  memset(socket_addr, 0, sizeof(*socket_addr));
  socket_addr->sun_family = AF_UNIX;
  strncpy(socket_addr->sun_path, socket_path, SOCKET_PATH_MAX);

  ret =
      connect(socket_fd, (struct sockaddr *)socket_addr, sizeof(*socket_addr));
  if (ret < 0) {
    free(socket_addr);
    print_errno("failed to connect to socket", errno);
    return errno;
  }

  free(socket_addr);

  ret = write_string(socket_fd, VERSION_STRING);
  if (ret == 0) {
    ret = write_ushort(socket_fd, VERSION_MAJOR);
    if (ret == 0) {
      ret = write_ushort(socket_fd, VERSION_MINOR);
      if (ret == 0) {
        ret = write_ushort(socket_fd, VERSION_PATCH);
      }
    }
  }
  if (ret < 0) {
    print_errno("write handshake failed", errno);
  }

  ret = read_ushort(socket_fd); // should be 0 on handshake success
  if (ret < 0) {
    print_errno("read handshake result failed", errno);
  } else if (ret != 0) {
    print_errno("handshake failed", errno);
  }

  ret = write_ushort(socket_fd, argc);
  if (ret < 0) {
    print_errno("failed to write argc", errno);
  }

  return 0;
};
