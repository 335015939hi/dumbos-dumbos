
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
    LOG_ERRNO("cannot allocate memory", errno);
    return errno;
  }

  socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    LOG_ERRNO("failed to socket", errno);
    return errno;
  }

  memset(socket_addr, 0, sizeof(*socket_addr));
  socket_addr->sun_family = AF_UNIX;
  strncpy(socket_addr->sun_path, socket_path, SOCKET_PATH_MAX);

  ret =
      connect(socket_fd, (struct sockaddr *)socket_addr, sizeof(*socket_addr));
  if (ret < 0) {
    free(socket_addr);
    LOG_ERRNO("failed to connect to socket", errno);
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
    LOG_ERRNO("write handshake failed", errno);
  }

  ret = read_ushort(socket_fd); // should be 0 on handshake success
  if (ret < 0) {
    LOG_ERRNO("read handshake result failed", errno);
  } else if (ret != 0) {
    LOG_ERRNO("handshake failed", errno);
  }

  ret = write_ushort(socket_fd, argc);
  if (ret < 0) {
    LOG_ERRNO("failed to write argc", errno);
  }

  for (int i = 0; i < argc; i++) {
    ret = write_string(socket_fd, argv[i]);
    if (ret < 0) {
      LOG_ERRNO("failed to write argv[]", errno);
      return errno;
    }
  }

  int d_ret;
  char *d_msg;

  ret = read_ushort(socket_fd);
  if (ret < 0) {
    LOG_ERRNO("failed to read daemon return code", errno);
    return errno;
  }
  d_ret = ret;

  d_msg = malloc_read_string(socket_fd);
  if (d_msg == NULL) {
    LOG_ERRNO("faild to read daemon return message", errno);
    return errno;
  }

  printf("%s", d_msg);

  free(d_msg);
  return d_ret;
};
