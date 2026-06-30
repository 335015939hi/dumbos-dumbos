
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_USE_PID

#include "../common.h"
#include "command.h"
#include "handler.h"

int handler(const int client_fd, const char *const server,
            const char *const port) {
  char *client_v_str;
  unsigned short client_v_major;
  unsigned short client_v_minor;
  unsigned short client_v_patch;
  signed long ret;

  client_v_str = malloc_read_string(client_fd);
  if (client_v_str == NULL) {
    LOG_ERRNO("reading client version string fail", errno);
    return errno;
  }

  LOG("client version string is %s\n", client_v_str);
  free(client_v_str);

  ret = read_ushort(client_fd);
  if (ret >= 0) {
    client_v_major = ret;
    ret = read_ushort(client_fd);
    if (ret >= 0) {
      client_v_minor = ret;
      ret = read_ushort(client_fd);
      if (ret >= 0) {
        client_v_patch = ret;
      }
    }
  }
  if (ret < 0) {
    LOG_ERRNO("reading client version code fail", errno);
    return errno;
  }

  LOG("client version code is %hu.%hu.%hu\n", client_v_major, client_v_minor,
      client_v_patch);
  if (client_v_major != (unsigned short)VERSION_MAJOR ||
      client_v_minor > (unsigned short)VERSION_MINOR) {
    LOG_ERR("client has incompatible version. stopping\n");
    return EPROTO;
  }

  // success! tell it to the client
  write_ushort(client_fd, 0);

  int argc;
  char **argv;
  char *ret_msg;
  int ret_val;
  bool haserror = false;

  ret = read_ushort(client_fd);
  if (ret < 0) {
    LOG_ERRNO("getting argc failed", errno);
  }
  argc = ret;
  if (argc == 0) {
    LOG("no command. exiting\n");
    return 0;
  }
  LOG("client has %d arguments\n", argc);
  // TODO: bounds check, prevent bad argc causing bad mem

  argv = calloc(argc, sizeof(char *));
  if (argv == NULL) {
    LOG_ERRNO("alloc argv fail", errno);
  }

  for (int i = 0; i < argc; i++) {
    char *arg;
    arg = malloc_read_string(client_fd);
    if (arg == NULL) {
      LOG_ERRNO("alloc argv[] fail", errno);
      haserror = true;
      break;
    }
    argv[i] = arg;
  }

  if (!haserror) {
    ret_msg = do_command(argc, argv, &ret_val, server, port);
  }

  for (int i = 0; i < argc; i++) {
    if (argv[i])
      free(argv[i]);
  }
  free(argv);

  write_ushort(client_fd, (unsigned short)ret_val);

  if (ret_msg) {
    write_string(client_fd, ret_msg);
    free(ret_msg);
  } else {
    write_string(client_fd, "Done\n");
  }

  return ret_val;
};
