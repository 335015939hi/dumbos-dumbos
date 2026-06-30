
#include <errno.h>
#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common.h"
#include "daemon.h"

#define DEFAULT_SOCKET_CONTEXT ""
#define DEFAULT_TMPDIR "/data/local/tmp/dumb/"

static const struct option long_opts[];
static const char *const short_opts;
void display_help(FILE *, const char *const);

int main(int argc, char **argv) {
#ifdef DEBUG_MODE
  LOG_WARN("running in debug mode. if you are not developing this "
           "program, please report this to someone.");
#endif
  LOG_DEBUG("argv[0]=%s PID=%ld argc=%d", argv[0], (long)getpid(), argc);

  bool error = 0;
  int option;
  int err;

  struct daemon_opts *opt;
  opt = malloc(sizeof(struct daemon_opts));

  // whether to fork to background
  bool opt_fork = true;
  opt->path = DEFAULT_SOCKET_PATH;
  opt->server = DEFAULT_SERVER;
  opt->port = DEFAULT_PORT;
  opt->con = DEFAULT_SOCKET_CONTEXT;
  opt->tmpdir = DEFAULT_TMPDIR;
  const char *opt_sock_mode_str = DEFAULT_SOCKET_MODE;
  const char *opt_sock_own_str = DEFAULT_SOCKET_OWNER;
#ifdef DEBUG_MODE
  bool opt_unlink = false;
#endif

  char *end;

  while (-1 !=
         (option = getopt_long(argc, argv, short_opts, long_opts, NULL))) {
    switch (option) {
    case 'h':
      LOG_DEBUG("found option help");
      display_help(stdout, argv[0]);
      return 0; // or exit(0). i like return better
      break;
    case 'v':
      LOG_DEBUG("found option version");
      fprintf(stdout, "dumbosd version %s versioncode %d.%d.%d\n",
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
    case 'F':
      LOG_DEBUG("found option fore");
      opt_fork = false;
      break;
    case 's':
      LOG_DEBUG("found option socket=%s", optarg);
      opt->path = optarg;
      break;
    case 'H':
      LOG_DEBUG("found option host=%s", optarg);
      opt->server = optarg;
      break;
    case 'o':
      LOG_DEBUG("found option owner=%s", optarg);
      opt_sock_own_str = optarg;
      break;
    case 'm':
      LOG_DEBUG("found option mode=%s", optarg);
      opt_sock_mode_str = optarg;
      break;
    case 'p':
      LOG_DEBUG("found option port=%s", optarg);
      opt->port = optarg;
      break;
    case 'Z':
      LOG_DEBUG("found option context=%s", optarg);
      opt->con = optarg;
      break;
    case 't':
      LOG_DEBUG("found option tmpdir=%s", optarg);
      opt->tmpdir = optarg;
      break;
#ifdef DEBUG_MODE
    case 'u':
      LOG_DEBUG("found option unlink");
      opt_unlink = true;
      break;
#endif
    default:
      LOG_DEBUG("found unhandled option");
      display_help(stderr, argv[0]);
      error = EINVAL;
      break;
    }
    if (error)
      return error; // or exit(error)
  }

#ifdef DEBUG_MODE
  if (opt_unlink) {
    err = unlink(opt->path);
    LOG("unlink exit with %d, errno %d", err, errno);
  }
#endif

  errno = 0;
  opt->mode = (mode_t)strtoul(opt_sock_mode_str, &end, 8);
  if (errno || *end != '\0' || opt->mode < 0) {
    LOG_FATAL("invalid mode '%s'", opt_sock_mode_str);
    return EINVAL;
  }

  char *sock_own_str = strdup(opt_sock_own_str);
  char *colon = strchr(sock_own_str, ':');
  if (NULL == colon) {
    LOG_ERR("bad user:group '%s'", opt_sock_own_str);
    free(sock_own_str);
    return EINVAL;
  }
  *colon = '\0';
  char *sock_user_str = sock_own_str;
  char *sock_grp_str = colon + 1;
  if (sock_user_str[0] >= '0' && sock_user_str[0] <= '9') {
    errno = 0;
    opt->uid = strtol(sock_user_str, &end, 10);
    if (*end != '\0' || errno) {
      LOG_ERR("bad user '%s'", sock_user_str);
      free(sock_own_str);
      return EINVAL;
    }
  } else {
    struct passwd *pw = getpwnam(sock_user_str);
    if (pw == NULL) {
      LOG_ERR("bad user  '%s'", sock_user_str);
      free(sock_own_str);
      return EINVAL;
    }
    opt->uid = pw->pw_uid;
  }
  if (sock_grp_str[0] >= '0' && sock_grp_str[0] <= '9') {
    errno = 0;
    opt->gid = strtol(sock_grp_str, &end, 10);
    if (*end != '\0' || errno) {
      LOG_ERR("bad group '%s'", sock_grp_str);
      free(sock_own_str);
      return EINVAL;
    }
  } else {
    struct group *gr = getgrnam(sock_grp_str);
    if (gr == NULL) {
      LOG_ERR("bad group '%s'", sock_grp_str);
      free(sock_own_str);
      return EINVAL;
    }
    opt->gid = gr->gr_gid;
  }

  free(sock_own_str);

  if (opt_fork) {
    pid_t pid = fork();
    if (pid < 0) {
      LOG_FATAL_ERRNO("Fork failed", errno);
      return errno;
    } else if (pid != 0) { // parent
      LOG("Forked to background. PID=%d", pid);
      return 0;
    }
  }

  LOG_DEBUG("reached start_daemon()");
  err = start_daemon(opt);
  LOG_DEBUG("exited start_daemon() code %d", err);
  free(opt);
  return err;
}

static const struct option long_opts[] = {
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'v'},
    {"log-level", required_argument, 0, 'l'},
    {"fore", no_argument, 0, 'F'},
    {"foreground", no_argument, 0, 'F'},
    {"socket", required_argument, 0, 's'},
    {"host", required_argument, 0, 'H'},
    {"port", required_argument, 0, 'p'},
    {"mode", required_argument, 0, 'm'},
    {"owner", required_argument, 0, 'o'},
    {"context", required_argument, 0, 'Z'},
    {"tmpdir", required_argument, 0, 't'},

#ifdef DEBUG_MODE
    {"unlink", no_argument, 0, 'u'},
#endif
    {0, 0, 0, 0}};
