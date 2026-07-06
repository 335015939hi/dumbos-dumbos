
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

  LOG_DEBUG("client() called with %d args", argc);

  socket_addr = malloc(sizeof(*socket_addr));
  if (socket_addr == NULL) {
    LOG_ERRNO("cannot allocate memory", errno);
    return errno;
  }

  socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    LOG_ERRNO("failed to socket", errno);
    free(socket_addr);
    return errno;
  }
  LOG_DEBUG("socket_fd=%d", socket_fd);

  memset(socket_addr, 0, sizeof(*socket_addr));
  socket_addr->sun_family = AF_UNIX;
  if (strlen(socket_path) >= SOCKET_PATH_MAX) {
    free(socket_addr);
    errno = ENAMETOOLONG;
    return ENAMETOOLONG;
  }
  strncpy(socket_addr->sun_path, socket_path, SOCKET_PATH_MAX);

  LOG_VERBOSE("socket_path=%s", socket_path);
  LOG_DEBUG("SOCKET_PATH_MAX=%zu", SOCKET_PATH_MAX);

  ret =
      connect(socket_fd, (struct sockaddr *)socket_addr, sizeof(*socket_addr));
  if (ret < 0) {
    free(socket_addr);
    close(socket_fd);
    LOG_ERRNO("failed to connect to socket", errno);
    return errno;
  }

  free(socket_addr);

  LOG_DEBUG("writing version string %s", VERSION_STRING);
  ret = write_string(socket_fd, VERSION_STRING);
  if (ret == 0) {
    LOG_DEBUG("writing major version %d", VERSION_MAJOR);
    ret = write_ushort(socket_fd, VERSION_MAJOR);
    if (ret == 0) {
      LOG_DEBUG("writing minor version %d", VERSION_MINOR);
      ret = write_ushort(socket_fd, VERSION_MINOR);
      if (ret == 0) {
        LOG_DEBUG("writing patch version %d", VERSION_PATCH);
        ret = write_ushort(socket_fd, VERSION_PATCH);
      }
    }
  }
  if (ret < 0) {
    LOG_ERRNO("write handshake failed", errno);
    close(socket_fd);
    return errno;
  }

  ret = read_ushort(socket_fd); // should be 0 on handshake success
  LOG_VERBOSE("daemon feedback=%d", ret);
  if (ret < 0) {
    ret = errno;
    LOG_ERRNO("read handshake result failed", ret);
    close(socket_fd);
    return ret;
  } else if (ret != 0) {
    LOG_ERRNO("handshake failed", ret);
    close(socket_fd);
    return ret;
  }

  LOG_VERBOSE("sending argc %d", argc);
  ret = write_ushort(socket_fd, argc);
  if (ret < 0) {
    ret = errno;
    LOG_ERRNO("failed to write argc", ret);
    close(socket_fd);
    return ret;
  }

  for (int i = 0; i < argc; i++) {
    LOG_VERBOSE("sending argv[%d] %s", i, argv[i]);
    ret = write_string(socket_fd, argv[i]);
    if (ret < 0) {
      ret = errno;
      close(socket_fd);
      LOG_ERRNO("failed to write argv[]", ret);
      return ret;
    }
  }

  LOG_DEBUG("finished sending args");

  int d_ret;
  char *d_msg;

  LOG_DEBUG("reading daemon return code");
  ret = read_ushort(socket_fd);
  if (ret < 0) {
    ret = errno;
    close(socket_fd);
    LOG_ERRNO("failed to read daemon return code", ret);
    return ret;
  }
  d_ret = ret;
  LOG_VERBOSE("daemon returned %d", ret);

  LOG_DEBUG("reading daemon message");
  d_msg = malloc_read_string(socket_fd);
  if (d_msg == NULL) {
    ret = errno;
    LOG_ERRNO("faild to read daemon return message", ret);
    return ret;
  }
  LOG_VERBOSE("daemon sent us %s", d_msg);

  printf("%s\n", d_msg);

  free(d_msg);
  LOG_DEBUG("client() returning %d", d_ret);
  return d_ret;
};
