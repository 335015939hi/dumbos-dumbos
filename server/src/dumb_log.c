
#include <stdio.h>

#include "chatgpt.h"
#include "common.h"
#include "dumbserver.h"

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
  if (state->body_too_large || state->allocation_failed) {
    LOG_ERR("internal error");
    return queue_text_response(connection, MHD_HTTP_NOT_FOUND,
                               "text/plain; charset=utf-8", "404 Not Found\n");
  }
  return queue_text_response(connection, MHD_HTTP_OK,
                             "text/plain; charset=utf-8", "ok\n");
}
