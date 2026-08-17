
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "chatgpt.h"
#include "common.h"
#include "dumbserver.h"
#include "requestid.h"

enum MHD_Result dumb_log_upload_handler(struct MHD_Connection *connection,
                                        const RequestState *state) {
  LOG("dumb_log_upload_handler()");
  const char *user =
      MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "user");
  const char *requestid = MHD_lookup_connection_value(
      connection, MHD_GET_ARGUMENT_KIND, "requestid");
  const char *requestsig = MHD_lookup_connection_value(
      connection, MHD_GET_ARGUMENT_KIND, "requestsig");
  if (user == NULL) {
    LOG_ERR("no user");
    return queue_text_response(connection, MHD_HTTP_NOT_FOUND,
                               "text/plain; charset=utf-8", "404 Not Found\n");
  } else if (requestsig == NULL) {
    LOG_ERR("no request signature");
    return queue_text_response(connection, MHD_HTTP_NOT_FOUND,
                               "text/plain; charset=utf-8", "404 Not Found\n");
  } else if (requestid == NULL) {
    LOG_ERR("no requestid");
    return queue_text_response(connection, MHD_HTTP_NOT_FOUND,
                               "text/plain; charset=utf-8", "404 Not Found\n");
  }
  LOG("user='%s',requestid='%s',requestsig='%s'", user, requestid, requestsig);
  if (!check_allowed_chars(user, DUMBOS_USER_ALLOWED_CHARS)) {
    LOG_ERR("invalid characters detected in user '%s'", user);
    return queue_text_response(connection, MHD_HTTP_NOT_FOUND,
                               "text/plain; charset=utf-8", "404 Not Found\n");
  }

  if (state->body_too_large || state->allocation_failed) {
    LOG_ERR("internal error");
    return queue_text_response(connection, MHD_HTTP_NOT_FOUND,
                               "text/plain; charset=utf-8", "404 Not Found\n");
  }

  FILE *user_pubkey_file;
  char *user_pubkey_file_path;
  int err;
  char user_pubkey_hex[ED25519_PUBLIC_KEY_HEX_SIZE];

  // FIXME:check for asprintf fail
  asprintf(&user_pubkey_file_path, "%s%s/%s", USERDATA_PREFIX, user,
           SERVER_USER_PUBKEY_FILE);
  LOG_DEBUG("user pubkey file='%s'", user_pubkey_file_path);
  if (NULL == (user_pubkey_file = fopen(user_pubkey_file_path, "rb"))) {
    LOG_ERRNO("failed opening user pubkey file", errno);
    return queue_text_response(connection, MHD_HTTP_NOT_FOUND,
                               "text/plain; charset=utf-8", "404 Not Found\n");
  }
  errno = 0;
  if (fread(user_pubkey_hex, 1, ED25519_PUBLIC_KEY_HEX_SIZE - 1,
            user_pubkey_file) != ED25519_PUBLIC_KEY_HEX_SIZE - 1) {
    LOG_ERRNO("failed reading from user pubkey file", errno);
    fclose(user_pubkey_file);
    return queue_text_response(connection, MHD_HTTP_NOT_FOUND,
                               "text/plain; charset=utf-8", "404 Not Found\n");
  }
  user_pubkey_hex[ED25519_PUBLIC_KEY_HEX_SIZE - 1] = '\0';

  err = request_id_verify(user, requestid, requestsig, user_pubkey_hex);
  LOG_DEBUG("request_id_verify() returned %d", err);
  if (err != 0) {
    LOG_ERRNO("failed to verify request", err);
    return queue_text_response(connection, MHD_HTTP_NOT_FOUND,
                               "text/plain; charset=utf-8", "404 Not Found\n");
  }

  return queue_text_response(connection, MHD_HTTP_OK,
                             "text/plain; charset=utf-8", "ok\n");
}
