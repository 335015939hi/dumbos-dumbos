#ifndef _DUMB_CURL_H
#define _DUMB_CURL_H

typedef struct custom_certificate {
  const void *ca_pem;
  size_t ca_pem_len;

  const char *ca_file;
} custom_certificate;

void *download_url(const char *URL, size_t *size,
                   const custom_certificate *cert);
#endif
