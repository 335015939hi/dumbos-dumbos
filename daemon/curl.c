// chatgpt code

/*
 * download_url.c
 *
 * Android-daemon-compatible HTTP/HTTPS downloader using libcurl.
 *
 * Function:
 *
 *     void *download_url(const char *URL,
 *                        size_t *size,
 *                        const custom_certificate *cert);
 *
 * Returns:
 *     malloc()'d buffer containing downloaded bytes.
 *
 * On success:
 *     - returns non-NULL
 *     - *size contains byte count
 *     - caller must free() the returned buffer
 *
 * On failure:
 *     - returns NULL
 *     - sets errno
 *     - sets *size to 0 when size != NULL
 *
 * Build on normal Linux:
 *
 *     cc -Wall -Wextra -O2 download_url.c -lcurl -lpthread
 *
 * Android/AOSP note:
 *     You need libcurl available to your daemon. Android does not magically
 *     provide every library just because humans enjoy suffering.
 */

#define _POSIX_C_SOURCE 1

#include <curl/curl.h>

#include <errno.h>
#include <linux/limits.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../common.h"

/*
 * Optional HTTPS trust material.
 *
 * This represents a custom CA certificate, not a client certificate.
 *
 * Use either:
 *
 *     .ca_pem     = pointer to PEM CA certificate bytes
 *     .ca_pem_len = byte length of PEM data
 *
 * or:
 *
 *     .ca_file    = path to PEM CA certificate file
 *
 * If both are provided, ca_pem wins.
 */
typedef struct custom_certificate {
  const void *ca_pem;
  size_t ca_pem_len;

  const char *ca_file;
} custom_certificate;

typedef struct download_buffer {
  unsigned char *data;
  size_t size;
  int err;
} download_buffer;

static pthread_once_t curl_once = PTHREAD_ONCE_INIT;
static int curl_global_errno = 0;

static void curl_global_init_once(void) {
  CURLcode rc = curl_global_init(CURL_GLOBAL_DEFAULT);

  if (rc != CURLE_OK) {
    curl_global_errno = ENOMEM;
  }
}

static int starts_with_http_scheme(const char *s) {
  if (s == NULL) {
    return 0;
  }

  return strncmp(s, "http://", 7) == 0 || strncmp(s, "https://", 8) == 0;
}

static char *make_effective_url(const char *URL) {
  static const char prefix[] = "http://";

  size_t url_len;
  size_t prefix_len;
  char *ret;

  if (URL == NULL || URL[0] == '\0') {
    errno = EINVAL;
    return NULL;
  }

  if (starts_with_http_scheme(URL)) {
    ret = strdup(URL);
    if (ret == NULL) {
      errno = ENOMEM;
    }
    return ret;
  }

  /*
   * If it contains :// but is not http:// or https://, reject it.
   * Example: ftp://example.com
   *
   * If it is host:8080/path, that does NOT contain ://,
   * so we treat it as http://host:8080/path.
   */
  if (strstr(URL, "://") != NULL) {
    errno = EPROTONOSUPPORT;
    return NULL;
  }

  url_len = strlen(URL);
  prefix_len = sizeof(prefix) - 1;

  if (url_len > SIZE_MAX - prefix_len - 1) {
    errno = EOVERFLOW;
    return NULL;
  }

  ret = malloc(prefix_len + url_len + 1);
  if (ret == NULL) {
    errno = ENOMEM;
    return NULL;
  }

  memcpy(ret, prefix, prefix_len);
  memcpy(ret + prefix_len, URL, url_len + 1);

  return ret;
}

static size_t write_callback(char *ptr, size_t elem_size, size_t elem_count,
                             void *userdata) {
  download_buffer *buf = userdata;
  size_t incoming;
  unsigned char *new_data;

  if (buf == NULL) {
    return 0;
  }

  if (elem_count != 0 && elem_size > SIZE_MAX / elem_count) {
    buf->err = EOVERFLOW;
    return 0;
  }

  incoming = elem_size * elem_count;

  if (incoming == 0) {
    return 0;
  }

  if (buf->size > SIZE_MAX - incoming) {
    buf->err = EOVERFLOW;
    return 0;
  }

  new_data = realloc(buf->data, buf->size + incoming);
  if (new_data == NULL) {
    buf->err = ENOMEM;
    return 0;
  }

  memcpy(new_data + buf->size, ptr, incoming);

  buf->data = new_data;
  buf->size += incoming;

  return incoming;
}

static int curlcode_to_errno(CURLcode rc) {
  switch (rc) {
  case CURLE_OK:
    return 0;

  case CURLE_UNSUPPORTED_PROTOCOL:
    return EPROTONOSUPPORT;

  case CURLE_URL_MALFORMAT:
    return EINVAL;

  case CURLE_COULDNT_RESOLVE_PROXY:
  case CURLE_COULDNT_RESOLVE_HOST:
    return EHOSTUNREACH;

  case CURLE_COULDNT_CONNECT:
    return ECONNREFUSED;

  case CURLE_OPERATION_TIMEDOUT:
    return ETIMEDOUT;

  case CURLE_REMOTE_ACCESS_DENIED:
    return EACCES;

  case CURLE_SSL_CONNECT_ERROR:
  case CURLE_SSL_CACERT_BADFILE:
  case CURLE_PEER_FAILED_VERIFICATION:
    return EACCES;

  case CURLE_HTTP_RETURNED_ERROR:
    return EPROTO;

  case CURLE_TOO_MANY_REDIRECTS:
    return ELOOP;

  case CURLE_RECV_ERROR:
    return ECONNRESET;

  case CURLE_SEND_ERROR:
    return EPIPE;

  case CURLE_OUT_OF_MEMORY:
    return ENOMEM;

  case CURLE_WRITE_ERROR:
    return EIO;

  default:
    return EIO;
  }
}

