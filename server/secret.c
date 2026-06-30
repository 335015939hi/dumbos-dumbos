
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common.h"
#include "secret.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

bool secret_str_safe(const char *const s);

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
    return EACCES;
  }

  if (secret_code_len + strlen(EXT_SIG) >= PATH_MAX ||
      secret_code_len + strlen(EXT_CODE) >= PATH_MAX) {
    free(secret_code);
    LOG_ERR("secret code too long");
    return ENAMETOOLONG;
  }

  free(secret_code);
  return 0;
}

bool secret_str_safe(const char *const s) {
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
