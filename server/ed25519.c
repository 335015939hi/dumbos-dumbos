// ed25519_demo.c
#include <openssl/err.h>
#include <openssl/evp.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ED25519_PRIV_LEN 32
#define ED25519_PUB_LEN 32
#define ED25519_SIG_LEN 64

static void print_errors(void) { ERR_print_errors_fp(stderr); }

static int hexval(int c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

static int hex_decode(const char *hex, uint8_t *out, size_t out_len) {
  size_t hex_len = strlen(hex);

  if (hex_len != out_len * 2) {
    return 0;
  }

  for (size_t i = 0; i < out_len; i++) {
    int hi = hexval(hex[i * 2]);
    int lo = hexval(hex[i * 2 + 1]);

    if (hi < 0 || lo < 0) {
      return 0;
    }

    out[i] = (uint8_t)((hi << 4) | lo);
  }

  return 1;
}

static void hex_print(const char *label, const uint8_t *buf, size_t len) {
  printf("%s=", label);

  for (size_t i = 0; i < len; i++) {
    printf("%02x", buf[i]);
  }

  printf("\n");
}
static int generate_ed25519_keypair(uint8_t pub[ED25519_PUB_LEN],
                                    uint8_t priv[ED25519_PRIV_LEN]) {
  int ok = 0;
  EVP_PKEY_CTX *pctx = NULL;
  EVP_PKEY *pkey = NULL;

  pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
  if (!pctx) {
    print_errors();
    goto out;
  }

  if (EVP_PKEY_keygen_init(pctx) != 1) {
    print_errors();
    goto out;
  }

  if (EVP_PKEY_keygen(pctx, &pkey) != 1) {
    print_errors();
    goto out;
  }

  size_t priv_len = ED25519_PRIV_LEN;
  if (EVP_PKEY_get_raw_private_key(pkey, priv, &priv_len) != 1 ||
      priv_len != ED25519_PRIV_LEN) {
    print_errors();
    goto out;
  }

  size_t pub_len = ED25519_PUB_LEN;
  if (EVP_PKEY_get_raw_public_key(pkey, pub, &pub_len) != 1 ||
      pub_len != ED25519_PUB_LEN) {
    print_errors();
    goto out;
  }

  ok = 1;

out:
  EVP_PKEY_free(pkey);
  EVP_PKEY_CTX_free(pctx);
  return ok;
}

static int ed25519_sign(uint8_t sig[ED25519_SIG_LEN],
                        const uint8_t priv[ED25519_PRIV_LEN],
                        const uint8_t *msg, size_t msg_len) {
  int ok = 0;
  EVP_PKEY *pkey = NULL;
  EVP_MD_CTX *ctx = NULL;

  pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, priv,
                                      ED25519_PRIV_LEN);

  if (!pkey) {
    print_errors();
    goto out;
  }

  ctx = EVP_MD_CTX_new();
  if (!ctx) {
    print_errors();
    goto out;
  }

  /*
   * For Ed25519, digest must be NULL.
   * No EVP_sha256(). No EVP_DigestSignUpdate().
   */
  if (EVP_DigestSignInit(ctx, NULL, NULL, NULL, pkey) != 1) {
    print_errors();
    goto out;
  }

  size_t sig_len = ED25519_SIG_LEN;

  if (EVP_DigestSign(ctx, sig, &sig_len, msg, msg_len) != 1) {
    print_errors();
    goto out;
  }

  if (sig_len != ED25519_SIG_LEN) {
    fprintf(stderr, "unexpected signature length: %zu\n", sig_len);
    goto out;
  }

  ok = 1;

out:
  EVP_MD_CTX_free(ctx);
  EVP_PKEY_free(pkey);
  return ok;
}

static int ed25519_verify(const uint8_t pub[ED25519_PUB_LEN],
                          const uint8_t sig[ED25519_SIG_LEN],
                          const uint8_t *msg, size_t msg_len) {
  int ok = 0;
  EVP_PKEY *pkey = NULL;
  EVP_MD_CTX *ctx = NULL;

  pkey =
      EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, pub, ED25519_PUB_LEN);

  if (!pkey) {
    print_errors();
    goto out;
  }

  ctx = EVP_MD_CTX_new();
  if (!ctx) {
    print_errors();
    goto out;
  }

  /*
   * Same rule: digest must be NULL.
   */
  if (EVP_DigestVerifyInit(ctx, NULL, NULL, NULL, pkey) != 1) {
    print_errors();
    goto out;
  }

  int ret = EVP_DigestVerify(ctx, sig, ED25519_SIG_LEN, msg, msg_len);

  if (ret == 1) {
    ok = 1; // valid signature
  } else if (ret == 0) {
    ok = 0; // invalid signature
  } else {
    print_errors(); // API/internal error
    ok = 0;
  }