static const char *const short_opts = "hvl:Fs:H:p:m:o:Z:t:"
#ifdef DEBUG_MODE
                                      "u"
#endif
    ;

void display_help(FILE *file, const char *const argv0) {
  const char *const help_text =
      // TODO
      "Usage:%s [options] [--] <command> [command args]\n"
#ifdef DEBUG_MODE
      "debug mode options:\n"
      " -u,--unlink            unlink socket path first\n"
#endif
      "Options:\n"
      " -h,--help               display this help text\n"
      " -v,--version            display version and exit\n"
      " -l,--log-level=<lvl>    set log level. accepts " LOG_NONE_NAME
      " " LOG_FATAL_NAME " " LOG_ERROR_NAME " " LOG_WARN_NAME
      " " LOG_NORMAL_NAME " " LOG_VERBOSE_NAME " " LOG_DEBUG_NAME
      " " LOG_MAX_NAME "\n"
      " -F,--fore,--foreground  do not fork, stay in foreground\n"
      " -s,--socket=<path>      socket path, default " DEFAULT_SOCKET_PATH "\n"
      " -H,--host=<host>        server host, default " DEFAULT_SERVER "\n"
      " -p,--port=<port>        server port, default " DEFAULT_PORT "\n"
      " -m,--mode=<octal>       socket permissions, "
      "default " DEFAULT_SOCKET_MODE "\n"
      " -o,--owner=<user:group> socket ownership, default " DEFAULT_SOCKET_OWNER
      "\n"
      " -Z,--context=<con>      socket SELinux context, "
      "default " DEFAULT_SOCKET_CONTEXT "\n"
      " -t,--tmpdir=<tmp>       use <tmp> as tempoary directory instead "
      "!!!<tmp> will be cleared!!! "
      "of " DEFAULT_TMPDIR "\n"
      "\n"

      ;
  fprintf(file, help_text, argv0);
}
