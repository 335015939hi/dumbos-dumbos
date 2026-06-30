
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <unistd.h>

#include "../common.h"
#include "server.h"

static const struct option long_opts[];
static const char *const short_opts;
void display_help(FILE *, const char *);

#define _STR(x) #x
#define STR(x) _STR(x)
#define DEFAULT_ADDR "0.0.0.0"

int main(int argc, char **argv) {
  int option;
  char *opt_port = STR(DEFAULT_PORT);
  char *opt_addr = DEFAULT_ADDR;
  bool opt_fork = false;
  char *opt_dir;
  char *opt_persist_dir;

  while ((option = getopt_long(argc, argv, short_opts, long_opts, NULL)) !=
         -1) {
    switch (option) {
    case 'h':
      display_help(stdout, argv[0]);
      break;
    case 'v':
      fprintf(stdout, "dumbos_server version %s versioncode %d.%d.%d\n",
              VERSION_STRING, VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
      return 0;
    case 'a':
      opt_addr = optarg;
      break;
    case 'p':
      opt_port = optarg;
      break;
    case 'b':
      opt_fork = true;
      break;
    default:
      display_help(stderr, argv[0]);
      return EINVAL;
    }
  }

  if (argc - optind != 2) {
    print_error("wrong number of arguments\n");
    return EINVAL;
  }

  opt_dir = argv[optind];
  opt_persist_dir = argv[optind + 1];

  if (opt_fork) {
    pid_t childp;
    childp = fork();
    if (childp < 0) {
      print_errno("failed to fork", errno);
      return errno;
    } else if (childp != 0) { // parent
      printf("forked to background. PID=%d", childp);
      return 0;
    }
  }

  return do_server(opt_addr, opt_port, opt_dir, opt_persist_dir);
}

static const struct option long_opts[] = {
    {"help", no_argument, 0, 'h'},       {"version", no_argument, 0, 'v'},
    {"port", required_argument, 0, 'p'}, {"address", required_argument, 0, 'a'},
    {"fork", no_argument, 0, 'b'},       {0, 0, 0, 0}};
static const char *const short_opts = "hvp:a:b";
void display_help(FILE *dest, const char *argv0) {
  fprintf(dest,
          "Usage:%s [options] <dir> <persist dir>\n"
          " dir is the directory that holds all the one time secret codes. "
          "persist dir is the directory with all the persistant secret codes, "
          "defaulting to TODO\n"
          "Options:\n"
          " -h,--help       display this help text\n"
          " -v,--version    display version and exit\n"
          " -a,--address    address to listen to, default TODO\n"
          " -p,--port       port to listen at, default TODO\n"
          " -b,--fork       fork to background\n",
          argv0);
};
