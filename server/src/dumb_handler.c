
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <microhttpd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "chatgpt.h"
#include "common.h"
#include "dumb.h"
#include "dumbserver.h"
#include "requestid.h"

#include "../../keys/key_private.h"

// TODO: better http rreturn codes for different errors

char *malloc_buf_to_hex(const void *buf, size_t size) {
  const char *const hex = "0123456789ABCDEF";
  char *output;
  uint8_t top;
  uint8_t bottom;

  output = malloc(2 * size + 1);
  if (output == NULL) {
    return NULL;
  }

  for (size_t i = 0; i < size; i++) {
    unsigned char c = ((char *)buf)[i];
    top = c / 16;
    bottom = c % 16;
    output[2 * i] = hex[top];
    output[2 * i + 1] = hex[bottom];
  }

  output[2 * size] = '\0';
  return output;
}
static int hexchartoval(const char c) {
  if ('0' <= c && c <= '9') {
    return c - '0';
  } else if ('a' <= c && c <= 'f') {
    return c - 'a' + 10;
  } else if ('A' <= c && c <= 'F') {
    return c - 'A' + 10;
  } else {
    return -1;
  }
}
void *malloc_hex_to_buf(const char *hex) {
  int hexlen = strlen(hex);
  char *output;
  if (hexlen % 2 != 0) {
    // not even number of characters
    errno = EINVAL;
    return NULL;
  }
  output = malloc(hexlen / 2);
  if (output == NULL) {
    return output;
  }

  for (int i = 0; i < hexlen / 2; i++) {
    char top = hexchartoval(hex[2 * i]);
    char bottom = hexchartoval(hex[2 * i + 1]);
    if (top < 0 || bottom < 0) {
      maybe_free(output);
      return NULL;
    }
    output[i] = (16 * top) + bottom;
  }

  return output;
}

enum MHD_Result dumb_handler(struct MHD_Connection *connection) {
  const char *code =
      MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "code");
  const char *user =
      MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "user");
  const char *requestid = MHD_lookup_connection_value(
      connection, MHD_GET_ARGUMENT_KIND, "requestid");
  const char *requestsig = MHD_lookup_connection_value(
      connection, MHD_GET_ARGUMENT_KIND, "requestsig");

  char *response = NULL;
  size_t responselen;
  enum MHD_Result ret;
  int err;

  if (code == NULL) {
    LOG_ERR("no code");
    return queue_text_response(connection, MHD_HTTP_NOT_FOUND,
                               "text/plain; charset=utf-8", "404 Not Found\n");
  }
  if (user == NULL) {
    LOG_ERR("no user");
    return queue_text_response(connection, MHD_HTTP_NOT_FOUND,
                               "text/plain; charset=utf-8", "404 Not Found\n");
  }
  if (!strcmp(user, DUMBOS_DEFAULT_USER)) {
    LOG_WARN("default user %s detected", user);
    // always allow default user.

  } else {
    if (requestid == NULL) {
      LOG_ERR("no requestid");
      return queue_text_response(connection, MHD_HTTP_NOT_FOUND,
                                 "text/plain; charset=utf-8",
                                 "404 Not Found\n");
    }
    if (requestsig == NULL) {
      LOG_ERR("no requestsig");
      return queue_text_response(connection, MHD_HTTP_NOT_FOUND,
                                 "text/plain; charset=utf-8",
                                 "404 Not Found\n");
    }
    LOG("code='%s' user='%s' requestid='%s' requestsig='%s'", code, user,
        requestid, requestsig);
    if (!check_allowed_chars(user, DUMBOS_USER_ALLOWED_CHARS)) {
      LOG_ERR("invalid characters detected in user '%s'", user);
      return queue_text_response(connection, MHD_HTTP_NOT_FOUND,
                                 "text/plain; charset=utf-8",
                                 "404 Not Found\n");
    }
    char *userpath;
    char *user_pubkey_path;
    char *request_data_path;
    char user_pubkey[ED25519_PUBLIC_KEY_HEX_SIZE];
    user_pubkey[ED25519_PUBLIC_KEY_HEX_SIZE - 1] = '\0';
    // FIXME: check for asprintf fail
    asprintf(&userpath, "%s%s/", USERDATA_PREFIX, user);
    asprintf(&user_pubkey_path, "%s%s", userpath, SERVER_USER_PUBKEY_FILE);
    asprintf(&request_data_path, "%s/request-%s", userpath, requestid);
    free(userpath);

    FILE *user_pubkey_file = fopen(user_pubkey_path, "rb");
    free(user_pubkey_path);
    if (user_pubkey_file == NULL) {
      free(request_data_path);
      LOG_ERRNO("failed to open pubkey file", errno);
      return queue_text_response(connection, MHD_HTTP_NOT_FOUND,
                                 "text/plain; charset=utf-8",
                                 "404 Not Found\n");
    }
    errno = 0;
    if (fread(user_pubkey, 1, ED25519_PUBLIC_KEY_HEX_SIZE - 1,
              user_pubkey_file) != ED25519_PUBLIC_KEY_HEX_SIZE - 1) {
      LOG_ERRNO("failed reading user pubkey", errno);
      free(request_data_path);
      fclose(user_pubkey_file);
      return queue_text_response(connection, MHD_HTTP_NOT_FOUND,
                                 "text/plain; charset=utf-8",
                                 "404 Not Found\n");
    }
    fclose(user_pubkey_file);

    free(request_data_path);
    err = request_id_verify(user, requestid, requestsig, user_pubkey);
    LOG_DEBUG("request_id_verify return %d", err);
    if (err != 0) {
      LOG_ERRNO("request_id_verify failed", err);
      LOG_ERR("Invalid request id");
      return queue_text_response(connection, MHD_HTTP_NOT_FOUND,
                                 "text/plain; charset=utf-8",
                                 "404 Not Found\n");
    }
  }

  response = dp_malloc_check_load(code, &responselen, _ed25519_private_key_hex);
  if (response == NULL) {
    return queue_text_response(connection, MHD_HTTP_NOT_FOUND,
                               "text/plain; charset=utf-8", "404 Not Found\n");
  }

  ret =
      queue_bytes_response(connection, MHD_HTTP_OK, "text/plain; charset=utf-8",
                           response, responselen);
  maybe_free(response);
  return ret;
}
