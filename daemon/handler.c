
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common.h"
#include "handler.h"

int handler(const int client_fd) {
  char *client_v_str;
  unsigned short client_v_major;
  unsigned short client_v_minor;
  unsigned short client_v_patch;
  signed long ret;

  client_v_str = malloc_read_string(client_fd);
  if (client_v_str == NULL) {
    print_error("reading client version string fail:");
    print_error(strerror(errno));
    print_error("\n");
    return errno;
  }

  fprintf(stdout, "client version string is %s\n", client_v_str);

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
    print_error("reading client version code fail:");
    print_error(strerror(errno));
    print_error("\n");
    free(client_v_str);
    return errno;
  }

  fprintf(stdout, "client version code is %hu.%hu.%hu\n", client_v_major,
          client_v_minor, client_v_patch);

  write_ushort(client_fd, 0);

  free(client_v_str);
  return 0;
};
