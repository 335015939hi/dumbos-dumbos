#ifndef _ED25519_H
#define _ED25519_H

// chatgpt code

#define ED25519_PUBLIC_KEY_SIZE 32
#define ED25519_PRIVATE_KEY_SIZE 32
#define ED25519_SIGNATURE_SIZE 64

#define ED25519_PUBLIC_KEY_HEX_SIZE (ED25519_PUBLIC_KEY_SIZE * 2 + 1)
#define ED25519_PRIVATE_KEY_HEX_SIZE (ED25519_PRIVATE_KEY_SIZE * 2 + 1)
#define ED25519_SIGNATURE_HEX_SIZE (ED25519_SIGNATURE_SIZE * 2 + 1)

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
int ed25519_generate_keypair_hex(char *public_hex, char *private_hex);

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
                     char *signature_hex);

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
                       const char *signature_hex);

#endif
