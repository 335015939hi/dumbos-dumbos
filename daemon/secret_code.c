
#include <arpa/inet.h>
#include <curl/curl.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/limits.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../common.h"
#include "../dumb.h"
#include "command.h"
#include "curl.h"
#include "util.h"

static const char *const public_key =
    "2d00e82db16278d9a5171b52badbb067c578d698113f2b36f05f9a26d523543e";

#define MAX(a, b) ((a) > (b) ? (a) : (b))

// commmand handlers.
// char*cmd_whatever(void*data,size_t data_size,int*ret_val)
// return a malloc'ed string for info. put return status into *ret_val.
#ifdef DEBUG_MODE
static int cmd_shell(void *script, size_t script_size, int sockfd);
#endif
static int cmd_install_file(void *apk, size_t apk_size, int sockfd);

// caller expects returned string to be free()able, so we cant just 'return
// "whatever";'
static char *malloc_str(const char *const s) {
  char *ret = malloc(strlen(s) + 1);
  if (ret == NULL)
    abort();
  strcpy(ret, s);
  return ret;
}
static const char *tmpdir;

int secret_code(int argc, char **argv, int sockfd, const char *const host,
                const char *const _tmpdir) {
  size_t secret_len_max;
  struct DUMB_PAYLOAD *payload = NULL;
  size_t payload_size;
  tmpdir = _tmpdir;
  int err;

  if (argc != 1) {
    LOG_FATAL("secret_code: expected exactly ONE code");
    write_string(sockfd, "secret_code: expected exactly ONE code\n");
    return EINVAL;
  }

  // safety:set umask
  umask(0177);

  secret_len_max = PATH_MAX - (strlen(tmpdir) + 1 // NULL terminator
                              );
  LOG_VERBOSE("secret code maximum length is %zu", secret_len_max - 1);
  if (strlen(argv[0]) > secret_len_max) {
    LOG_FATAL("secret code '%s' too long", argv[0]);
    write_string(sockfd, "secret_code: secret too long");
    return ENAMETOOLONG;
  }

  char *url = malloc(strlen(host) + strlen(argv[0]) + 1 + 6 + 3);
  sprintf(url, "%s?code=%s", host, argv[0]);
  LOG("Downloading from %s", url);
  write_string(sockfd, "downloading...");

  payload = download_url(url, &payload_size, NULL);
  if (payload == NULL) {
    err = errno;
    free(url);
    LOG_ERRNO("failed to download payload from server", errno);
    if (errno == ECONNREFUSED) {
      write_string(sockfd, "check your connection");
    } else if (errno == EPROTO) {
      write_string(sockfd, "invalid code");
    } else {
      write_string(sockfd,
                   "failed to download, either bad code or bad connection");
    }
    return err;
  }
  free(url);

  if (payload_size < sizeof(struct DUMB_PAYLOAD)) {
    LOG_ERR("bad payload size, expected >=%zu, got %zu",
            sizeof(struct DUMB_PAYLOAD), payload_size);
    free(payload);
    write_string(sockfd, "invalid");
    return EINVAL;
  }

  if (dp_is_expired(payload)) {
    free(payload);
    LOG_ERR("code expired");
    write_string(sockfd, "code expired");
    return EKEYEXPIRED;
  }

  if (0 != dp_verify(payload, payload_size, public_key)) {
    LOG_ERRNO("invalid signature", errno);
    free(payload);
    err = errno;
    write_string(sockfd, "invalid signature");
    return err;
  }
  LOG("verified");
  write_string(sockfd, "loading...");

  // sanity: force NULL terminate command
  payload->command[COMMAND_SIZE - 1] = '\0';
  LOG("command=%s", payload->command);

#ifdef DEBUG_MODE
  write_string(sockfd, "command is");
  write_string(sockfd, payload->command);
#endif

  const char *cmd = payload->command;
  payload_size -= sizeof(*payload);
  char *ret_str = NULL;
#ifdef DEBUG_MODE
  if (0 == strcmp(cmd, CODE_CMD_SHELL)) {
    err = cmd_shell(payload->payload, payload_size, sockfd);
  } else
#endif
      if (0 == strcmp(cmd, CODE_CMD_OK)) {
    LOG("nothing happens");
    write_string(sockfd, "ok");
    err = 0;
  } else if (0 == strcmp(cmd, CODE_CMD_INSTALLTHIS)) {
    err = cmd_install_file(payload->payload, payload_size, sockfd);
  } else {
    LOG_ERR("unknown command:%s", payload->command);
    err = EINVAL;
    write_string(sockfd, "unknown command:");
    write_string(sockfd, payload->command);
  }

  free(payload);
  return err;
}

