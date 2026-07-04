/*
 * ed25519_hex.c
 *
 * Ed25519 key generation, signing, and verification using OpenSSL.
 *
 * Key format:
 *   private key: 32 raw Ed25519 private seed bytes, hex encoded -> 64 chars
 *   public key:  32 raw Ed25519 public key bytes, hex encoded  -> 64 chars
 *   signature:   64 raw Ed25519 signature bytes, hex encoded   -> 128 chars
 *
 * Build:
 *   cc -Wall -Wextra -O2 -DTEST_ED25519 ed25519_hex.c -lcrypto -o ed25519_hex
 *
 * Requires OpenSSL 1.1.1 or newer.
 */

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <openssl/crypto.h>
#include <openssl/evp.h>

#include "ed25519.h"

/*
 * Convert one ASCII hex character to its value.
 *
 * Returns:
 *   0..15 on success
 *   -1    on invalid input
 */
static int hex_char_value(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';

  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;

  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;

  return -1;
}

/*
 * Decode a fixed-length hex string into bytes.
 *
 * hex must be exactly out_len * 2 characters long.
 *
 * Returns:
 *   0  on success
 *   -1 on failure, errno set
 */
static int hex_to_bytes(const char *hex, unsigned char *out, size_t out_len) {
  size_t hex_len;

  if (hex == NULL || out == NULL) {
    errno = EINVAL;
    return -1;
  }

  hex_len = strlen(hex);

  if (hex_len != out_len * 2) {
    errno = EINVAL;
    return -1;
  }

  for (size_t i = 0; i < out_len; i++) {
    int hi = hex_char_value(hex[i * 2]);
    int lo = hex_char_value(hex[i * 2 + 1]);

    if (hi < 0 || lo < 0) {
      errno = EINVAL;
      return -1;
    }

    out[i] = (unsigned char)((hi << 4) | lo);
  }

  return 0;
}

/*
 * Encode bytes as lowercase hex.
 *
 * out_size must be at least len * 2 + 1.
 *
 * Returns:
 *   0  on success
 *   -1 on failure, errno set
 */
static int bytes_to_hex(const unsigned char *in, size_t len, char *out,
                        size_t out_size) {
  static const char table[] = "0123456789abcdef";

  if (in == NULL || out == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (out_size < len * 2 + 1) {
    errno = ENOSPC;
    return -1;
  }

  for (size_t i = 0; i < len; i++) {
    out[i * 2] = table[(in[i] >> 4) & 0x0f];
    out[i * 2 + 1] = table[in[i] & 0x0f];
  }

  out[len * 2] = '\0';
  return 0;
}

/*
 * Generate an Ed25519 keypair.
 *
 * public_hex must have room for ED25519_PUBLIC_KEY_HEX_SIZE bytes.
 * private_hex must have room for ED25519_PRIVATE_KEY_HEX_SIZE bytes.
 *
 * Returns:
 *   0  on success
 *   -1 on failure, errno set
 */
int ed25519_generate_keypair_hex(char *public_hex, char *private_hex) {
  int ret = -1;

  EVP_PKEY_CTX *ctx = NULL;
  EVP_PKEY *pkey = NULL;

  unsigned char public_key[ED25519_PUBLIC_KEY_SIZE];
  unsigned char private_key[ED25519_PRIVATE_KEY_SIZE];

  size_t public_len = sizeof public_key;
  size_t private_len = sizeof private_key;

  if (public_hex == NULL || private_hex == NULL) {
    errno = EINVAL;
    return -1;
  }

  ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
  if (ctx == NULL) {
    errno = EIO;
    goto cleanup;
  }

  if (EVP_PKEY_keygen_init(ctx) <= 0) {
    errno = EIO;
    goto cleanup;
  }

  if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
    errno = EIO;
    goto cleanup;
  }

  if (EVP_PKEY_get_raw_public_key(pkey, public_key, &public_len) <= 0 ||
      public_len != ED25519_PUBLIC_KEY_SIZE) {
    errno = EIO;
    goto cleanup;
  }

  if (EVP_PKEY_get_raw_private_key(pkey, private_key, &private_len) <= 0 ||
      private_len != ED25519_PRIVATE_KEY_SIZE) {
    errno = EIO;
    goto cleanup;
  }

  if (bytes_to_hex(public_key, sizeof public_key, public_hex,
                   ED25519_PUBLIC_KEY_HEX_SIZE) < 0) {
    goto cleanup;
  }

  if (bytes_to_hex(private_key, sizeof private_key, private_hex,
                   ED25519_PRIVATE_KEY_HEX_SIZE) < 0) {
    goto cleanup;
  }

  ret = 0;

cleanup:
  OPENSSL_cleanse(private_key, sizeof private_key);

  EVP_PKEY_free(pkey);
  EVP_PKEY_CTX_free(ctx);

  return ret;
}

/*
 * Sign data with a raw Ed25519 private key encoded as hex.
 *
 * private_hex must be 64 hex chars plus NUL.
 * signature_hex must have room for ED25519_SIGNATURE_HEX_SIZE bytes.
 *
 * Returns:
 *   0  on success
 *   -1 on failure, errno set
 */
