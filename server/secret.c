
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common.h"
#include "secret.h"

int handle_secret(const int fd) {
  LOG_DEBUG("handle_secret() started");
  LOG_DEBUG("fd=%d", fd);

  char *secret_code;

  LOG("secret code handler");

  LOG_DEBUG("reading secret code");
  secret_code = malloc_read_string(fd);
  if (NULL == secret_code) {
    LOG_ERRNO("read secret code failed", errno);
    return 0;
  }

  LOG("recieved secret code '%s'", secret_code);
  // TODO:

  free(secret_code);
  return 0;
}
