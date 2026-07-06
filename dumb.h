#ifndef _DUMB_H
#define _DUMB_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "ed25519.h"

char *malloc_buf_to_hex(const void *buf, size_t size);
void *malloc_hex_to_buf(const char *hex);
void *malloc_prepare_file(const char *const code, size_t *ret_len);

#define CODE_MAXLEN 256
#define CODE_FILE_PATH "dumb-codes/"
#define CODE_EXT ""

#define EXPIRE_SIZE 16 // epoch time
// size of the command section. after shrinking this check the commands to make
// sure they all still fit
#define COMMAND_SIZE 16

// default time offset for expire
#define DEFAULT_EXPIRE_TIME 60

struct DUMB_PAYLOAD {
  char signature[ED25519_SIGNATURE_HEX_SIZE];
  char expire[EXPIRE_SIZE];
  char command[COMMAND_SIZE];
  char payload[];
};

// commands for the code
// WARNING MAKE SURE these strings, including NULL terminator, are smaller than
// COMMAND_SIZE. otherwise behaviour is undefined
#ifdef DEBUG_MODE
// execute some shell commands. payload is script (1 string)
#define CODE_CMD_SHELL "script"
#endif
// no-op
#define CODE_CMD_OK "ok"
// install package given in payload. payload is one big apk file
#define CODE_CMD_INSTALLTHIS "install-this"
// install package given by path. payload is path and checksum (2 strings)
#define CODE_CMD_INSTALL_PATH "install-path"

// set expire date (epoch time, seconds)
void dp_set_expire(struct DUMB_PAYLOAD *payload, time_t);
// get expire date only, try not to use. return 0 on fail
time_t dp_get_expire(const struct DUMB_PAYLOAD *payload);
// get expire date, setting it if not exist. use this one
time_t dp_get_expire_or_set(struct DUMB_PAYLOAD *payload);
// true if expired, flase otherwise. will use dp_get_expire_or_set()
bool dp_is_expired(struct DUMB_PAYLOAD *payload);
// find and load a secret code payload, returning NULL on error. will return
// malloc'ed buffer this will also perform checks such as expiry and
// automatically sign
void *dp_malloc_check_load(const char *const code, size_t *ret_size);
// load payload by path. does no checks. return malloc'ed buffer
void *dp_malloc_load(const char *path, size_t *ret_size);

//!!!this will delete the signature from memory!!!
int dp_verify(struct DUMB_PAYLOAD *payload, size_t size,
              const char *pubkey_hex);

int dp_sign(struct DUMB_PAYLOAD *payload, size_t size,
            const char *private_key_hex);

extern char *ed25519_private_key;

#endif
