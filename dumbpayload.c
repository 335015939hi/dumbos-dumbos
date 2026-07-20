
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "dumb.h"
#include "ed25519.h"

char ed25519_private_key[ED25519_PRIVATE_KEY_HEX_SIZE];

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
  if (strlen(private_key_hex) != ED25519_PRIVATE_KEY_HEX_SIZE - 1) {
    errno = EINVAL;
    return -1;
  }
  if (size < sizeof(struct DUMB_PAYLOAD)) {
    errno = EINVAL;
    return -1;
  }
  memset(payload->signature, '\0', ED25519_SIGNATURE_HEX_SIZE);

  err = ed25519_sign_hex(private_key_hex, payload, size, signature);
  if (err < 0) {
    return err;
  }
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
  if (size < sizeof(struct DUMB_PAYLOAD)) {
    errno = EINVAL;
    return -1;
  }
  memcpy(signature, payload->signature, ED25519_SIGNATURE_HEX_SIZE);
  memset(payload->signature, '\0', ED25519_SIGNATURE_HEX_SIZE);
  return ed25519_verify_hex(pubkey_hex, payload, size, signature);
}

void dp_set_expire(struct DUMB_PAYLOAD *payload, time_t expire) {
  memset(payload->expire, '\0', EXPIRE_SIZE);
  snprintf(payload->expire, EXPIRE_SIZE, "%ld", expire);
}

time_t dp_get_expire(const struct DUMB_PAYLOAD *payload) {
  long long expire;
  int err;
  char buf[EXPIRE_SIZE + 1];
  buf[EXPIRE_SIZE + 1 - 1] = '\0';
  memcpy(buf, payload->expire, EXPIRE_SIZE);
  err = parse_long_long(buf, &expire);
  if (err < 0) {
    return 0;
  }
  return (time_t)expire;
}

time_t dp_get_expire_or_set(struct DUMB_PAYLOAD *payload) {
  time_t expire = dp_get_expire(payload);
  if (expire == 0) {
    expire = time(NULL) + DEFAULT_EXPIRE_TIME;
    dp_set_expire(payload, expire);
  }
  return expire;
}

bool dp_is_expired(struct DUMB_PAYLOAD *payload) {
  time_t expire = dp_get_expire_or_set(payload);
  return expire <= time(NULL);
}

bool dp_is_expired_compare(struct DUMB_PAYLOAD *payload, time_t cur_time) {
  time_t expire = dp_get_expire_or_set(payload);
  return expire <= cur_time;
}

void *dp_malloc_load(const char *path, size_t *ret_size) {
  size_t size;
  int fd;
  struct DUMB_PAYLOAD *payload;
  int err;
  struct stat stat;

  LOG("486098 using path '%s'", path);

  fd = open(path, O_RDONLY);
  if (fd < 0) {
    LOG_ERRNO("104485 failed to open", errno);
    return NULL;
  }
  LOG("104986 opened file '%s',fd=%d", path, fd);

  err = fstat(fd, &stat);
  if (err < 0) {
    LOG_ERRNO("122983 failed to fstat", errno);
    close(fd);
    return NULL;
  }

  size = stat.st_size;
  LOG("file size is %ld", size);
  if (size < sizeof(struct DUMB_PAYLOAD)) {
    LOG_ERR("375337 file too small");
    errno = EINVAL;
    close(fd);
    return NULL;
  }

  payload = malloc(size + 1);
  if (payload == NULL) {
    LOG_ERRNO("263338 failed to malloc", errno);
    close(fd);
    return NULL;
  }

  err = read_all(fd, payload, size);
  if (err < 0) {
    LOG_ERRNO("139357 failed to read_all()", errno);
    maybe_free(payload);
    close(fd);
    return NULL;
  }

  close(fd);
  *ret_size = size;
  return payload;
}

void *dp_malloc_check_load(const char *const code, size_t *ret_size) {
  char *path;
  int err;
  struct stat stat;
  size_t size;
  int codefd;
  struct DUMB_PAYLOAD *payload;

  if (!check_code_allowed_chars(code)) {
    LOG_ERR("invalid characters detected in '%s'", code);
    return NULL;
  }

  path = malloc(PATH_MAX);
  if (!path)
    return NULL;

  err = snprintf(path, PATH_MAX, "%s%s", CODE_FILE_PATH, code);
  if (err >= PATH_MAX) {
    LOG_ERR("274895 path too long");
    maybe_free(path);
    return NULL;
  }
  LOG("475829 using path '%s'", path);

  codefd = open(path, O_RDWR);
  if (codefd < 0) {
    maybe_free(path);
    LOG_ERRNO("failed to open", errno);
    return NULL;
  }
  LOG("opened file '%s',fd=%d", path, codefd);
  maybe_free(path);

  err = fstat(codefd, &stat);
  if (err < 0) {
    LOG_ERRNO("123987 failed to fstat", errno);
    close(codefd);
    return NULL;
  }

  size = stat.st_size;
  LOG("file size is %ld", size);
  if (size < sizeof(struct DUMB_PAYLOAD)) {
    LOG_ERR("375927 file too small");
    errno = EINVAL;
    close(codefd);
    return NULL;
  }

  payload = malloc(size + 1);
  if (payload == NULL) {
    LOG_ERRNO("262638 failed to malloc", errno);
    close(codefd);
    return NULL;
  }

  err = read_all(codefd, payload, size);
  if (err < 0) {
    LOG_ERRNO("129387 failed to read_all()", errno);
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
    LOG_ERR("134923 failed to lseek on fd=%d:%s", codefd, strerror(errno));
  } else {
    err = write_all(codefd, payload, size);
    if (err < 0) {
      LOG_ERR("294875 failed to write to fd=%d:%s", codefd, strerror(errno));
    }
  }

  close(codefd);
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
