
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common.h"
#include "secret.h"

bool secret_str_safe(const char *const s);

int handle_secret(const int fd) {
  LOG_DEBUG("handle_secret() started");
  LOG_DEBUG("fd=%d", fd);

  char *secret_code;
  bool secret_code_safe;

  LOG("secret code handler");

  LOG_DEBUG("reading secret code");
  secret_code = malloc_read_string(fd);
  if (NULL == secret_code) {
    LOG_ERRNO("read secret code failed", errno);
    return 0;
  }

  LOG("recieved secret code '%s'", secret_code);
  // TODO:
  secret_code_safe = secret_str_safe(secret_code);
  if (!secret_code_safe) {
    return EINVAL;
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