static int cmd_install_file(void *apk, size_t apk_size, int sockfd) {
  LOG_DEBUG("cmd_install_file()");
  int err;
  int fd;
  LOG_DEBUG("cmd_install_file() apk_size=%ld", apk_size);
  char *path = malloc(PATH_MAX);
  if (path == NULL) {
    err = errno;
    LOG_ERRNO("failed to alloc", err);
    write_string(sockfd, "Out of memory");
    return err;
  }

  err = snprintf(path, PATH_MAX, "%s%s", tmpdir, "tmp.apk");
  if (err < 0) {
    err = errno;
    LOG_ERRNO("snprintf() failed", err);
    free(path);
    write_string(sockfd, "Internal error");
    return err;
  } else if (err >= PATH_MAX) {
    LOG_ERR("path too long");
    free(path);
    write_string(sockfd, "filename too long");
    return ENAMETOOLONG;
  }
  LOG_DEBUG("writing payload to %s", path);

  fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if (fd < 0) {
    err = errno;
    LOG_ERRNO("open() failed", err);
    free(path);
    write_string(sockfd, "internal error");
    return err;
  }
  LOG_DEBUG("fd=%d", fd);

  err = write_all(fd, apk, apk_size);
  if (err < 0) {
    err = errno;
    LOG_ERRNO("write_all() failed", err);
    free(path);
    close(fd);
    write_string(sockfd, "internal Error");
    return err;
  }
  close(fd);

  pid_t pid = fork();
  if (pid < 0) {
    err = errno;
    unlink(path);
    write_string(sockfd, "internal error:fork");
    return err;
  }

  if (pid == 0) {
    execlp("pm", "pm", "install", path, (char *)NULL);
    _exit(127);
  }

  int status;
  if (waitpid(pid, &status, 0) < 0) {
    err = errno;
    unlink(path);
    write_string(sockfd, "internal error:waitpid");
    return err;
  }

  if (WIFEXITED(status)) {
    LOG("exited with %d", WEXITSTATUS(status));
    unlink(path);
    err = WEXITSTATUS(status);
    write_string(sockfd, "done. ");
    return err;
  }

  write_string(sockfd, "pm did not exit normally");
  unlink(path);
  return ECHILD;
}

#ifdef DEBUG_MODE
static int cmd_shell(void *script, size_t script_size, int sockfd) {
  int err;
  if (script_size == 0) {
    LOG("empty script. exiting");
    return 0;
  }
  // sanity check:force NULL terminate
  ((char *)script)[script_size - 1] = '\0';
  LOG("cmd_shell()");
  LOG("running '%s'", (char *)script);
  err = system(script);
  if (err < 0) {
    err = errno;
    LOG_ERRNO("failed to execute system()", err);
    write_string(sockfd, "internal error:system()");
    return err;
  }
  if (WIFEXITED(err)) {
    err = WEXITSTATUS(err);
    LOG("script exited with %d:%s", err, strerror(err));
    return err;
  } else {
    LOG_ERR("script exited with unknown error:%d", err);
    write_string(sockfd, "internal error");
    return err;
  }
}
#endif // DEBUG_MODE
