#ifndef _DUMB_H
#define _DUMB_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "common.h"  //for DEBUG_MODE
#include "ed25519.h" //for ED25519_SIGNATURE_HEX_SIZE

char *malloc_buf_to_hex(const void *buf, size_t size);
void *malloc_hex_to_buf(const char *hex);
void *malloc_prepare_file(const char *const code, size_t *ret_len);

/*
 * secret code: when the server recieves a code, it converts the code into a
 * file path and looks for the file. if no file exists, it is interpreted as an
 * invalid code. to prevent path transversals, code injection, and other misc
 * exploits we should aggresively limit the allowed characters. the server
 * should check the code for illegal characters before looking up a code.
 */
// maximum length of a code
#define CODE_MAXLEN 256
// the prefix added to secret code
#define CODE_FILE_PATH "dumb-codes/"
// suffix appended to secret code
#define CODE_EXT ""
// allowed characters for the secret code
#define SECRET_CODE_ALLOWED_CHARS "qwertyuiopasdfghjklzxcvbnm0123456789-_"

// size of the expiry date field
#define EXPIRE_SIZE 16 // epoch time
// size of the command field. after shrinking this check the commands (below) to
// make sure they all still fit
#define COMMAND_SIZE 16

// default time offset for expire, in seconds. by default, the code will expire
// DEFAULT_EXPIRE_TIME seconds after being used for the first time
#define DEFAULT_EXPIRE_TIME 60

// struct defining a payload.
// sizeof(struct DUMB_PAYLOAD) will exlucde the .payload (to be renamed .data)
// field
struct DUMB_PAYLOAD {
  // the signature of the whole payload, with the signature field zero'd
  char signature[ED25519_SIGNATURE_HEX_SIZE];
  // the expiry date, in epoch seconds, as a string. a string ":<integer>" will
  // be interpreted as expiring <integer> seconds after use. an invalid integer
  // conversion will fallback to default behaviour, i.e. expire
  // DEFAULT_EXPIRE_TIME seconds after use. the string "0" means valid
  // indefinately.
  char expire[EXPIRE_SIZE];
  // the command. see the macros below CODE_CMD_*
  char command[COMMAND_SIZE];
  // the data of the payload. this is dependent in what the payload is supposed
  // to do and therefore of unknown size. sizeof will exclude this field.
  // FIXME:rename to data
  char payload[];
};

// commands for the code
// WARNING make sure these strings, including NULL terminator, are smaller than
// COMMAND_SIZE. otherwise behaviour is undefined
// this header file shall be the authoritative source for documenting each
// command and the format of its data. all integers shall use network endianess.
// all strings are C-style strings.
#ifdef DEBUG_MODE
// execute some shell commands. data is a shell script (1 string)
#define CODE_CMD_SHELL "script"
#endif // DEBUG_MODE
// no-op. no data. useful for testing connectivity and debugging
#define CODE_CMD_OK "ok"
// install package given in data. data is one apk file
#define CODE_CMD_INSTALLTHIS "install-this"
// install package given by path. data is path and checksum (2 strings)
#define CODE_CMD_INSTALL_PATH "install-path"
// mount /dev/block/sda1 and copy /sdcard/* to it. no data
// TODO:optional <path> string for copy source
#define CODE_CMD_FILE_EXPORT "export-files"
// mount /dev/block/sda1 and copy files from it to internal storage. no data
#define CODE_CMD_FILE_IMPORT "import-files"
// enable or disable ADB. no data
#define CODE_CMD_ADB_ENABLE "adb-enable"
#define CODE_CMD_ADB_DISABLE "adb-disable"
// toggle wifi. no data
#define CODE_CMD_WIFI_ENABLE "wifi-enable"
#define CODE_CMD_WIFI_DISABLE "wifi-disable"
// toggle OEM unlocking. no data
#define CODE_CMD_OEM_UNLOCK "oem-unlock"
#define CODE_CMD_OEM_LOCK "oem-lock"
// multiple payloads in one. all integers use network endianess.
// format is unsigned short number (number of payloads); unsigned long size
// (size of payload), struct DUMB_PAYLOAD payload, unsigned long size2, struct
// DUMB_PAYLOAD payload2, ...
#define CODE_CMD_COMPOSITE "composite"
// add, remove, or flush list of internet-enabled apps. will reload firewall.
// format (except CODE_CMD_FW_FLUSH, which has no data) is NULL seperated list
// of app IDs
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
// get expire date, setting it if not exist. use this one. may write to the
// payload.
time_t dp_get_expire_or_set(struct DUMB_PAYLOAD *payload);
// true if expired, false otherwise. will use dp_get_expire_or_set()
bool dp_is_expired(struct DUMB_PAYLOAD *payload);
// similar to dp_is_expired, but compares against given time instead of system
// time. useful if system time is untrusted (i.e. user device; the daemon should
// get time from the network)
bool dp_is_expired_compare(struct DUMB_PAYLOAD *payload, time_t cur_time);
// find and load a secret code payload, returning NULL on error. will return
// malloc'ed buffer. this will also perform checks such as expiry and
// automatically sign.
// caller owns the returned buffer.
// intended for server-side only
void *dp_malloc_check_load(const char *const code, size_t *ret_size,
                           const char *ed25519_private_key);
// load payload by path. does no checks. return malloc'ed buffer
// caller owns the returned buffer.
void *dp_malloc_load(const char *path, size_t *ret_size);

// verifies the payload against pubkey_hex. returns 0 on success, non-zero and
// sets errno on any failure
//!!!this will delete the signature from payload!!!
int dp_verify(struct DUMB_PAYLOAD *payload, size_t size,
              const char *pubkey_hex);

// signs the payload using private_key_hex. returns 0 on success, non-0 and sets
// errno on failure
int dp_sign(struct DUMB_PAYLOAD *payload, size_t size,
            const char *private_key_hex);

#endif
