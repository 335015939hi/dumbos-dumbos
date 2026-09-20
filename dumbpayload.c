
#include <arpa/inet.h>
#include <asm-generic/errno.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/limits.h>
#include <math.h>
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
  // force NULL-terminate
  signature[ED25519_SIGNATURE_HEX_SIZE - 1] = '\0';
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

struct DUMB_PAYLOAD *dp_create_new() {
  struct DUMB_PAYLOAD *new = malloc(sizeof(struct DUMB_PAYLOAD));
  if (new == NULL) {
    return NULL;
  }
  memset(new, 0, sizeof(struct DUMB_PAYLOAD));
  return new;
}

// 0 on success, errno on fail
int dp_set_command(struct DUMB_PAYLOAD *payload, const char *command) {
  if (strnlen(command, COMMAND_SIZE) >= COMMAND_SIZE) {
    errno = EOVERFLOW;
    return EOVERFLOW;
  }
  strncpy(payload->command, command, COMMAND_SIZE);
  payload->command[COMMAND_SIZE - 1] = '\0';
  return 0;
}
const char *dp_get_command(const struct DUMB_PAYLOAD *payload) {
  const char *str = payload->command;
  if (strnlen(str, COMMAND_SIZE) >= COMMAND_SIZE) {
    errno = EOVERFLOW;
    return NULL;
  }
  return str;
}

int dp_set_expire_str(struct DUMB_PAYLOAD *payload, const char *expire_string) {
  if (strnlen(expire_string, EXPIRE_SIZE) >= EXPIRE_SIZE) {
    errno = EOVERFLOW;
    return EOVERFLOW;
  }
  strncpy(payload->expire, expire_string, EXPIRE_SIZE);
  payload->expire[EXPIRE_SIZE - 1] = '\0';
  return 0;
}
const char *dp_get_expire_str(const struct DUMB_PAYLOAD *payload) {
  const char *str = payload->expire;
  if (strnlen(str, EXPIRE_SIZE) >= EXPIRE_SIZE) {
    errno = EOVERFLOW;
    return NULL;
  }
  return str;
}

int dp_set_signature(struct DUMB_PAYLOAD *payload, const char *signature) {
  if (strnlen(signature, ED25519_SIGNATURE_HEX_SIZE) !=
      ED25519_SIGNATURE_HEX_SIZE - 1) {
    errno = EINVAL;
    return EINVAL;
  }
  memcpy(payload->signature, signature, ED25519_SIGNATURE_HEX_SIZE);
  return 0;
}
const char *dp_get_signature(const struct DUMB_PAYLOAD *payload) {
  const char *signature = payload->signature;
  // aa valid signature should be exactly ED25519_SIGNATURE_HEX_SIZE-1
  // characters long, excluding NULL terminater
  if (strnlen(signature, ED25519_SIGNATURE_HEX_SIZE) !=
      ED25519_SIGNATURE_HEX_SIZE - 1) {
    return NULL;
  }
  return signature;
}

int dp_set_data_size(struct DUMB_PAYLOAD *payload, size_t size) {
  if (size > DUMB_PAYLOAD_DATA_MAX_SIZE) {
    errno = EOVERFLOW;
    return EOVERFLOW;
  }
  payload->data_size = htonl(size);
  return 0;
}

// return <0 on error and sets errno
ssize_t dp_get_data_size(const struct DUMB_PAYLOAD *payload) {
  size_t size = ntohl(payload->data_size);
  if (size > DUMB_PAYLOAD_DATA_MAX_SIZE) {
    errno = EOVERFLOW;
    return -1;
  }
  return size;
}

// NULL on error and sets errno
// payload will be realloced
// on any error assume payload is corrupted; free() it and stop using
struct DUMB_PAYLOAD *dp_set_data(struct DUMB_PAYLOAD *payload, const void *data,
                                 size_t size) {
  if (dp_set_data_size(payload, size) != 0) {
    return NULL;
  }
  struct DUMB_PAYLOAD *new =
      realloc(payload, sizeof(struct DUMB_PAYLOAD) + size);
  if (new == NULL)
    return NULL;
  memcpy(new->payload, data, size);
  return new;
}
// returns a pointer to data and write its size to *size_dest
// return NULL and sets errno on error
// if there is no data, errno=0 and return NULL
const void *dp_get_data(const struct DUMB_PAYLOAD *payload, size_t *size_dest) {
  ssize_t size = dp_get_data_size(payload);
  if (size < 0) {
    // dp_get_data_size() should set errno
    return NULL;
  }
  if (size == 0) {
    *size_dest = 0;
    errno = 0;
    return NULL;
  }
  *size_dest = size;
  return payload->payload;
}

void *dp_malloc_get_data(const struct DUMB_PAYLOAD *payload,
                         size_t *size_dest) {
  size_t size;
  const void *data_src;
  void *data_dest;
  data_src = dp_get_data(payload, &size);
  if (data_src == NULL) {
    if (errno == 0) {
      *size_dest = 0;
    }
    return NULL;
  }
  data_dest = malloc(size);
  if (data_dest == NULL) {
    return NULL;
  }
  memcpy(data_dest, data_src, size);
  *size_dest = size;
  return data_dest;
}

// validates the size of payload
// return true if sizes correct, false if error detected
// detected_full_size is the full size detected when loading from file or
// downloading or whatever, including headers and data
bool dp_validate_size(const struct DUMB_PAYLOAD *payload,
                      size_t detected_full_size) {
  if (detected_full_size < sizeof(struct DUMB_PAYLOAD)) {
    return false;
  }
  ssize_t data_size = dp_get_data_size(payload);
  if (data_size < 0) {
    return false;
  }
  if (data_size + sizeof(struct DUMB_PAYLOAD) != detected_full_size) {
    return false;
  }
  return true;
}

// warning: this function trusts the data_size field. make sure the source of
// the payload is trustworthy!! write <payload> to file at <pathname>. returns 0
// on success, not 0 and sets errno on fail
int dp_write_to_file(const struct DUMB_PAYLOAD *payload, const char *pathname) {
  int fd = open(pathname, O_WRONLY | O_CREAT | O_TRUNC, 00600);
  if (fd < 0) {
    return -1;
  }
  ssize_t size = dp_get_data_size(payload);
  if (size < 0) {
    close(fd);
    return -1;
  }
  size += sizeof(struct DUMB_PAYLOAD);
  if (write_all(fd, payload, size) < 0) {
    close(fd);
    return -1;
  }
  close(fd);
  return 0;
}
