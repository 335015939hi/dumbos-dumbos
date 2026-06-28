
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common.h"
#include "daemon.h"

static const struct option long_opts[];
static const char *const short_opts;
void display_help(FILE *, const char *const);

int main(int argc, char **argv) {
#ifdef DEBUG_MODE
  printf("WARNING: running in debug mode. if you are not developing this "
         "program, please report this to someone.\n");
#endif

  bool error = 0;
  int option;

  // whether to fork to background
  bool opt_fork = true;
  char *opt_socket_path = DEFAULT_SOCKET_PATH;
  char *opt_server = DEFAULT_SERVER;
  unsigned int opt_port = DEFAULT_PORT;

  char *end;

  while (-1 !=
         (option = getopt_long(argc, argv, short_opts, long_opts, NULL))) {
    switch (option) {
    case 'h':
      display_help(stdout, argv[0]);
      return 0; // or exit(0). i like return better
      break;
    case 'v':
      fprintf(stdout, "dumbosd version %s versioncode %d\n", VERSION_STRING,
              VERSION_CODE);
      return 0;
      break;
    case 'F':
      opt_fork = false;
      break;
    case 's':
      opt_socket_path = optarg;
      break;
    case 'H':
      opt_server = optarg;
      break;
    case 'p':
      opt_port = strtoul(optarg, &end, 10);
      if (errno == ERANGE || end == optarg || *end != '\0' ||
          opt_port > 65536) {
        print_error("invalid port");
        error = EINVAL;
      }
      break;
    default:
      display_help(stderr, argv[0]);
      error = EINVAL;
      break;
    }
    if (error)
      return error; // or exit(error)
  }

  if (opt_fork) {
    pid_t pid = fork();
    if (pid < 0) {
      fprintf(stderr, "Fork failed\n");
      return errno;
    } else if (pid != 0) { // parent
      fprintf(stdout, "Forked to background. PID=%d\n", pid);
      return 0;
    }
  }

  return start_daemon(opt_socket_path, opt_server, opt_port);
}

static const struct option long_opts[] = {
    {"help", no_argument, 0, 'h'},         {"version", no_argument, 0, 'v'},
    {"fore", no_argument, 0, 'F'},         {"foreground", no_argument, 0, 'F'},
    {"socket", required_argument, 0, 's'}, {"host", required_argument, 0, 'H'},
    {"port", required_argument, 0, 'p'},   {0, 0, 0, 0}};
static const char *const short_opts = "hvFs:H:p:";

void display_help(FILE *file, const char *const argv0) {
  const char *const help_text =
      // TODO
      "Usage:%s [options] [--] <command> [command args]\n"
#ifdef DEBUG_MODE
      "debug mode options:\n"
#endif
      "Options:\n"
      " -h,--help              display this help text\n"
      " -v,--version           display version and exit\n"
      " -F,--fore,--foreground do not fork, stay in foreground\n"
      " -s,--socket=<path>     socket path, default " DEFAULT_SOCKET_PATH "\n"
      " -H,--host=<host>       server host, default " DEFAULT_SERVER "\n"
      " -p,--port=<port>       server port, default %d\n"

      ;
  fprintf(file, help_text, argv0, DEFAULT_PORT);
}
