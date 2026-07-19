/*
 * Small HTTP server demo using GNU libmicrohttpd.
 *
 * Why use a library?
 *
 * Because HTTP is not "just a socket with strings". That is a trap.
 * A socket gives you bytes. HTTP requires parsing methods, URLs, headers,
 * query strings, chunked uploads, connection lifetime, response formatting,
 * and a thousand tiny ways to be wrong.
 *
 * libmicrohttpd handles the HTTP protocol machinery. We write the actual
 * application logic: routes, responses, POST body handling, etc.
 */

#include <microhttpd.h>

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "chatgpt.h"
#include "common.h"
#include "dumb.h"

/*
 * Maximum POST body size accepted by this demo.
 *
 * Real servers should stream large uploads to a file or processing pipeline.
 * Buffering everything in RAM is simple, but simple is often just "future bug"
 * wearing a tiny hat.
 */
#define MAX_UPLOAD_BYTES (64 * 1024)

/*
 * Maximum static file size for this demo.
 *
 * Again, real servers should stream files. This demo reads the whole file into
 * memory to keep the code small and readable.
 */
#define MAX_STATIC_FILE_BYTES (1024 * 1024)

#define STATIC_PREFIX "/static/"

/*
 * Used by the signal handler.
 *
 * sig_atomic_t is the correct-ish type for communication between a signal
 * handler and normal code. Do not malloc, printf, or do anything cute inside
 * signal handlers unless you enjoy undefined behavior as a lifestyle.
 */
static volatile sig_atomic_t g_should_stop = 0;

static void handle_signal(int signo) {
  (void)signo;
  g_should_stop = 1;
}

/*
 * Return true if `s` starts with `prefix`.
 */
bool starts_with(const char *s, const char *prefix) {
  return strncmp(s, prefix, strlen(prefix)) == 0;
}

/*
 * A tiny MIME type guesser.
 *
 * This is intentionally boring. Real servers have better MIME databases.
 */
static const char *guess_content_type(const char *path) {
  const char *dot = strrchr(path, '.');

  if (dot == NULL) {
    return "application/octet-stream";
  }

  if (strcmp(dot, ".html") == 0) {
    return "text/html; charset=utf-8";
  }

  if (strcmp(dot, ".txt") == 0) {
    return "text/plain; charset=utf-8";
  }

  if (strcmp(dot, ".json") == 0) {
    return "application/json; charset=utf-8";
  }

  if (strcmp(dot, ".css") == 0) {
    return "text/css; charset=utf-8";
  }

  if (strcmp(dot, ".js") == 0) {
    return "application/javascript; charset=utf-8";
  }

  if (strcmp(dot, ".png") == 0) {
    return "image/png";
  }

  if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0) {
    return "image/jpeg";
  }

  return "application/octet-stream";
}

/*
 * Queue a response whose body is a normal C string.
 *
 * libmicrohttpd response ownership rules:
 *
 * - MHD_RESPMEM_MUST_COPY means libmicrohttpd copies the buffer.
 * - Therefore, `body` can point to a stack buffer or string literal.
 *
 * This is the easiest mode for small text responses.
 */
enum MHD_Result queue_text_response(struct MHD_Connection *connection,
                                    unsigned int status_code,
                                    const char *content_type,
                                    const char *body) {
  struct MHD_Response *response;
  enum MHD_Result ret;

  response = MHD_create_response_from_buffer(strlen(body), (void *)body,
                                             MHD_RESPMEM_MUST_COPY);

  if (response == NULL) {
    return MHD_NO;
  }

  /*
   * Add some basic headers.
   *
   * Content-Type tells the client how to interpret the body.
   * X-Server is useless but common in demos, because apparently headers
   * needed vanity plates too.
   */
  MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, content_type);
  MHD_add_response_header(response, "X-Server", "c-http-server-demo");

  ret = MHD_queue_response(connection, status_code, response);

  /*
   * Destroy our reference. If the response was queued successfully,
   * libmicrohttpd keeps its own internal reference until it is sent.
   */
  MHD_destroy_response(response);

  return ret;
}

/*
 * Queue a response from arbitrary bytes.
 *
 * Useful for echoing POST bodies or serving files.
 */
