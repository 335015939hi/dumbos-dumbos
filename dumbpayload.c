
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
#include "ed25519.h"

// checks code for illegal characters
// FIXME: i wrote a better one somewhere, use that instead
static bool check_code_allowed_chars(const char *const code) {
  unsigned int len = strlen(code);
  if (len > CODE_MAXLEN) {
    return false;
  }
  for (unsigned int i = 0; i < len; i++) {
    bool good = false;
    for (unsigned int j = 0; j < strlen(SECRET_CODE_ALLOWED_CHARS); j++) {
      if (SECRET_CODE_ALLOWED_CHARS[j] == code[i]) {
        good = true;
        break;
      }
    }
    if (!good) {
      return false;
    }
  }
  return true;
}

int dp_sign(struct DUMB_PAYLOAD *payload, size_t size,
            const char *private_key_hex) {
  char signature[ED25519_SIGNATURE_HEX_SIZE];
  int err;
  // make sure private_key_hex is the right size
  if (strlen(private_key_hex) != ED25519_PRIVATE_KEY_HEX_SIZE - 1) {
    errno = EINVAL;
    return -1;
  }
  // basic check to make sure payload is valid (or at least valid size)
  if (size < sizeof(struct DUMB_PAYLOAD)) {
    errno = EINVAL;
    return -1;
  }
  // zero out the signature. signing and verifying should all zero out the
  // signature field, for consistancy
  memset(payload->signature, '\0', ED25519_SIGNATURE_HEX_SIZE);

  // where the magic happens
  err = ed25519_sign_hex(private_key_hex, payload, size, signature);
  if (err < 0) {
    return err;
  }
  // copy the signature to payload
  memcpy(payload->signature, signature, ED25519_SIGNATURE_HEX_SIZE);
  return 0;
}

int dp_verify(struct DUMB_PAYLOAD *payload, size_t size,
              const char *pubkey_hex) {
  char signature[ED25519_SIGNATURE_HEX_SIZE];
  if (strlen(pubkey_hex) != ED25519_PUBLIC_KEY_HEX_SIZE - 1) {
    errno = EINVAL;
    return -1;
  }
  // basic check to make sure payload is valid (or at least valid size)
  if (size < sizeof(struct DUMB_PAYLOAD)) {
    errno = EINVAL;
    return -1;
  }
  // copy the signature to a temporary buffer, and zero out signature field in
  // payload. signing and verifying should all zero out the signature field, for
  // consistancy
  memcpy(signature, payload->signature, ED25519_SIGNATURE_HEX_SIZE);
  memset(payload->signature, '\0', ED25519_SIGNATURE_HEX_SIZE);
  // magic
  return ed25519_verify_hex(pubkey_hex, payload, size, signature);
}

void dp_set_expire(struct DUMB_PAYLOAD *payload, time_t expire) {
  memset(payload->expire, '\0', EXPIRE_SIZE);
  snprintf(payload->expire, EXPIRE_SIZE, "%lld", (long long)expire);
}

time_t dp_get_expire(const struct DUMB_PAYLOAD *payload) {
  long long expire;
  int err;
  char buf[EXPIRE_SIZE + 1];
  memcpy(buf, payload->expire, EXPIRE_SIZE);
  // just in case its not NULL-terminated
  buf[EXPIRE_SIZE + 1 - 1] = '\0';
  err = parse_long_long(buf, &expire);
  if (err < 0) {
    return (time_t)-1;
  }
  return (time_t)expire;
}

time_t dp_get_expire_or_set(struct DUMB_PAYLOAD *payload) {
  time_t expire = dp_get_expire(payload);
  // check if expire field is an invalid integer. if invalid, check for the ':'
  // prefix, or fallback to default behaviour. if valid, expire will hold the
  // expire date in epoch seconds
  if (expire == (time_t)-1) {
    // check for the magic ':' that indicates custom expire-after-use time
    if (payload->expire[0] == ':') {
      // shift the expire field left 1 byte, and check again
      memmove(payload->expire, payload->expire + 1, EXPIRE_SIZE - 1);
      expire = dp_get_expire(payload);
      if (expire == (time_t)-1) {
        // still invalid. we assume default behaviour
        expire = time(NULL) + DEFAULT_EXPIRE_TIME;
      } else {
        // expire is the custom expire-after-user time, we add current time to
        // it
        expire += time(NULL);
      }
    } else {
      // default behaviour
      expire = time(NULL) + DEFAULT_EXPIRE_TIME;
    }
    dp_set_expire(payload, expire);
  }
  return expire;
}

bool dp_is_expired(struct DUMB_PAYLOAD *payload) {
  time_t expire = dp_get_expire_or_set(payload);
  // check for infinite expire time
  if (expire == 0)
    return false;
  return expire <= time(NULL);
}

bool dp_is_expired_compare(struct DUMB_PAYLOAD *payload, time_t cur_time) {
  time_t expire = dp_get_expire_or_set(payload);
  // check for infinite expire
  if (expire == 0)
    return false;
  return expire <= cur_time;
}

void *dp_malloc_load(const char *path, size_t *ret_size) {
  size_t size;
  int fd;
  struct DUMB_PAYLOAD *payload;
  int err;
  struct stat stat;
  LOG_DEBUG("dp_malloc_load() started");

  LOG("using path '%s'", path);

  fd = open(path, O_RDONLY);
  if (fd < 0) {
    LOG_ERRNO("failed to open", errno);
    return NULL;
  }
  LOG("opened file '%s',fd=%d", path, fd);

  err = fstat(fd, &stat);
  if (err < 0) {
    LOG_ERRNO("failed to fstat", errno);
    close(fd);
    return NULL;
  }

  size = stat.st_size;
  LOG("file size is %ld", size);
  if (size < sizeof(struct DUMB_PAYLOAD)) {
    LOG_ERR("file too small");
    errno = EINVAL;
    close(fd);
    return NULL;
  }
  // TODO: set a max payload size
  if (size > SIZE_MAX - 1) {
    errno = EOVERFLOW;
    LOG_ERR("file too big");
    close(fd);
    return NULL;
  }

  payload = malloc(size + 1);
  if (payload == NULL) {
    LOG_ERRNO("failed to malloc", errno);
    close(fd);
    return NULL;
  }

  err = read_all(fd, payload, size);
  if (err < 0) {
    LOG_ERRNO("failed to read_all()", errno);
    maybe_free(payload);
    close(fd);
    return NULL;
  }

  close(fd);
  *ret_size = size;
  return payload;
}
