#ifndef _REQUEST_ID_H
#define _REQUEST_ID_H

#include "ed25519.h"

#define DUMBOS_USERNAME_MAXLEN 64
#define DUMBOS_FANCY_NAME_MAXLEN 127
#define DUMBOS_DEFAULT_USER "hspiqpwoasfddhaksuuiwqueqwrds"
#define DUMBOS_USER_ALLOWED_CHARS                                              \
  "qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM-_1234567890"
#define REQUESTID_ALLOWED_CHARS                                                \
  "qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890"
#define DUMBOS_USER_DATA_MAGIC "dp2js9chakqkqe1"
#define DUMBOS_USER_DATA_MAGIC_SIZE 16

struct DUMBOS_USER_DATA {
  char magic[DUMBOS_USER_DATA_MAGIC_SIZE];
  char username[DUMBOS_USERNAME_MAXLEN];
  char null_byte;
  // display name, supports special characters, etc.
  char fancy_name[DUMBOS_FANCY_NAME_MAXLEN];
  char fancy_null_byte;
  char priv_key_hex[ED25519_PRIVATE_KEY_HEX_SIZE];
};

// output is a buffer of ED25519_SIGNATURE_HEX_SIZE
int request_id_sign(char *output, const char *user, const char *request_id,
                    long long request_time, const char *priv_key_hex);
// this function not thread safe!!! on the other hand, no need to free()
char *request_id_generate(void);

int request_id_verify(const char *user, const char *request_id,
                      const char *request_time_str, const char *signature,
                      const char *pub_key_hex);

struct DUMBOS_USER_DATA *dumbos_alloc_get_user(void);
struct DUMBOS_USER_DATA *dumbos_alloc_new_user(const char *user,
                                               const char *priv_key_hex);

// returns a pointer to the fancy name, falling back to normal username by
// default. do not modify this
const char *dumbos_get_fancy_name(const struct DUMBOS_USER_DATA *data);
// sets the 'fancy name', return 0 on success, non-0 and setss errno on error
int dumbos_set_fancy_name(struct DUMBOS_USER_DATA *data, const char *name);

// for rust
size_t dumbos_user_data_size(void);

#endif
