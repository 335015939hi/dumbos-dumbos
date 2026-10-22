#ifndef _DUMB_CURL_H
#define _DUMB_CURL_H

#include <stddef.h>

void *download_url(const char *URL, size_t *size, int sockfd);
// POST to <url> with <data> buffer of <data_len> size. if <http_response> is
// non-NULL, the http response code will be put there. on failure, will return
// non-0 and set errno. returns 0 on success.
int post_buffer(const char *url, const void *data, size_t data_len,
                int *http_response);
#endif
