
#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <time.h>

#include "common.h"
#include "ed25519.h"
#include "requestid.h"

#define REQUEST_ID_MAGIC "af123oidoo"

#define DUMBOS_USER_DATA_PATH "/dev/dumbos-userdata"

#define REQUEST_ID_LEN 64
const char *request_id_chars = REQUESTID_ALLOWED_CHARS;

int request_id_sign(char *output, const char *user, const char *request_id,
                    const char *priv_key_hex) {
  size_t data_size =
      strlen(user) + strlen(request_id) + strlen(REQUEST_ID_MAGIC) + 1;
  char *data;
  int err;
  err = asprintf(&data, "%s%s%s", user, REQUEST_ID_MAGIC, request_id);
  if (err < 0) {
    err = errno;
    LOG_ERRNO("asprintf failed", errno);
    return err;
  }
  err = ed25519_sign_hex(priv_key_hex, data, data_size, output);
  free(data);
  if (err != 0) {
    LOG_ERRNO("request_id_sign(): ed25519_sign_hex() failed", errno);
    return errno;
  }
  return 0;
}

char *request_id_generate(void) {
  static char requestid[REQUEST_ID_LEN + 1];
  if (getrandom(requestid, REQUEST_ID_LEN, 0) != REQUEST_ID_LEN) {
    return NULL;
  }
  for (int i = 0; i < REQUEST_ID_LEN; i++) {
    requestid[i] = request_id_chars[requestid[i] % strlen(request_id_chars)];
  }
  return requestid;
}

int request_id_verify(const char *user, const char *request_id,
                      const char *signature, const char *pub_key_hex) {
  size_t data_size =
      strlen(user) + strlen(request_id) + strlen(REQUEST_ID_MAGIC) + 1;
  char *data;
  int err;
  err = asprintf(&data, "%s%s%s", user, REQUEST_ID_MAGIC, request_id);
  if (err < 0) {
    err = errno;
    LOG_ERRNO("asprintf failed", errno);
    return err;
  }
  err = ed25519_verify_hex(pub_key_hex, data, data_size, signature);
  free(data);
  if (err != 0) {
    LOG_ERRNO("request_id_verify():ed25519_verify_hex failed", errno);
    return errno;
  } else {
    return 0;
  }
}

struct DUMBOS_USER_DATA *dumbos_alloc_get_user(void) {
  int err;
  struct DUMBOS_USER_DATA *ret = malloc(sizeof(struct DUMBOS_USER_DATA));
  if (ret == NULL) {
    LOG_ERRNO("dumbos_alloc_get_user:failed to allocate memory", errno);
    return NULL;
  }
  FILE *user_data = fopen(DUMBOS_USER_DATA_PATH, "rb");
  if (user_data == NULL) {
    if (errno == ENOENT) {
      LOG("dumbos_alloc_get_user: no user set. using default");
      memcpy(ret->magic, DUMBOS_USER_DATA_MAGIC, DUMBOS_USER_DATA_MAGIC_SIZE);
      snprintf(ret->username, DUMBOS_USERNAME_MAXLEN, "%s",
               DUMBOS_DEFAULT_USER);
      snprintf(ret->priv_key_hex, ED25519_PRIVATE_KEY_HEX_SIZE, "%s", "TODO");
    } else {
      LOG_ERRNO("dumbos_alloc_get_user: failed to open file for reading",
                errno);
      free(ret);
      return NULL;
    }
  } else {
    errno = 0;
    err = fread(ret, 1, sizeof(struct DUMBOS_USER_DATA), user_data);
    if (err != sizeof(struct DUMBOS_USER_DATA)) {
      err = errno;
      if (ferror(user_data)) {
        LOG_ERR("dumbos_alloc_get_user: ferror");
        err = EIO;
      }
      if (feof(user_data)) {
        LOG_ERR("dumbos_alloc_get_user: feof");
        err = EIO;
      }
      LOG_ERRNO("dumbos_alloc_get_user: failed to fread", err);
      fclose(user_data);
      free(ret);
      errno = err;
      return NULL;
    }
  }
  fclose(user_data);
  if (memcmp(ret->magic, DUMBOS_USER_DATA_MAGIC, DUMBOS_USER_DATA_MAGIC_SIZE) !=
      0) {
    free(ret);
    LOG_ERR("dumbos_alloc_get_user:invalid magic detected");
    errno = EINVAL;
    return NULL;
  }
  ret->null_byte = '\0';
  ret->priv_key_hex[ED25519_PRIVATE_KEY_HEX_SIZE - 1] = '\0';
  return ret;
}

struct DUMBOS_USER_DATA *dumbos_alloc_new_user(const char *user,
                                               const char *priv_key_hex) {
  LOG_DEBUG("dumbos_alloc_new_user()");
  if (strlen(user) > DUMBOS_USERNAME_MAXLEN - 1) {
    LOG_ERR("username too long");
    errno = EINVAL;
    return NULL;
  }
  if (strlen(priv_key_hex) != ED25519_PRIVATE_KEY_HEX_SIZE - 1) {
    LOG_ERR("invalid private key length detected");
    errno = EINVAL;
    return NULL;
  }
  struct DUMBOS_USER_DATA *new_user = malloc(sizeof *new_user);
  if (new_user == NULL) {
    LOG_ERRNO("failed to allocate memory", errno);
    return NULL;
  }
  snprintf(new_user->username, DUMBOS_USERNAME_MAXLEN, "%s", user);
  snprintf(new_user->priv_key_hex, ED25519_PRIVATE_KEY_HEX_SIZE, "%s",
           priv_key_hex);
  memcpy(new_user->magic, DUMBOS_USER_DATA_MAGIC, DUMBOS_USER_DATA_MAGIC_SIZE);
  LOG_DEBUG("dumbos_alloc_new_user(): new user '%s'", user);
  return new_user;
}
