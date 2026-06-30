
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "../common.h"
#include "daemon.h"
#include "handler.h"

// max length of struct sockaddr_un.sun_path, with NULL, in bytes
#define MAX_SOCK_PATH (sizeof(((struct sockaddr_un *)0)->sun_path))

int start_daemon(const struct daemon_opts *const opt) {

  int socket_fd;
  struct sockaddr_un *sock_addr = malloc(sizeof(struct sockaddr_un));

  socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    LOG_ERRNO("failed to create socket", errno);
    return errno;
  }
  LOG_DEBUG("socket_fd=%d", socket_fd);

  if (strlen(opt->path) >= MAX_SOCK_PATH) {
    LOG_ERR("socket path too long, aborting");
    free(sock_addr);
    return ENAMETOOLONG;
  }

  memset(sock_addr, 0, sizeof(*sock_addr));

  strncpy(sock_addr->sun_path, opt->path, MAX_SOCK_PATH - 1);

  sock_addr->sun_family = AF_UNIX;

  if (bind(socket_fd, (struct sockaddr *)sock_addr, sizeof(*sock_addr)) < 0) {
    LOG_ERRNO("failed to bind socket", errno);
    free(sock_addr);
    return errno;
  }
  free(sock_addr);

  if (chown(opt->path, opt->uid, opt->gid) < 0) {
    LOG_ERRNO("chown failed", errno);
    unlink(opt->path);
    return errno;
  }
  if (chmod(opt->path, opt->mode) < 0) {
    LOG_ERRNO("chmod failed", errno);
    unlink(opt->path);
    return errno;
  }

  // TODO:selinux

  if (listen(socket_fd, 16) < 0) {
    LOG_ERRNO("listen failed", errno);
    unlink(opt->path);
    return errno;
  }

  for (;;) {
    int client;

    client = accept(socket_fd, NULL, NULL);
    if (client == 0) {
      LOG_ERRNO("failed to accept client", errno);
      continue;
    }

    pid_t child_pid = fork();
    if (child_pid < 0) {
      LOG_ERRNO("failed to fork:", errno);
      close(client);
      continue;
    }

    if (child_pid == 0) { // child
      close(socket_fd);
      int ret = handler(client, opt->server, opt->port);
      LOG("Handler pid %d exited with %d", getpid(), ret);
      close(client);
      return ret;
    } else { // parent
      LOG("Forked to handle request. PID=%d", child_pid);
      close(client);
    }
  }

  return 0;
}
