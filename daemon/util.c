
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/limits.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../common.h"
#include "util.h"

int rm_r(const char *const path) {
  char *buf;
  DIR *d;
  struct dirent *ent;

  LOG_DEBUG("rm_r(\"%s\") started", path);

  LOG_VERBOSE("clearing directory %s", path);

  d = opendir(path);
  if (d == NULL) {
    LOG_WARN("directory '%s' not exist", path);
    return -1;
  }

  buf = malloc(PATH_MAX);
  if (buf == NULL) {
    LOG_ERRNO("malloc failed", errno);
    return -1;
  }

  while ((ent = readdir(d))) {
    // make sure were not going to ./ and ../
    if (strcmp(ent->d_name, "..") && strcmp(ent->d_name, ".")) {
      snprintf(buf, PATH_MAX, "%s/%s", path, ent->d_name);
      LOG_VERBOSE("unlinking '%s'", buf);
      if (unlink(buf) < 0) {
        LOG_WARN("unlink %s failed:%s", buf, strerror(errno));
      }
    }
  }

  closedir(d);
  free(buf);
  LOG_DEBUG("rm_r() exiting");
  return 0;
}

int mkdir_p(const char *path) {
  char *tmp;
  size_t len;
  char *p;
  int err;

  LOG_DEBUG("mkdir_p(\"%s\") starting", path);

  len = strlen(path);

  if (len >= PATH_MAX) {
    errno = ENAMETOOLONG;
    return -1;
  }

  tmp = malloc(PATH_MAX);
  if (tmp == NULL) {
    return -1;
  }

  strcpy(tmp, path);

  if (tmp[len - 1] == '/') {
    tmp[len - 1] = '\0';
    len--;
  }

  for (p = tmp + 1; *p != '\0'; p++) {
    if (*p == '/') {
      *p = '\0';
      LOG_VERBOSE("creating '%s/'", tmp);

      err = mkdir(tmp, 0700);
      if (err < 0 && errno != EEXIST) {
        LOG_WARN("mkdir '%s' failed:%s", tmp, strerror(errno));
        free(tmp);
        return err;
      }

      *p = '/';
    }
  }

  LOG_VERBOSE("creating '%s/'", tmp);
  err = mkdir(tmp, 0700);
  if (err < 0 && errno != EEXIST) {
    LOG_WARN("mkdir '%s' failed:%s", tmp, strerror(errno));
    free(tmp);
    return err;
  }

  LOG_DEBUG("mkdir_p exiting");

  free(tmp);
  return 0;
}

#define ED25519_PUBLIC_KEY_LEN 32
#define ED25519_SIGNATURE_LEN 64

int verify_ed25519_signature_hex_pubkey(
    const void *data, int datalen, const void *sig, int siglen,
    const char pubkey_hex[ED25519_PUBLIC_KEY_LEN * 2 + 1]);
int verify_ed25519_signature(const void *data, int datalen, const void *sig,
                             int siglen,
                             const char pubkey_hex[ED25519_PUBLIC_KEY_LEN]);

// TODO:
static const char _pubkey[ED25519_PUBLIC_KEY_LEN * 2 + 1] =
    "8494f5310a4b05cf90cfdbaab71413e1ae490f5cb8b113eb47e73b6145dfd58c";
int verify_sig(const char *file, const char *sig) {
  int sfd;
  int ffd;
  off_t sigsize;
  off_t filesize;
  void *sigbuf;
  void *filebuf;
  int err;
  struct stat st;

  LOG_DEBUG("verify_sig() called on '%s' '%s'", file, sig);

  LOG_DEBUG("opening '%s'", sig);
  sfd = open(sig, O_RDONLY);
  if (sfd < 0) {
    LOG_ERR("open '%s' fail:%s", sig, strerror(errno));
    return -1;
  }
  LOG_DEBUG("opening '%s'", file);
  ffd = open(file, O_RDONLY);
  if (ffd < 0) {
    err = errno;
    LOG_ERR("open '%s' fail:%s", file, strerror(errno));
    close(sfd);
    errno = err;
    return -1;
  }
  LOG_DEBUG("ffd=%d sfd=%d", ffd, sfd);

  LOG_DEBUG("getting %s size", sig);
  err = fstat(sfd, &st);
  if (err < 0) {
    err = errno;
    LOG_ERR("failed fstat() on '%s':%s", sig, strerror(err));
    close(sfd);
    close(ffd);
    errno = err;
    return -1;
  }
  sigsize = st.st_size;
  LOG_DEBUG("%s size is %ld", sig, sigsize);

  LOG_DEBUG("getting %s size", file);
  err = fstat(ffd, &st);
  if (err < 0) {
    err = errno;
    LOG_ERR("failed fstat() on '%s':%s", file, strerror(err));
    close(sfd);
    close(ffd);
    errno = err;
    return -1;
  }
  filesize = st.st_size;
  LOG_DEBUG("%s size is %ld", file, filesize);

  // TODO:malloc check
  sigbuf = malloc(sigsize);
  filebuf = malloc(filesize);

  // TODO:read check
  LOG_DEBUG("reading files into RAM");
  read_all(sfd, sigbuf, sigsize);
  read_all(ffd, filebuf, filesize);

  LOG_DEBUG("verifying signature");
  err = verify_ed25519_signature_hex_pubkey(filebuf, filesize, sigbuf, sigsize,
                                            _pubkey);
  LOG_DEBUG("verify_ed25519_signature exited with %d", err);

  free(sigbuf);
  free(filebuf);
  close(sfd);
  close(ffd);
  if (err == 1) {
    return 0;
  } else if (err == 0) {
    errno = EACCES;
    return EACCES;
  } else if (err == -1) {
    errno = EINVAL;
    return EINVAL;
  } else {
    errno = EINVAL;
    return EINVAL;
  }
}

