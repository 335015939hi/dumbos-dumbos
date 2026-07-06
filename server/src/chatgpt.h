#ifndef _CHATGPT_H
#define _CHATGPT_H

// enum MHD_Result;
struct MHD_Connection;
enum MHD_Result dumb_handler(struct MHD_Connection *connection);

// function signatures of stuff written by ChatGPT

#include <microhttpd.h>

typedef struct RequestState {
  char *body;
  size_t body_len;
  bool body_too_large;
  bool allocation_failed;
} RequestState;

/*
 * Return true if `s` starts with `prefix`.
 */
bool starts_with(const char *s, const char *prefix);

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
                                    const char *content_type, const char *body);

/*
 * Queue a response from arbitrary bytes.
 *
 * Useful for echoing POST bodies or serving files.
 */
enum MHD_Result queue_bytes_response(struct MHD_Connection *connection,
                                     unsigned int status_code,
                                     const char *content_type, const void *data,
                                     size_t size);

enum MHD_Result queue_owned_file_response(struct MHD_Connection *connection,
                                          unsigned int status_code,
                                          const char *content_type, void *data,
                                          size_t size);

/*
 * Return a 405 Method Not Allowed response.
 *
 * 405 should include an Allow header telling the client which methods are
 * supported. Many toy servers forget this because apparently standards are
 * decorative now.
 */
enum MHD_Result queue_method_not_allowed(struct MHD_Connection *connection);

/*
 * Append POST upload data to the RequestState buffer.
 *
 * libmicrohttpd gives us chunks of upload data. We collect them into one buffer
 * for this demo.
 */
bool append_upload_data(RequestState *state, const char *upload_data,
                        size_t upload_data_size);

/*
 * Read a whole file into memory.
 *
 * This is intentionally simple. For production, use streaming APIs instead
 * of reading entire files into RAM.
 */
bool read_file_into_memory(const char *path, char **out_data, size_t *out_size);

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
                                  const char *url);

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
enum MHD_Result handle_get(struct MHD_Connection *connection, const char *url);

/*
 * Handle POST routes.
 *
 * By the time this function is called, upload data has already been collected
 * into RequestState by the main request handler.
 */
enum MHD_Result handle_post(struct MHD_Connection *connection, const char *url,
                            const RequestState *state);

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
                               size_t *upload_data_size, void **con_cls);

/*
 * Called when libmicrohttpd is done with a request.
 *
 * This is where we free per-request state stored in con_cls.
 */
void request_completed(void *cls, struct MHD_Connection *connection,
                       void **con_cls, enum MHD_RequestTerminationCode toe);

/*
 * Parse a TCP port from argv.
 */
bool parse_port(const char *s, unsigned int *out_port);

#endif
