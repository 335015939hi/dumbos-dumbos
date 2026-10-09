
#define _GNU_SOURCE

#include <errno.h>
#include <selinux/selinux.h>
#include <signal.h>
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
#include "util.h"

// max length of struct sockaddr_un.sun_path, with NULL, in bytes
#define MAX_SOCK_PATH (sizeof(((struct sockaddr_un *)0)->sun_path))

static char *default_server = NULL;
static char *dumb_upload_server = NULL;
static char *dumb_code_server = NULL;
static char *dumb_tmpdir = NULL;

static char *alloc_dumb_server_append(const char *s) {

  if (default_server == NULL) {
    errno = EINVAL;
    return NULL;
  }
  if (strlen(default_server) == 0) {
    errno = EINVAL;
    return 0;
  }
  const char *slash = "";
  if (default_server[strlen(default_server) - 1] != '/') {
    slash = "/";
  }
  char *result;
  int err = asprintf(&result, "%s%s%s", default_server, slash, s);
  if (err < 0) {
    return NULL;
  }
  return result;
}
const char *get_dumb_server_code(void) { return dumb_code_server; }
const char *get_dumb_server_upload(void) { return dumb_upload_server; }

void set_default_server(const char *server) {
  if (default_server)
    free(default_server);
  if (dumb_upload_server)
    free(dumb_upload_server);
  if (dumb_code_server)
    free(dumb_code_server);

  default_server = strdup(server);
  dumb_code_server = alloc_dumb_server_append(DUMB_URL_CODE_PATH);
  dumb_upload_server = alloc_dumb_server_append(DUMB_URL_UPLOAD_PATH);
  if (default_server == NULL || dumb_code_server == NULL ||
      dumb_upload_server == NULL) {
    // panic!!!!
    LOG_FATAL(
        "default_server=%p dumb_code_server=%p dumb_upload_server=%p errno=%d",
        default_server, dumb_code_server, dumb_upload_server, errno);
    abort();
  }
}

void set_tmpdir(const char *path) {
  if (dumb_tmpdir)
    free(dumb_tmpdir);
  int len = strlen(path);
  if (path[len - 1] == '/') {
    dumb_tmpdir = strdup(path);
  } else {
    dumb_tmpdir = malloc(len + 2);
    if (dumb_tmpdir) {
      memcpy(dumb_tmpdir, path, len);
      dumb_tmpdir[len] = '/';
      dumb_tmpdir[len + 1] = '\0';
    }
  }
  if (dumb_tmpdir == NULL) {
    LOG_FATAL("dumb_tmpdir=%p errno=%d", dumb_tmpdir, errno);
    abort();
  }
}
const char *get_tmpdir(void) { return dumb_tmpdir; }

int start_daemon(const struct daemon_opts *const opt) {

  signal(SIGCHLD, SIG_IGN);

  int socket_fd;
  int err;
  int len;
  struct sockaddr_un *sock_addr = malloc(sizeof(struct sockaddr_un));
  if (sock_addr == NULL) {
    LOG_ERRNO("failed to malloc", errno);
    return errno;
  }

  // who cares if this fails?
  maybe_set_lockscreen_text();

  socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    LOG_ERRNO("failed to create socket", errno);
    free(sock_addr);
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
    close(socket_fd);
    free(sock_addr);
    return errno;
  }
  free(sock_addr);

  if (chown(opt->path, opt->uid, opt->gid) < 0) {
    LOG_ERRNO("chown failed", errno);
    close(socket_fd);
    unlink(opt->path);
    return errno;
  }
  if (chmod(opt->path, opt->mode) < 0) {
    LOG_ERRNO("chmod failed", errno);
    close(socket_fd);
    unlink(opt->path);
    return errno;
  }

  LOG_DEBUG("setting selinux context '%s'", opt->con);
  if (setfilecon(opt->path, opt->con) < 0) {
    LOG_WARN_ERRNO("setting selinux context failed", errno);
  }

  if (listen(socket_fd, 16) < 0) {
    LOG_ERRNO("listen failed", errno);
    close(socket_fd);
    unlink(opt->path);
    return errno;
  }

  LOG_DEBUG("setting tmpdir");
  set_tmpdir(opt->tmpdir);

  LOG_VERBOSE("creating tmpdir '%s'", get_tmpdir());
  err = mkdir_p(get_tmpdir());
  if (err != 0) {
    LOG_WARN_ERRNO("creating tmpdir failed", errno);
  }

  set_default_server(opt->server);

  for (;;) {
    int client;

    LOG_VERBOSE("waiting for  client...");
    client = accept(socket_fd, NULL, NULL);
    if (client < 0) {
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
      int ret = handler(client);
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
