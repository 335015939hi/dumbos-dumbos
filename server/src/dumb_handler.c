
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
  char *response = NULL;
  size_t responselen;
  enum MHD_Result ret;

  if (code == NULL) {
    return queue_text_response(connection, MHD_HTTP_NOT_FOUND,
                               "text/plain; charset=utf-8", "404 Not Found\n");
  }

  response = dp_malloc_check_load(code, &responselen);
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