out:
  EVP_MD_CTX_free(ctx);
  EVP_PKEY_free(pkey);
  return ok;
}

static uint8_t *make_signed_blob(const uint8_t *msg, size_t msg_len,
                                 size_t *blob_len_out) {
  const char *prefix = "DUMBOSD1\nlen=";
  const char *sep = "\n\n";

  char lenbuf[32];
  int lenbuf_len = snprintf(lenbuf, sizeof(lenbuf), "%zu", msg_len);
  if (lenbuf_len < 0 || (size_t)lenbuf_len >= sizeof(lenbuf)) {
    return NULL;
  }

  size_t prefix_len = strlen(prefix);
  size_t sep_len = strlen(sep);

  size_t blob_len = prefix_len + (size_t)lenbuf_len + sep_len + msg_len;

  uint8_t *blob = malloc(blob_len);
  if (!blob) {
    return NULL;
  }

  size_t off = 0;

  memcpy(blob + off, prefix, prefix_len);
  off += prefix_len;

  memcpy(blob + off, lenbuf, (size_t)lenbuf_len);
  off += (size_t)lenbuf_len;

  memcpy(blob + off, sep, sep_len);
  off += sep_len;

  memcpy(blob + off, msg, msg_len);
  off += msg_len;

  *blob_len_out = blob_len;
  return blob;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr,
            "usage:\n"
            "  %s keygen\n"
            "  %s sign <priv_hex> <message>\n"
            "  %s verify <pub_hex> <sig_hex> <message>\n",
            argv[0], argv[0], argv[0]);
    return 2;
  }

  if (strcmp(argv[1], "keygen") == 0) {
    uint8_t pub[ED25519_PUB_LEN];
    uint8_t priv[ED25519_PRIV_LEN];

    if (!generate_ed25519_keypair(pub, priv)) {
      return 1;
    }

    hex_print("private", priv, sizeof(priv));
    hex_print("public", pub, sizeof(pub));
    return 0;
  }

  if (strcmp(argv[1], "sign") == 0) {
    if (argc != 4) {
      fprintf(stderr, "usage: %s sign <priv_hex> <message>\n", argv[0]);
      return 2;
    }

    uint8_t priv[ED25519_PRIV_LEN];
    uint8_t sig[ED25519_SIG_LEN];

    if (!hex_decode(argv[2], priv, sizeof(priv))) {
      fprintf(stderr, "bad private key hex\n");
      return 2;
    }

    const uint8_t *msg = (const uint8_t *)argv[3];
    size_t msg_len = strlen(argv[3]);

    size_t blob_len = 0;
    uint8_t *blob = make_signed_blob(msg, msg_len, &blob_len);
    if (!blob) {
      fprintf(stderr, "failed to make signed blob\n");
      return 1;
    }

    int ok = ed25519_sign(sig, priv, blob, blob_len);
    free(blob);

    if (!ok) {
      return 1;
    }

    hex_print("signature", sig, sizeof(sig));
    return 0;
  }

  if (strcmp(argv[1], "verify") == 0) {
    if (argc != 5) {
      fprintf(stderr, "usage: %s verify <pub_hex> <sig_hex> <message>\n",
              argv[0]);
      return 2;
    }

    uint8_t pub[ED25519_PUB_LEN];
    uint8_t sig[ED25519_SIG_LEN];

    if (!hex_decode(argv[2], pub, sizeof(pub))) {
      fprintf(stderr, "bad public key hex\n");
      return 2;
    }

    if (!hex_decode(argv[3], sig, sizeof(sig))) {
      fprintf(stderr, "bad signature hex\n");
      return 2;
    }

    const uint8_t *msg = (const uint8_t *)argv[4];
    size_t msg_len = strlen(argv[4]);

    size_t blob_len = 0;
    uint8_t *blob = make_signed_blob(msg, msg_len, &blob_len);
    if (!blob) {
      fprintf(stderr, "failed to make signed blob\n");
      return 1;
    }

    int ok = ed25519_verify(pub, sig, blob, blob_len);
    free(blob);

    if (ok) {
      printf("OK\n");
      return 0;
    }

    printf("BAD\n");
    return 1;
  }

  fprintf(stderr, "unknown command\n");
  return 2;
}
