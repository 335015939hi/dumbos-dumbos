
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "dumb.h"
#include "dumbserver.h"
#include "ed25519.h"

void *dp_malloc_check_load(const char *const code, const char *username,
                           size_t *ret_size, const char *ed25519_private_key) {
  char *path;
  char *userpath;
  int err;
  struct stat stat;
  size_t size;
  int codefd;
  struct DUMB_PAYLOAD *payload;
  LOG_DEBUG("dp_malloc_check_load() started");

  if (!check_allowed_chars(code, SECRET_CODE_ALLOWED_CHARS)) {
    LOG_ERR("invalid characters detected in '%s'", code);
    return NULL;
  }

  path = malloc(PATH_MAX);
  if (!path)
    return NULL;
  err = snprintf(path, PATH_MAX, "%s%s%s", CODE_FILE_PATH, CODE_FILE_PREFIX,
                 code);
  if (err >= PATH_MAX) {
    LOG_ERR("path too long");
    maybe_free(path);
    return NULL;
  }
  LOG("using path '%s'", path);

  userpath = malloc(PATH_MAX);
  if (!path)
    return NULL;
  err = snprintf(userpath, PATH_MAX, "%s%s/%s%s", USERDATA_PREFIX, username,
                 CODE_FILE_PREFIX, code);
  if (err >= PATH_MAX) {
    LOG_ERR("path too long");
    maybe_free(path);
    return NULL;
  }
  LOG("using per-user path '%s' (this will be checked first)", userpath);
  // TODO:

  codefd = open(userpath, O_RDWR);
  if (codefd < 0) {
    LOG_DEBUG("failed to open user path:%s", strerror(errno));
    codefd = open(path, O_RDWR);
    if (codefd < 0) {
      maybe_free(path);
      maybe_free(userpath);
      LOG_DEBUG("failed to open default path:%s", strerror(errno);
      LOG_ERR("could not find code");
      return NULL;
    }
    LOG("opened file '%s',fd=%d", path, codefd);
  } else {
    LOG("opened file '%s',fd=%d", userpath, codefd);
  }
  maybe_free(userpath);
  maybe_free(path);

  err = fstat(codefd, &stat);
  if (err < 0) {
    LOG_ERRNO("failed to fstat", errno);
    close(codefd);
    return NULL;
  }

  size = stat.st_size;
  LOG("file size is %ld", size);
  if (size < sizeof(struct DUMB_PAYLOAD)) {
    LOG_ERR("file too small");
    errno = EINVAL;
    close(codefd);
    return NULL;
  }

  payload = malloc(size + 1);
  if (payload == NULL) {
    LOG_ERRNO("failed to malloc", errno);
    close(codefd);
    return NULL;
  }

  err = read_all(codefd, payload, size);
  if (err < 0) {
    LOG_ERRNO("failed to read_all()", errno);
    maybe_free(payload);
    close(codefd);
    return NULL;
  }

  if (dp_is_expired(payload)) {
    maybe_free(payload);
    LOG_ERR("code expired");
    close(codefd);
    return NULL;
  }

  // not yet signed. do not write signed payload to disk.
  err = lseek(codefd, 0, SEEK_SET);
  if (err < 0) {
    LOG_ERR("failed to lseek on fd=%d:%s", codefd, strerror(errno));
  } else {
    err = write_all(codefd, payload, size);
    if (err < 0) {
      LOG_ERR("failed to write to fd=%d:%s", codefd, strerror(errno));
    }
  }
  close(codefd);
  if (err < 0) {
    free(payload);
    return NULL;
  }

  *ret_size = size;

  // do not write signed payload to disk
  err = dp_sign(payload, size, ed25519_private_key);
  if (err < 0) {
    LOG_ERRNO("dp_sign() failed", errno);
    free(payload);
    return NULL;
  }

  return payload;
}