#define SET_CURL_OPT(handle, opt, value)                                       \
  do {                                                                         \
    rc = curl_easy_setopt((handle), (opt), (value));                           \
    if (rc != CURLE_OK) {                                                      \
      saved_errno = curlcode_to_errno(rc);                                     \
      goto fail;                                                               \
    }                                                                          \
  } while (0)

void *download_url(const char *URL, size_t *size,
                   const custom_certificate *cert) {
  CURL *curl = NULL;
  CURLcode rc;
  download_buffer buf;
  char *effective_url = NULL;
  int saved_errno = 0;
  LOG_DEBUG("download_url(\"%s\")", URL);

#if defined(CURLOPT_CAINFO_BLOB)
  struct curl_blob ca_blob;
#endif

  memset(&buf, 0, sizeof(buf));

  if (size != NULL) {
    *size = 0;
  }

  if (URL == NULL || URL[0] == '\0' || size == NULL) {
    errno = EINVAL;
    return NULL;
  }

  effective_url = make_effective_url(URL);
  if (effective_url == NULL) {
    return NULL;
  }

  pthread_once(&curl_once, curl_global_init_once);
  if (curl_global_errno != 0) {
    free(effective_url);
    errno = curl_global_errno;
    return NULL;
  }

  curl = curl_easy_init();
  if (curl == NULL) {
    free(effective_url);
    errno = ENOMEM;
    return NULL;
  }

  SET_CURL_OPT(curl, CURLOPT_URL, effective_url);

  /*
   * Only allow HTTP and HTTPS.
   * This prevents cursed redirects like http://good.example ->
   * file:///etc/passwd.
   */
#if defined(CURLOPT_PROTOCOLS_STR)
  SET_CURL_OPT(curl, CURLOPT_PROTOCOLS_STR, "http,https");
  SET_CURL_OPT(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
  SET_CURL_OPT(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
  SET_CURL_OPT(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif

  SET_CURL_OPT(curl, CURLOPT_WRITEFUNCTION, write_callback);
  SET_CURL_OPT(curl, CURLOPT_WRITEDATA, &buf);

  /*
   * Important for daemons and threaded programs.
   */
  SET_CURL_OPT(curl, CURLOPT_NOSIGNAL, 1L);

  /*
   * Treat HTTP 4xx/5xx as failure.
   */
  SET_CURL_OPT(curl, CURLOPT_FAILONERROR, 1L);

  /*
   * Follow redirects, but cap them.
   */
  SET_CURL_OPT(curl, CURLOPT_FOLLOWLOCATION, 1L);
  SET_CURL_OPT(curl, CURLOPT_MAXREDIRS, 5L);

  /*
   * Do not let a daemon hang forever because a server chose spiritual death.
   */
  SET_CURL_OPT(curl, CURLOPT_CONNECTTIMEOUT_MS, 10000L);
  SET_CURL_OPT(curl, CURLOPT_TIMEOUT_MS, 60000L);

  /*
   * Verify HTTPS properly.
   */
  SET_CURL_OPT(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  SET_CURL_OPT(curl, CURLOPT_SSL_VERIFYHOST, 2L);

  /*
   * Optional custom CA cert.
   */
  if (cert != NULL) {
    if (cert->ca_pem != NULL && cert->ca_pem_len != 0) {
#if defined(CURLOPT_CAINFO_BLOB)
      memset(&ca_blob, 0, sizeof(ca_blob));

      ca_blob.data = (void *)cert->ca_pem;
      ca_blob.len = cert->ca_pem_len;
      ca_blob.flags = CURL_BLOB_NOCOPY;

      SET_CURL_OPT(curl, CURLOPT_CAINFO_BLOB, &ca_blob);
#else
      saved_errno = ENOTSUP;
      goto fail;
#endif
    } else if (cert->ca_file != NULL && cert->ca_file[0] != '\0') {
      SET_CURL_OPT(curl, CURLOPT_CAINFO, cert->ca_file);
    }
  }
  char curl_errbuf[CURL_ERROR_SIZE];
  long http_code = 0;
  curl_errbuf[0] = '\0';
  SET_CURL_OPT(curl, CURLOPT_ERRORBUFFER, curl_errbuf);
  rc = curl_easy_perform(curl);
  if (rc != CURLE_OK) {
    LOG_ERR("curl failed: rc=%d, curl=%s, detail=%s, http=%ld\n", (int)rc,
            curl_easy_strerror(rc), curl_errbuf[0] ? curl_errbuf : "(none)",
            http_code);
    saved_errno = buf.err != 0 ? buf.err : curlcode_to_errno(rc);
    goto fail;
  }

  /*
   * Empty response body is success.
   * Return malloc(1) so NULL means failure only.
   */
  if (buf.data == NULL) {
    buf.data = malloc(1);
    if (buf.data == NULL) {
      saved_errno = ENOMEM;
      goto fail;
    }
  }

  *size = buf.size;

  curl_easy_cleanup(curl);
  free(effective_url);

  return buf.data;

fail:
  curl_easy_cleanup(curl);
  free(effective_url);
  free(buf.data);

  if (size != NULL) {
    *size = 0;
  }

  errno = saved_errno != 0 ? saved_errno : EIO;
  return NULL;
}

#undef SET_CURL_OPT
