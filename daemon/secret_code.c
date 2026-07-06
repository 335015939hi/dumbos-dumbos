
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
static char *cmd_shell(void *script, size_t script_size, int *ret_val);
#endif
static char *cmd_install_file(void *apk, size_t apk_size, int *ret_val);

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

char *secret_code(int argc, char **argv, int *ret_val, const char *const host,
                  const char *const _tmpdir) {
  size_t secret_len_max;
  struct DUMB_PAYLOAD *payload = NULL;
  size_t payload_size;
  tmpdir = _tmpdir;

  if (argc != 1) {
    *ret_val = EINVAL;
    LOG_ERR("secret_code: expected exactly ONE code");
    return malloc_str("secret_code: expected exactly ONE code\n");
  }

  // safety:set umask
  umask(0177);

  secret_len_max =
      PATH_MAX - (strlen(tmpdir) + MAX(strlen(EXT_SIG), strlen(EXT_CODE)) +
                  1 // NULL terminator
                 );
  LOG_VERBOSE("secret code maximum length is %zu", secret_len_max - 1);
  if (strlen(argv[0]) > secret_len_max) {
    LOG_ERR("secret code '%s' too long", argv[0]);
    *ret_val = ENAMETOOLONG;
    return malloc_str("secret_code: secret too long");
  }

  char *url = malloc(strlen(host) + strlen(argv[0]) + 1 + 6 + 3);
  sprintf(url, "%s?code=%s", host, argv[0]);
  LOG("Downloading from %s", url);

  payload = download_url(url, &payload_size, NULL);
  if (payload == NULL) {
    free(url);
    LOG_ERRNO("failed to download payload from server", errno);
    *ret_val = errno;
    return malloc_str("check your connection (or bad code)");
  }
  free(url);

  if (payload_size < sizeof(struct DUMB_PAYLOAD)) {
    LOG_ERR("bad payload size, expected >=%d, got %ld",
            sizeof(struct DUMB_PAYLOAD), payload_size);
    *ret_val = EINVAL;
    return malloc_str("invalid file");
  }

  if (dp_is_expired(payload)) {
    free(payload);
    LOG_ERR("code expired");
    *ret_val = EKEYEXPIRED;
    return malloc_str("code expired");
  }

  if (0 != dp_verify(payload, payload_size, public_key)) {
    LOG_ERRNO("bad signature", errno);
    free(payload);
    *ret_val = errno;
    return malloc_str("bad signature");
  }
  LOG("verified");

  // sanity: force NULL terminate command
  payload->command[COMMAND_SIZE - 1] = '\0';
  LOG("command=%s", payload->command);

  const char *cmd = payload->command;
  payload_size -= sizeof(*payload);
  char *ret_str = NULL;
#ifdef DEBUG_MODE
  if (0 == strcmp(cmd, CODE_CMD_SHELL)) {
    ret_str = cmd_shell(payload->payload, payload_size, ret_val);
  } else
#endif
      if (0 == strcmp(cmd, CODE_CMD_OK)) {
    LOG("nothing happens");
    *ret_val = 0;
    ret_str = NULL;
  } else if (0 == strcmp(cmd, CODE_CMD_INSTALLTHIS)) {
    ret_str = cmd_install_file(payload->payload, payload_size, ret_val);
  } else {
    LOG_ERR("unknown command:%s", payload->command);
    *ret_val = EINVAL;
    ret_str = malloc_str("bad command");
  }

  free(payload);
  return ret_str;
}

static char *cmd_install_file(void *apk, size_t apk_size, int *ret_val) {
  LOG_DEBUG("cmd_install_file()");
  int err;
  int fd;
  LOG_DEBUG("cmd_install_file() apk_size=%ld", apk_size);
  char *path = malloc(PATH_MAX);
  if (path == NULL) {
    LOG_ERRNO("failed to alloc", errno);
    *ret_val = errno;
    return malloc_str("Out of memory");
  }

  err = snprintf(path, PATH_MAX, "%s%s", tmpdir, "tmp.apk");
  if (err < 0) {
    LOG_ERRNO("snprintf() failed", errno);
    *ret_val = errno;
    free(path);
    return malloc_str("Internal error");
  } else if (err >= PATH_MAX) {
    LOG_ERR("path too long");
    *ret_val = ENAMETOOLONG;
    return malloc_str("path too long");
  }
  LOG_DEBUG("writing payload to %s", path);

  fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if (fd < 0) {
    LOG_ERRNO("open() failed", errno);
    *ret_val = errno;
    free(path);
    return malloc_str("internal error");
  }
  LOG_DEBUG("fd=%d", fd);

  err = write_all(fd, apk, apk_size);
  if (err < 0) {
    LOG_ERRNO("write_all() failed", errno);
    *ret_val = errno;
    free(path);
    close(fd);
    return malloc_str("internal error");
  }
  close(fd);

  // TODO: don't use system(), probably unsafe
  char *system_cmd;
  asprintf(&system_cmd, "pm install '%s'", path);
  LOG("running %s", system_cmd);
  err = system(system_cmd);
  free(system_cmd);
  LOG("exited with %d", WEXITSTATUS(err));
  // unlink(path);
  free(path);
  *ret_val = WEXITSTATUS(err);
  return NULL;
}

#ifdef DEBUG_MODE
static char *cmd_shell(void *script, size_t script_size, int *ret_val) {
  int err;
  // sanity check
  ((char *)script)[script_size - 1] = '\0';
  LOG("cmd_shell()");
  LOG("running '%s'", (char *)script);
  err = system(script);
  if (err < 0) {
    LOG_ERRNO("failed to execute system()", errno);
    *ret_val = errno;
    return malloc_str("internal error");
  }
  if (WIFEXITED(err)) {
    err = WEXITSTATUS(err);
    LOG("script exited with %d:%s", err, strerror(err));
    *ret_val = err;
    return NULL;
  } else {
    LOG_ERR("script exited with unknown error:%d", err);
    *ret_val = err;
    return malloc_str("internal error");
  }
}
#endif // DEBUG_MODE
