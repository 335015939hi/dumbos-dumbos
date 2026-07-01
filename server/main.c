
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#define LOG_USE_PID

#include "../common.h"
#include "server.h"

// TODO: kill kids

static const struct option long_opts[];
static const char *const short_opts;
void display_help(FILE *, const char *);

#define DEFAULT_ADDR "0.0.0.0"

int main(int argc, char **argv) {
  int option;
  int err;
  char *opt_port = DEFAULT_PORT;
  char *opt_addr = DEFAULT_ADDR;
  bool opt_fork = false;
  char *opt_dir;
  char *opt_persist_dir;

  LOG_VERBOSE("Our PID is %ld", (long)getpid());
  LOG_DEBUG("found %d arguments", argc);
  LOG_DEBUG("argv[0] is %s", argv[0]);

  while ((option = getopt_long(argc, argv, short_opts, long_opts, NULL)) !=
         -1) {
    switch (option) {
    case 'h':
      LOG_DEBUG("found option help");
      display_help(stdout, argv[0]);
      return 0;
      break;
    case 'v':
      LOG_DEBUG("found option version");
      printf("dumbos_server version %s versioncode %d.%d.%d\n", VERSION_STRING,
             VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
      return 0;
    case 'l':
      LOG_DEBUG("found option log-level=%s", optarg);
      err = set_log_verbosity(optarg);
      if (err != 0) {
        LOG_WARN("invalid log verbosity '%s'", optarg);
      } else {
        LOG_DEBUG("log verbosity set to %d", log_verbosity);
      }
      break;
    case 'a':
      LOG_DEBUG("found option address=%s", optarg);
      opt_addr = optarg;
      break;
    case 'p':
      LOG_DEBUG("found option port=%s", optarg);
      opt_port = optarg;
      break;
    case 'b':
      LOG_DEBUG("found option fork");
      opt_fork = true;
      break;
    default:
      LOG_DEBUG("found unhandled option '%c'", option);
      display_help(stderr, argv[0]);
      return EINVAL;
    }
  }

  if (argc - optind != 2) {
    LOG_FATAL("wrong number of arguments");
    return EINVAL;
  }

  opt_dir = argv[optind];
  opt_persist_dir = argv[optind + 1];
  LOG_VERBOSE("dir set to %s", opt_dir);
  LOG_VERBOSE("used dir set to %s", opt_persist_dir);

  if (opt_fork) {
    pid_t childp;
    childp = fork();
    if (childp < 0) {
      LOG_FATAL_ERRNO("failed to fork", errno);
      return errno;
    } else if (childp != 0) { // parent
      LOG("forked to background. PID=%d", childp);
      LOG_DEBUG("parent with pid %d exiting", getpid());
      return 0;
    }
  }
  LOG_VERBOSE("starting do_server()");
  err = do_server(opt_addr, opt_port, opt_dir, opt_persist_dir);
  LOG_VERBOSE("do_server() exited with %d", err);
  return err;
}

static const struct option long_opts[] = {
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'v'},
    {"log-level", required_argument, 0, 'l'},
    {"port", required_argument, 0, 'p'},
    {"address", required_argument, 0, 'a'},
    {"fork", no_argument, 0, 'b'},
    {0, 0, 0, 0}};
static const char *const short_opts = "hvl:p:a:b";
void display_help(FILE *dest, const char *argv0) {
  fprintf(dest,
          "Usage:%s [options] <dir> <used dir>\n"
          " dir is the directory that holds all the one time secret codes. \n"
          " persist dir is the directory with all the used secret codes, \n"
          " if a code is in both used and normal dirs, it will be treated as "
          "persistant\n"
          "defaulting to TODO\n"
          "Options:\n"
          " -h,--help             display this help text\n"
          " -v,--version          display version and exit\n"
          " -l,--log-level=<lvl>  set log level. accepts " LOG_NONE_NAME
          " " LOG_FATAL_NAME " " LOG_ERROR_NAME " " LOG_WARN_NAME
          " " LOG_NORMAL_NAME " " LOG_VERBOSE_NAME " " LOG_DEBUG_NAME
          " " LOG_MAX_NAME "\n"
          " -a,--address=<addr>   address to listen to, default TODO\n"
          " -p,--port=<port>      port to listen at, default TODO\n"
          " -b,--fork             fork to background\n",
          argv0);
};
