
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common.h"
#include "secret.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

static bool secret_str_safe(const char *const s);

int handle_secret(const int fd, const char *const code_dir,
                  const char *const code_used_dir) {
  LOG_DEBUG("handle_secret() started");
  LOG_DEBUG("fd=%d", fd);

  char *secret_code;
  int secret_code_len;
  bool secret_code_safe;
  int secret_code_len_max;

  secret_code_len_max =
      PATH_MAX - (MAX(strlen(code_dir), strlen(code_used_dir)) +
                  MAX(strlen(EXT_SIG), strlen(EXT_CODE)) + 1 // NULL terminator
                 );
  LOG_VERBOSE("secret code maximum length is %d", secret_code_len_max - 1);

  LOG("secret code handler");

  LOG_DEBUG("reading secret code");
  secret_code = malloc_read_string(fd);
  if (NULL == secret_code) {
    LOG_ERRNO("read secret code failed", errno);
    return 0;
  }
  secret_code_len = strlen(secret_code);

  LOG("recieved secret code '%s'", secret_code);
  // TODO:
  secret_code_safe = secret_str_safe(secret_code);
  if (!secret_code_safe) {
    free(secret_code);
    write_ushort(fd, EINVAL);
    return EINVAL;
  }

  if (secret_code_len + strlen(EXT_SIG) >= PATH_MAX ||
      secret_code_len + strlen(EXT_CODE) >= PATH_MAX) {
    free(secret_code);
    LOG_ERR("secret code too long");
    write_ushort(fd, ENAMETOOLONG);
    return ENAMETOOLONG;
  }

  int codefd;
  int sigfd;
  char *path_tmp;
  path_tmp = malloc(PATH_MAX);
  int err;
  // TODO:malloc check
  sprintf(path_tmp, "%s%s%s", code_dir, secret_code, EXT_CODE);
  LOG_DEBUG("opening %s", path_tmp);
  codefd = open(path_tmp, O_RDONLY);
  if (codefd < 0) {
    LOG_DEBUG("%s not exist", path_tmp);
    LOG_ERR("invalid code '%s'", secret_code);
    free(path_tmp);
    free(secret_code);
    write_ushort(fd, EACCES);
    return EACCES;
  }
  sprintf(path_tmp, "%s%s%s", code_dir, secret_code, EXT_SIG);
  sigfd = open(path_tmp, O_RDONLY);
  if (sigfd < 0) {
    LOG_DEBUG("%s not exist", path_tmp);
    LOG_ERR("invalid code '%s'", secret_code);
    close(codefd);
    free(path_tmp);
    free(secret_code);
    write_ushort(fd, EACCES);
    return EACCES;
  }

  // tell them we like the secret
  err = write_ushort(fd, 0);
  if (err < 0) {
    LOG_ERRNO("acknowlege secret code fail", errno);
    close(codefd);
    close(sigfd);
    free(path_tmp);
    free(secret_code);
    return errno;
  }
  // now send the code and signature files
  sprintf(path_tmp, "%s%s%s", code_dir, secret_code, EXT_CODE);
  LOG_VERBOSE("sending '%s'", path_tmp);
  err = write_file(fd, path_tmp);
  if (err < 0) {
    LOG_ERR("sending %s fail:%s", path_tmp, strerror(errno));
    close(codefd);
    close(sigfd);
    free(path_tmp);
    free(secret_code);
    return errno;
  }
  sprintf(path_tmp, "%s%s%s", code_dir, secret_code, EXT_SIG);
  LOG_VERBOSE("sending '%s'", path_tmp);
  err = write_file(fd, path_tmp);
  if (err < 0) {
    LOG_ERR("sending %s fail:%s", path_tmp, strerror(errno));
    close(codefd);
    close(sigfd);
    free(path_tmp);
    free(secret_code);
    return errno;
  }

  // TODO:cleanup

  close(codefd);
  close(sigfd);
  free(path_tmp);
  free(secret_code);
  return 0;
}

static bool secret_str_safe(const char *const s) {
  const char *i;
  const char *j;
  const char *const allow = SECRET_CODE_ALLOWED_CHARS;

  for (i = s; *i != '\0'; i++) {
    for (j = allow; *j != '\0'; j++) {
      if (*i == *j) {
        break;
      }
    }
    if (*j == '\0') {
      return false;
    }
  }
  return true;
}