enum MHD_Result queue_bytes_response(struct MHD_Connection *connection,
                                     unsigned int status_code,
                                     const char *content_type, const void *data,
                                     size_t size) {
  struct MHD_Response *response;
  enum MHD_Result ret;

  response = MHD_create_response_from_buffer(size, (void *)data,
                                             MHD_RESPMEM_MUST_COPY);

  if (response == NULL) {
    return MHD_NO;
  }

  MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, content_type);
  MHD_add_response_header(response, "X-Server", "c-http-server-demo");

  ret = MHD_queue_response(connection, status_code, response);

  MHD_destroy_response(response);

  return ret;
}

/*
 * Queue a file response.
 *
 * This variant takes ownership of `data` using MHD_RESPMEM_MUST_FREE.
 * That means libmicrohttpd will free the buffer when done with the response.
 */
enum MHD_Result queue_owned_file_response(struct MHD_Connection *connection,
                                          unsigned int status_code,
                                          const char *content_type, void *data,
                                          size_t size) {
  struct MHD_Response *response;
  enum MHD_Result ret;

  response = MHD_create_response_from_buffer(size, data, MHD_RESPMEM_MUST_FREE);

  if (response == NULL) {
    free(data);
    return MHD_NO;
  }

  MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, content_type);
  MHD_add_response_header(response, "X-Server", "c-http-server-demo");

  ret = MHD_queue_response(connection, status_code, response);

  MHD_destroy_response(response);

  return ret;
}

/*
 * Return a 405 Method Not Allowed response.
 *
 * 405 should include an Allow header telling the client which methods are
 * supported. Many toy servers forget this because apparently standards are
 * decorative now.
 */
enum MHD_Result queue_method_not_allowed(struct MHD_Connection *connection) {
  const char *body = "405 Method Not Allowed\n";
  struct MHD_Response *response;
  enum MHD_Result ret;

  response = MHD_create_response_from_buffer(strlen(body), (void *)body,
                                             MHD_RESPMEM_MUST_COPY);

  if (response == NULL) {
    return MHD_NO;
  }

  MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE,
                          "text/plain; charset=utf-8");
  MHD_add_response_header(response, "Allow", "GET, POST");

  ret = MHD_queue_response(connection, MHD_HTTP_METHOD_NOT_ALLOWED, response);

  MHD_destroy_response(response);

  return ret;
}

/*
 * Append POST upload data to the RequestState buffer.
 *
 * libmicrohttpd gives us chunks of upload data. We collect them into one buffer
 * for this demo.
 */
bool append_upload_data(RequestState *state, const char *upload_data,
                        size_t upload_data_size) {
  char *new_body;

  if (upload_data_size == 0) {
    return true;
  }

  if (state->body_len + upload_data_size > MAX_UPLOAD_BYTES) {
    state->body_too_large = true;
    return false;
  }

  /*
   * +1 so we can NUL-terminate the buffer.
   *
   * The body is still tracked by length, so it can technically contain NUL
   * bytes. The terminator is just convenient for text cases.
   */
  new_body = realloc(state->body, state->body_len + upload_data_size + 1);

  if (new_body == NULL) {
    state->allocation_failed = true;
    return false;
  }

  memcpy(new_body + state->body_len, upload_data, upload_data_size);

  state->body = new_body;
  state->body_len += upload_data_size;
  state->body[state->body_len] = '\0';

  return true;
}

/*
 * Read a whole file into memory.
 *
 * This is intentionally simple. For production, use streaming APIs instead
 * of reading entire files into RAM.
 */
bool read_file_into_memory(const char *path, char **out_data,
                           size_t *out_size) {
  FILE *fp;
  long size;
  char *buffer;
  size_t bytes_read;

  *out_data = NULL;
  *out_size = 0;

  fp = fopen(path, "rb");

  if (fp == NULL) {
    return false;
  }

  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return false;
  }

  size = ftell(fp);

  if (size < 0 || size > MAX_STATIC_FILE_BYTES) {
    fclose(fp);
    return false;
  }

  if (fseek(fp, 0, SEEK_SET) != 0) {
    fclose(fp);
    return false;
  }

  /*
   * malloc(0) is implementation-defined-ish in practical behavior.
   * Allocate at least 1 byte so the pointer is sane even for empty files.
   */
  buffer = malloc((size_t)size == 0 ? 1 : (size_t)size);

  if (buffer == NULL) {
    fclose(fp);
    return false;
  }

  bytes_read = fread(buffer, 1, (size_t)size, fp);

  if (bytes_read != (size_t)size) {
    free(buffer);
    fclose(fp);
    return false;
  }

  fclose(fp);

  *out_data = buffer;
  *out_size = (size_t)size;

  return true;
}

