
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <unistd.h>

#include "../common.h"
#include "client.h"

// full path of the default socket. see daemon/main.c
#define DEFAULT_SOCKET_PATH "/dev/socket/dumbosd.socket"

static const struct option long_opts[];
static const char *const short_opts;
void display_help(FILE *, const char *const);

int main(int argc, char **argv) {
  const char *opt_socket = DEFAULT_SOCKET_PATH;
  LOG_DEBUG("argv[0]=%s pid=%ld", argv[0], (long)getpid());

  int option;
  int err;

  while (-1 !=
         (option = getopt_long(argc, argv, short_opts, long_opts, NULL))) {
    int error = 0;
    switch (option) {
    case 'h':
      LOG_DEBUG("found option help");
      display_help(stdout, argv[0]);
      return 0;
      break;
    case 'v':
      LOG_DEBUG("found option version");
      fprintf(stdout, "dumbos client version %s versioncode %d.%d.%d\n",
              VERSION_STRING, VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
      return 0;
      break;
    case 'l':
      LOG_DEBUG("found option log-level=%s", optarg);
      err = set_log_verbosity(optarg);
      if (err != 0) {
        LOG_WARN("invalid log verbosity '%s'", optarg);
      } else {
        LOG_DEBUG("log verbosity set to %d", log_verbosity);
      }
      break;
    case 's':
      LOG_DEBUG("found option socket=%s", optarg);
      opt_socket = optarg;
      break;
    default:
      LOG_DEBUG("found unhandled option");
      display_help(stderr, argv[0]);
      error = EINVAL;
      break;
    }
    if (error)
      return error;
  }

  int ret;

  LOG_DEBUG("starting client()");
  ret = client(argc - optind, &argv[optind], opt_socket);
  LOG_DEBUG("client() exited with %d", ret);

  return ret;
}

static const struct option long_opts[] = {
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'v'},
    {"log-level", required_argument, NULL, 'l'},
    {"socket", required_argument, NULL, 's'},
    {0, 0, 0, 0}};
static const char *const short_opts = "hvl:s:";
void display_help(FILE *f, const char *const argv0) {
  static const char *const help_str =
      "Usage:%s [options] [--] command [command args]\n"
      "Options:\n"
      " -h,--help             display this help\n"
      " -v,--version          display version\n"
      " -l,--log-level=<lvl>  set log level. accepts " LOG_NONE_NAME
      " " LOG_FATAL_NAME " " LOG_ERROR_NAME " " LOG_WARN_NAME
      " " LOG_NORMAL_NAME " " LOG_VERBOSE_NAME " " LOG_DEBUG_NAME
      " " LOG_MAX_NAME "\n"
      " -s,--socket=<path>    set socket path\n";

  fprintf(f, help_str, argv0);
};
