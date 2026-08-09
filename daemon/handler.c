
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common.h"
#include "command.h"
#include "handler.h"

// max number of arguments we will accept from the client
#define MAX_ARGS 256

int handler(const int client_fd, const char *const server, const char *tmpdir) {
  char *client_v_str;
  unsigned short client_v_major;
  unsigned short client_v_minor;
  unsigned short client_v_patch;
  int err;

  signal(SIGCHLD, SIG_DFL);

  client_v_str = malloc_read_string(client_fd);
  if (client_v_str == NULL) {
    LOG_ERRNO("reading client version string fail", errno);
    return errno;
  }

  LOG("client version string is %s", client_v_str);
  free(client_v_str);

  err = read_ushort(client_fd);
  if (err >= 0) {
    client_v_major = err;
    err = read_ushort(client_fd);
    if (err >= 0) {
      client_v_minor = err;
      err = read_ushort(client_fd);
      if (err >= 0) {
        client_v_patch = err;
      }
    }
  }
  if (err < 0) {
    LOG_ERRNO("reading client version code fail", errno);
    return errno;
  }

  LOG("client version code is %hu.%hu.%hu", client_v_major, client_v_minor,
      client_v_patch);
  if (client_v_major != (unsigned short)VERSION_MAJOR ||
      client_v_minor != (unsigned short)VERSION_MINOR ||
      client_v_patch != (unsigned short)VERSION_PATCH) {
    LOG_ERR("client has incompatible version. stopping");
    write_ushort(client_fd, EPROTO);
    return EPROTO;
  }

  // success! tell it to the client
  err = write_ushort(client_fd, 0);
  if (err != 0) {
    LOG_ERRNO("failed to write_ushort() for handshake success", errno);
  }

  int argc;
  char **argv;
  char *ret_msg = NULL;
  int ret = EINVAL;
  bool haserror = false;

  ret = read_ushort(client_fd);
  if (ret < 0) {
    LOG_ERRNO("getting argc failed", errno);
    return EPROTO;
  }
  argc = ret;
  if (argc == 0) {
    LOG("no command. exiting");
    write_string(client_fd, "No command");
    write_string(client_fd, "");
    write_ushort(client_fd, 0);
    return 0;
  }
  LOG("client has %d arguments", argc);
  if (argc > MAX_ARGS || argc < 0) {
    LOG_ERR("too many arguments, max %d got %d", MAX_ARGS, argc);
    write_string(client_fd, "too many arguments");
    write_ushort(client_fd, E2BIG);
    return E2BIG;
  }

  argv = calloc(argc, sizeof(char *));
  if (argv == NULL) {
    err = errno;
    LOG_ERRNO("alloc argv fail", err);
    write_string(client_fd, "internal error");
    write_string(client_fd, "");
    write_ushort(client_fd, err);
    return err;
  }

  for (int i = 0; i < argc; i++) {
    char *arg;
    arg = malloc_read_string(client_fd);
    if (arg == NULL) {
      ret = errno;
      LOG_ERRNO("alloc argv[] fail", err);
      write_string(client_fd, "internal error");
      write_string(client_fd, "");
      write_ushort(client_fd, err);
      haserror = true;
      break;
    }
    argv[i] = arg;
  }

  if (!haserror) {
    ret = do_command(argc, argv, client_fd, server, tmpdir);
    LOG_DEBUG("do_command exited with %d", ret);
  }

  LOG_DEBUG("writing empty string");
  err = write_string(client_fd, "");
  if (err != 0) {
    LOG_ERRNO("write_string(\"\") failed", errno);
  }

  for (int i = 0; i < argc; i++) {
    if (argv[i])
      free(argv[i]);
  }
  free(argv);

  err = write_ushort(client_fd, (unsigned short)ret);
  if (err != 0) {
    LOG_ERRNO("writing return code to client failed", errno);
  }

  return ret;
};