/*
 * Serve a file under ./public using URLs like:
 *
 *     /static/demo.txt
 *
 * This performs minimal path traversal protection.
 *
 * It rejects:
 *
 * - ".."
 * - backslashes
 * - empty path
 * - paths too large for our fixed buffer
 *
 * This is okay for a demo. For serious file serving, use openat(), directory
 * file descriptors, stricter normalization, and probably let Caddy/nginx do it
 * because you presumably have other things to do before the heat death of the
 * universe.
 */
enum MHD_Result serve_static_file(struct MHD_Connection *connection,
                                  const char *url) {
  const char *relative_path;
  char filesystem_path[512];
  char *file_data;
  size_t file_size;
  int n;

  relative_path = url + strlen(STATIC_PREFIX);

  if (relative_path[0] == '\0') {
    return queue_text_response(connection, MHD_HTTP_NOT_FOUND,
                               "text/plain; charset=utf-8",
                               "404 Static file not found\n");
  }

  if (strstr(relative_path, "..") != NULL ||
      strchr(relative_path, '\\') != NULL || relative_path[0] == '/') {
    return queue_text_response(connection, MHD_HTTP_BAD_REQUEST,
                               "text/plain; charset=utf-8",
                               "400 Bad static file path\n");
  }

  n = snprintf(filesystem_path, sizeof(filesystem_path), "public/%s",
               relative_path);

  if (n < 0 || (size_t)n >= sizeof(filesystem_path)) {
    return queue_text_response(connection, MHD_HTTP_URI_TOO_LONG,
                               "text/plain; charset=utf-8",
                               "414 URI Too Long\n");
  }

  if (!read_file_into_memory(filesystem_path, &file_data, &file_size)) {
    return queue_text_response(connection, MHD_HTTP_NOT_FOUND,
                               "text/plain; charset=utf-8",
                               "404 Static file not found\n");
  }

  return queue_owned_file_response(connection, MHD_HTTP_OK,
                                   guess_content_type(filesystem_path),
                                   file_data, file_size);
}

/*
 * Handle GET routes.
 *
 * libmicrohttpd gives `url` without the query string.
 * So for:
 *
 *     /hello?name=Tony
 *
 * `url` is:
 *
 *     /hello
 *
 * Query values are fetched with MHD_lookup_connection_value().
 */
enum MHD_Result handle_get(struct MHD_Connection *connection, const char *url) {
  if (strcmp(url, "/dumb") == 0) {
    return dumb_handler(connection);
  }

  return queue_text_response(connection, MHD_HTTP_NOT_FOUND,
                             "text/plain; charset=utf-8", "404 Not Found\n");
}

/*
 * Handle POST routes.
 *
 * By the time this function is called, upload data has already been collected
 * into RequestState by the main request handler.
 */
enum MHD_Result handle_post(struct MHD_Connection *connection, const char *url,
                            const RequestState *state) {
  if (state->allocation_failed) {
    return queue_text_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR,
                               "text/plain; charset=utf-8",
                               "500 Failed to allocate memory\n");
  }

  if (state->body_too_large) {
    return queue_text_response(connection, MHD_HTTP_CONTENT_TOO_LARGE,
                               "text/plain; charset=utf-8",
                               "413 Request body too large\n");
  }

  if (strcmp(url, "/echo") == 0) {
    /*
     * Echo the request body back to the client.
     *
     * This uses length, not strlen(), so binary request bodies are safe.
     */
    return queue_bytes_response(
        connection, MHD_HTTP_OK, "text/plain; charset=utf-8",
        state->body != NULL ? state->body : "", state->body_len);
  }

  return queue_text_response(connection, MHD_HTTP_NOT_FOUND,
                             "text/plain; charset=utf-8",
                             "404 POST route not found\n");
}

/*
 * Main request handler.
 *
 * libmicrohttpd calls this function for incoming requests.
 *
 * Important parameters:
 *
 * - connection:
 *     The HTTP connection object. You use it to read request metadata and queue
 *     a response.
 *
 * - url:
 *     The path part of the URL, without the query string.
 *
 * - method:
 *     "GET", "POST", etc.
 *
 * - upload_data / upload_data_size:
 *     Request body chunks for POST/PUT-style requests.
 *
 * - con_cls:
 *     A void** where you can store per-request state.
 */
