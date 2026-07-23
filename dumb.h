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
// install package given in data. data is one big apk file
#define CODE_CMD_INSTALLTHIS "install-this"
// install package given by path. data is path and checksum (2 strings)
#define CODE_CMD_INSTALL_PATH "install-path"
// mount /dev/block/sda1 and copy /sdcard/* to it
#define CODE_CMD_FILE_EXPORT "export-files"
// mount /dev/block/sda1 and copy files from it to internal storage
#define CODE_CMD_FILE_IMPORT "import-files"
// enable or disable ADB
#define CODE_CMD_ADB_ENABLE "adb-enable"
#define CODE_CMD_ADB_DISABLE "adb-disable"
// toggle wifi
#define CODE_CMD_WIFI_ENABLE "wifi-enable"
#define CODE_CMD_WIFI_DISABLE "wifi-disable"
// toggle OEM unlocking
#define CODE_CMD_OEM_UNLOCK "oem-unlock"
#define CODE_CMD_OEM_LOCK "oem-lock"
// multiple payloads in one. all integers use network endianess. format is
// unsigned short number (number of payloads); unsigned long size (size of
// payload), struct DUMB_PAYLOAD payload, unsigned long size2, struct
// DUMB_PAYLOAD payload2, ...
#define CODE_CMD_COMPOSITE "composite"
// add, remove, or flush list of internet-enabled apps. will reload firewall.
// format is NULL seperated list of app IDs (flush has no data)
#define CODE_CMD_FW_ALLOW "fw-allow"
#define CODE_CMD_FW_DENY "fw-deny"
#define CODE_CMD_FW_FLUSH "fw-flush"
// temporarily add a internet enabled app, persists until firewall rules are
// reloaded (reboot or another firewall command)
#define CODE_CMD_FW_TEMP_ADD "fw-tmp-add"

// set expire date (epoch time, seconds)
void dp_set_expire(struct DUMB_PAYLOAD *payload, time_t);
// get expire date only, try not to use. return (time_t)-1 on fail
time_t dp_get_expire(const struct DUMB_PAYLOAD *payload);
// get expire date, setting it if not exist. use this one
time_t dp_get_expire_or_set(struct DUMB_PAYLOAD *payload);
// true if expired, false otherwise. will use dp_get_expire_or_set()
bool dp_is_expired(struct DUMB_PAYLOAD *payload);
// similar to dp_is_expired, but compares against given time instead of system
// time.
bool dp_is_expired_compare(struct DUMB_PAYLOAD *payload, time_t cur_time);
// find and load a secret code payload, returning NULL on error. will return
// malloc'ed buffer this will also perform checks such as expiry and
// automatically sign
void *dp_malloc_check_load(const char *const code, size_t *ret_size,
                           const char *ed25519_private_key);
// load payload by path. does no checks. return malloc'ed buffer
void *dp_malloc_load(const char *path, size_t *ret_size);

//!!!this will delete the signature from memory!!!
int dp_verify(struct DUMB_PAYLOAD *payload, size_t size,
              const char *pubkey_hex);

int dp_sign(struct DUMB_PAYLOAD *payload, size_t size,
            const char *private_key_hex);

#endif
