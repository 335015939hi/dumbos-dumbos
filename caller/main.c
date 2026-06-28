
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <unistd.h>

#include "../common.h"
#include "client.h"

static const struct option long_opts[];
static const char *const short_opts;
void display_help(FILE *, const char *const);

int main(int argc, char **argv) {
  const char *opt_socket = DEFAULT_SOCKET_PATH;

  int option;

  while (-1 !=
         (option = getopt_long(argc, argv, short_opts, long_opts, NULL))) {
    int error = 0;
    switch (option) {
    case 'h':
      display_help(stdout, argv[0]);
      return 0;
      break;
    case 'v':
      fprintf(stdout, "dumbos client version %s versioncode %d\n",
              VERSION_STRING, VERSION_CODE);
      return 0;
      break;
    case 's':
      opt_socket = optarg;
      break;
    default:
      display_help(stderr, argv[0]);
      error = EINVAL;
      break;
    }
    if (error)
      return error;
  }

  int ret;

  ret = client(argc - optind, &argv[optind], opt_socket);

  return ret;
}

static const struct option long_opts[] = {
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'v'},
    {"socket", required_argument, NULL, 's'},
    {0, 0, 0, 0}};
static const char *const short_opts = "hvs:";
void display_help(FILE *f, const char *const argv0) {
  static const char *const help_str =
      "Usage:%s [options] [--] command [command args]\n"
      "Options:\n"
      " -h,--help           display this help\n"
      " -v,--version        display version\n"
      " -s,--socket=<path>  set socket path\n";

  fprintf(f, help_str, argv0);
};