enum MHD_Result handle_request(void *cls, struct MHD_Connection *connection,
                               const char *url, const char *method,
                               const char *version, const char *upload_data,
                               size_t *upload_data_size, void **con_cls) {
  RequestState *state;

  (void)cls;
  (void)version;

  /*
   * First call for this request: allocate request state.
   *
   * For GET this may seem unnecessary, but it gives us one uniform code path.
   */
  if (*con_cls == NULL) {
    state = calloc(1, sizeof(*state));

    if (state == NULL) {
      return MHD_NO;
    }

    *con_cls = state;

    /*
     * Returning MHD_YES tells libmicrohttpd:
     * "State is ready, call me again for the same request."
     */
    return MHD_YES;
  }

  state = *con_cls;

  if (strcmp(method, MHD_HTTP_METHOD_GET) == 0) {
    return handle_get(connection, url);
  }

  return queue_method_not_allowed(connection);
}

/*
 * Called when libmicrohttpd is done with a request.
 *
 * This is where we free per-request state stored in con_cls.
 */
void request_completed(void *cls, struct MHD_Connection *connection,
                       void **con_cls, enum MHD_RequestTerminationCode toe) {
  RequestState *state;

  (void)cls;
  (void)connection;
  (void)toe;

  state = *con_cls;

  if (state != NULL) {
    free(state->body);
    free(state);
    *con_cls = NULL;
  }
}

/*
 * Parse a TCP port from argv.
 */
bool parse_port(const char *s, unsigned int *out_port) {
  char *end;
  unsigned long value;

  errno = 0;
  value = strtoul(s, &end, 10);

  if (errno != 0 || end == s || *end != '\0') {
    return false;
  }

  if (value == 0 || value > 65535) {
    return false;
  }

  *out_port = (unsigned int)value;
  return true;
}

static int read_keyfile(char key[ED25519_PRIVATE_KEY_HEX_SIZE],
                        const char *path) {
  LOG_VERBOSE("reading keyfile %s", path);
  FILE *f = fopen(path, "r");
  if (f == NULL) {
    LOG_ERRNO("failed to open file for reading", errno);
    return errno;
  }
  if (ED25519_PRIVATE_KEY_HEX_SIZE - 1 !=
      fread(key, 1, ED25519_PRIVATE_KEY_HEX_SIZE - 1, f)) {
    LOG_ERRNO("failed reading key file", errno);
    if (errno == 0)
      errno = ENOMSG;
    fclose(f);
    return errno;
  }
  fclose(f);
  key[ED25519_PRIVATE_KEY_HEX_SIZE - 1] = '\0';
  return 0;
}

int main(int argc, char **argv) {
  unsigned int port;
  int err;
  struct MHD_Daemon *daemon;

  if (argc != 3) {
    fprintf(stderr, "usage: %s <port> <private_key_path>\n", argv[0]);
    return EXIT_FAILURE;
  }

  if (!parse_port(argv[1], &port)) {
    fprintf(stderr, "invalid port: %s\n", argv[1]);
    return EXIT_FAILURE;
  }

  if ((err = read_keyfile(ed25519_private_key, argv[2])) != 0) {
    LOG_FATAL("failed to read key file. exiting");
    return err;
  }

  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);

  /*
   * Start the HTTP server.
   *
   * MHD_USE_INTERNAL_POLLING_THREAD:
   *   libmicrohttpd runs its own thread for socket polling.
   *
   * MHD_USE_ERROR_LOG:
   *   print internal library errors to stderr.
   *
   * MHD_OPTION_NOTIFY_COMPLETED:
   *   register a cleanup callback for per-request state.
   */
  daemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_ERROR_LOG,
                            port, NULL, NULL, &handle_request, NULL,
                            MHD_OPTION_NOTIFY_COMPLETED, request_completed,
                            NULL, MHD_OPTION_END);

  if (daemon == NULL) {
    fprintf(stderr, "failed to start server on port %u\n", port);
    return EXIT_FAILURE;
  }

  printf("server listening on port %u\n", port);
  printf("press Ctrl+C to stop\n");

  /*
   * Keep the main thread alive.
   *
   * The actual HTTP work happens in libmicrohttpd's internal polling thread.
   */
  while (!g_should_stop) {
    sleep(500);
  }

  printf("\nstopping server\n");

  MHD_stop_daemon(daemon);

  return EXIT_SUCCESS;
}
