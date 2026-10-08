#include "key_private.h"

const char *server_internal_get_private_key(void) {
  return _ed25519_private_key_hex;
}