int ed25519_sign_hex(const char *private_hex, const void *data, size_t size,
                     char *signature_hex) {
  int ret = -1;

  unsigned char private_key[ED25519_PRIVATE_KEY_SIZE];
  unsigned char signature[ED25519_SIGNATURE_SIZE];

  size_t signature_len = sizeof signature;

  EVP_PKEY *pkey = NULL;
  EVP_MD_CTX *ctx = NULL;

  if (private_hex == NULL || signature_hex == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (data == NULL && size != 0) {
    errno = EINVAL;
    return -1;
  }

  if (hex_to_bytes(private_hex, private_key, sizeof private_key) < 0) {
    return -1;
  }

  pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, private_key,
                                      sizeof private_key);
  if (pkey == NULL) {
    errno = EIO;
    goto cleanup;
  }

  ctx = EVP_MD_CTX_new();
  if (ctx == NULL) {
    errno = EIO;
    goto cleanup;
  }

  /*
   * Ed25519 does not use a separate hash function here.
   * The digest argument must be NULL.
   *
   * Also: do not use EVP_DigestSignUpdate() for Ed25519.
   * One-shot EVP_DigestSign() is the correct API.
   */
  if (EVP_DigestSignInit(ctx, NULL, NULL, NULL, pkey) <= 0) {
    errno = EIO;
    goto cleanup;
  }

  if (EVP_DigestSign(ctx, signature, &signature_len,
                     (const unsigned char *)data, size) <= 0) {
    errno = EIO;
    goto cleanup;
  }

  if (signature_len != ED25519_SIGNATURE_SIZE) {
    errno = EIO;
    goto cleanup;
  }

  if (bytes_to_hex(signature, sizeof signature, signature_hex,
                   ED25519_SIGNATURE_HEX_SIZE) < 0) {
    goto cleanup;
  }

  ret = 0;

cleanup:
  OPENSSL_cleanse(private_key, sizeof private_key);
  OPENSSL_cleanse(signature, sizeof signature);

  EVP_MD_CTX_free(ctx);
  EVP_PKEY_free(pkey);

  return ret;
}

/*
 * Verify an Ed25519 signature.
 *
 * public_hex must be 64 hex chars plus NUL.
 * signature_hex must be 128 hex chars plus NUL.
 *
 * Returns:
 *   0  if the signature is valid
 *   -1 if invalid or on error, errno set
 *
 * Invalid signature sets errno = EPERM.
 * Bad input sets errno = EINVAL.
 * OpenSSL/internal failure sets errno = EIO.
 */
int ed25519_verify_hex(const char *public_hex, const void *data, size_t size,
                       const char *signature_hex) {
  int verify_ret;

  unsigned char public_key[ED25519_PUBLIC_KEY_SIZE];
  unsigned char signature[ED25519_SIGNATURE_SIZE];

  EVP_PKEY *pkey = NULL;
  EVP_MD_CTX *ctx = NULL;

  if (public_hex == NULL || signature_hex == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (data == NULL && size != 0) {
    errno = EINVAL;
    return -1;
  }

  if (hex_to_bytes(public_hex, public_key, sizeof public_key) < 0) {
    return -1;
  }

  if (hex_to_bytes(signature_hex, signature, sizeof signature) < 0) {
    return -1;
  }

  pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, public_key,
                                     sizeof public_key);
  if (pkey == NULL) {
    errno = EIO;
    goto error;
  }

  ctx = EVP_MD_CTX_new();
  if (ctx == NULL) {
    errno = EIO;
    goto error;
  }

  /*
   * Ed25519 verification is also one-shot.
   * No EVP_DigestVerifyUpdate().
   */
  if (EVP_DigestVerifyInit(ctx, NULL, NULL, NULL, pkey) <= 0) {
    errno = EIO;
    goto error;
  }

  verify_ret = EVP_DigestVerify(ctx, signature, sizeof signature,
                                (const unsigned char *)data, size);

  EVP_MD_CTX_free(ctx);
  EVP_PKEY_free(pkey);

  if (verify_ret == 1) {
    return 0;
  }

  if (verify_ret == 0) {
    errno = EPERM;
    return -1;
  }

  errno = EIO;
  return -1;

error:
  EVP_MD_CTX_free(ctx);
  EVP_PKEY_free(pkey);
  return -1;
}

#ifdef TEST_ED25519

int main(void) {
  char public_hex[ED25519_PUBLIC_KEY_HEX_SIZE];
  char private_hex[ED25519_PRIVATE_KEY_HEX_SIZE];
  char signature_hex[ED25519_SIGNATURE_HEX_SIZE];

  const char msg[] = "hello from the human cryptography circus";

  if (ed25519_generate_keypair_hex(public_hex, private_hex) < 0) {
    perror("ed25519_generate_keypair_hex");
    return 1;
  }

  printf("public:  %s\n", public_hex);
  printf("private: %s\n", private_hex);

  if (ed25519_sign_hex(private_hex, msg, strlen(msg), signature_hex) < 0) {
    perror("ed25519_sign_hex");
    return 1;
  }

  printf("sig:     %s\n", signature_hex);

  if (ed25519_verify_hex(public_hex, msg, strlen(msg), signature_hex) < 0) {
    perror("ed25519_verify_hex");
    return 1;
  }

  puts("valid signature");

  if (ed25519_verify_hex(public_hex, "tampered", strlen("tampered"),
                         signature_hex) < 0) {
    perror("tampered verify");
    puts("tampered message correctly rejected");
  } else {
    puts("BUG: tampered message accepted");
    return 1;
  }

  return 0;
}

#endif
