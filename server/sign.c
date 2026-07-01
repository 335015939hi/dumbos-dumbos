// sign_file.c
#define _POSIX_C_SOURCE 200809L

#include <openssl/err.h>
#include <openssl/evp.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ED25519_PRIVATE_KEY_LEN 32
#define ED25519_SIGNATURE_LEN 64

static void print_crypto_errors(void) { ERR_print_errors_fp(stderr); }

static int hexval(int c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

static int hex_decode_fixed(const char *hex, uint8_t *out, size_t out_len) {
  if (!hex)
    return 0;

  size_t hex_len = strlen(hex);
  if (hex_len != out_len * 2) {
    return 0;
  }

  for (size_t i = 0; i < out_len; i++) {
    int hi = hexval((unsigned char)hex[i * 2]);
    int lo = hexval((unsigned char)hex[i * 2 + 1]);

    if (hi < 0 || lo < 0) {
      return 0;
    }

    out[i] = (uint8_t)((hi << 4) | lo);
  }

  return 1;
}

static int read_entire_file(const char *path, uint8_t **data_out,
                            size_t *len_out) {
  int fd = -1;
  struct stat st;
  uint8_t *buf = NULL;

  if (!path || !data_out || !len_out) {
    errno = EINVAL;
    return 0;
  }

  *data_out = NULL;
  *len_out = 0;

  fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    return 0;
  }

  if (fstat(fd, &st) != 0) {
    goto fail;
  }

  if (!S_ISREG(st.st_mode)) {
    errno = EINVAL;
    goto fail;
  }

  if (st.st_size < 0) {
    errno = EINVAL;
    goto fail;
  }

  /*
   * Your verifier currently takes int datalen.
   * Refuse files too large to pass to that verifier safely.
   */
  if ((uintmax_t)st.st_size > (uintmax_t)INT_MAX) {
    errno = EFBIG;
    goto fail;
  }

  size_t len = (size_t)st.st_size;

  if (len == 0) {
    /*
     * malloc(0) is annoying. Give empty files a real pointer.
     */
    buf = malloc(1);
    if (!buf) {
      goto fail;
    }
  } else {
    buf = malloc(len);
    if (!buf) {
      goto fail;
    }

    size_t off = 0;
    while (off < len) {
      ssize_t n = read(fd, buf + off, len - off);
      if (n < 0) {
        if (errno == EINTR)
          continue;
        goto fail;
      }
      if (n == 0) {
        errno = EIO;
        goto fail;
      }
      off += (size_t)n;
    }
  }

  close(fd);

  *data_out = buf;
  *len_out = len;
  return 1;

fail: {
  int saved_errno = errno;
  close(fd);
  free(buf);
  errno = saved_errno;
}
  return 0;
}

static int write_entire_file(const char *path, const void *data, size_t len) {
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
  if (fd < 0) {
    return 0;
  }

  const uint8_t *p = data;
  size_t off = 0;

  while (off < len) {
    ssize_t n = write(fd, p + off, len - off);
    if (n < 0) {
      if (errno == EINTR)
        continue;

      int saved_errno = errno;
      close(fd);
      errno = saved_errno;
      return 0;
    }

    if (n == 0) {
      int saved_errno = EIO;
      close(fd);
      errno = saved_errno;
      return 0;
    }

    off += (size_t)n;
  }

  if (close(fd) != 0) {
    return 0;
  }

  return 1;
}

static char *make_sig_path(const char *path) {
  const char suffix[] = ".sig";

  size_t path_len = strlen(path);
  size_t suffix_len = sizeof(suffix) - 1;

  if (path_len > SIZE_MAX - suffix_len - 1) {
    errno = ENAMETOOLONG;
    return NULL;
  }

  char *out = malloc(path_len + suffix_len + 1);
  if (!out) {
    return NULL;
  }

  memcpy(out, path, path_len);
  memcpy(out + path_len, suffix, suffix_len + 1);

  return out;
}

static int ed25519_sign(uint8_t sig[ED25519_SIGNATURE_LEN],
                        const uint8_t priv[ED25519_PRIVATE_KEY_LEN],
                        const void *data, size_t datalen) {
  int ok = 0;
  EVP_PKEY *pkey = NULL;
  EVP_MD_CTX *ctx = NULL;

  pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, priv,
                                      ED25519_PRIVATE_KEY_LEN);

  if (!pkey) {
    print_crypto_errors();
    goto out;
  }

  ctx = EVP_MD_CTX_new();
  if (!ctx) {
    print_crypto_errors();
    goto out;
  }

  /*
   * Ed25519 uses no external digest here.
   * Do not pass EVP_sha256().
   * Do not use EVP_DigestSignUpdate().
   */
  if (EVP_DigestSignInit(ctx, NULL, NULL, NULL, pkey) != 1) {
    print_crypto_errors();
    goto out;
  }

  size_t siglen = ED25519_SIGNATURE_LEN;

  if (EVP_DigestSign(ctx, sig, &siglen, data, datalen) != 1) {
    print_crypto_errors();
    goto out;
  }

  if (siglen != ED25519_SIGNATURE_LEN) {
    fprintf(stderr, "unexpected signature length: %zu\n", siglen);
    goto out;
  }

  ok = 1;

out:
  EVP_MD_CTX_free(ctx);
  EVP_PKEY_free(pkey);
  return ok;
}

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s <private_key_hex_64_chars> <file>\n", argv[0]);
    return 2;
  }

  const char *priv_hex = argv[1];
  const char *file_path = argv[2];

  uint8_t priv[ED25519_PRIVATE_KEY_LEN];
  uint8_t sig[ED25519_SIGNATURE_LEN];

  uint8_t *data = NULL;
  size_t datalen = 0;

  char *sig_path = NULL;

  if (!hex_decode_fixed(priv_hex, priv, sizeof(priv))) {
    fprintf(stderr, "bad private key hex: expected 64 hex chars\n");
    return 2;
  }

  if (!read_entire_file(file_path, &data, &datalen)) {
    fprintf(stderr, "failed to read %s: %s\n", file_path, strerror(errno));
    return 1;
  }

  if (!ed25519_sign(sig, priv, data, datalen)) {
    free(data);
    return 1;
  }

  sig_path = make_sig_path(file_path);
  if (!sig_path) {
    fprintf(stderr, "failed to create signature path: %s\n", strerror(errno));
    free(data);
    return 1;
  }

  if (!write_entire_file(sig_path, sig, sizeof(sig))) {
    fprintf(stderr, "failed to write %s: %s\n", sig_path, strerror(errno));
    free(sig_path);
    free(data);
    return 1;
  }

  printf("wrote %s (%d bytes)\n", sig_path, ED25519_SIGNATURE_LEN);

  free(sig_path);
  free(data);
  return 0;
}