// ChatGpt code below. use with caution

/*
 * Returns:
 *   1 = valid signature
 *   0 = invalid signature
 *  -1 = API / argument error
 */
int verify_ed25519_signature(const void *data, int datalen, const void *sig,
                             int siglen,
                             const char pubkey[ED25519_PUBLIC_KEY_LEN]) {
  int result = -1;
  LOG_DEBUG("verify_ed25519_signature()");
#ifdef DEBUG_MODE
  char *data_str;
  char *sig_str;
  data_str = calloc(1, datalen + 1);
  sig_str = calloc(1, siglen + 1);
  memcpy(data_str, data, datalen);
  memcpy(sig_str, sig, siglen);
  LOG_DEBUG("datalen=%d,data='%s'", datalen, data_str);
  LOG_DEBUG("siglen =%d,sig ='%s'", siglen, sig_str);
  free(sig_str);
  free(data_str);
#endif

  EVP_PKEY *pkey = NULL;
  EVP_MD_CTX *ctx = NULL;

  if (!data || datalen < 0) {
    LOG_ERR("wrong data size");
    return -1;
  } else if (!sig || siglen != ED25519_SIGNATURE_LEN) {
    LOG_ERR("wrong signature size");
    return -1;
  } else if (!pubkey) {
    LOG_ERR("bad pubkey");
    return -1;
  }

  pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL,
                                     (const unsigned char *)pubkey,
                                     ED25519_PUBLIC_KEY_LEN);

  if (!pkey) {
    LOG_ERR("pkey generate fail");
    goto out;
  }

  ctx = EVP_MD_CTX_new();
  if (!ctx) {
    LOG_ERR("ctx fail");
    goto out;
  }

  /*
   * Ed25519 is digestless here.
   * Do NOT pass EVP_sha256().
   * Do NOT use EVP_DigestVerifyUpdate().
   */
  if (EVP_DigestVerifyInit(ctx, NULL, NULL, NULL, pkey) != 1) {
    LOG_ERR("EVP_DigestVerifyInit fail");
    goto out;
  }

  int ret = EVP_DigestVerify(ctx, (const unsigned char *)sig, (size_t)siglen,
                             (const unsigned char *)data, (size_t)datalen);
  LOG_DEBUG("EVP_DigestVerify return %d", ret);

  if (ret == 1) {
    result = 1; // signature valid
  } else if (ret == 0) {
    result = 0; // signature invalid
  } else {
    LOG_ERR("verify_ed25519_signature() internal error");
    result = -1; // OpenSSL/BoringSSL error
  }

out:
  EVP_MD_CTX_free(ctx);
  EVP_PKEY_free(pkey);
  return result;
}

static int hexval(int c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  LOG_DEBUG("bad hex character %c", c);
  return -1;
}

static int hex_decode_fixed(const char *hex, unsigned char *out,
                            size_t out_len) {
  for (size_t i = 0; i < out_len; i++) {
    int hi = hexval((unsigned char)hex[i * 2]);
    int lo = hexval((unsigned char)hex[i * 2 + 1]);

    if (hi < 0 || lo < 0) {
      return 0;
    }

    out[i] = (unsigned char)((hi << 4) | lo);
  }

  // return hex[out_len * 2] == '\0';
  return true;
}

int verify_ed25519_signature_hex_pubkey(
    const void *data, int datalen, const void *sig, int siglen,
    const char pubkey_hex[ED25519_PUBLIC_KEY_LEN * 2 + 1]) {
  unsigned char raw_pubkey[ED25519_PUBLIC_KEY_LEN];
  if (siglen < ED25519_SIGNATURE_LEN) {
    return -1;
  }

  if (!pubkey_hex) {
    return -1;
  }

  if (!hex_decode_fixed(pubkey_hex, raw_pubkey, sizeof(raw_pubkey))) {
    return -1;
  }

  return verify_ed25519_signature(data, datalen, sig, ED25519_SIGNATURE_LEN,
                                  (const char *)raw_pubkey);
}
