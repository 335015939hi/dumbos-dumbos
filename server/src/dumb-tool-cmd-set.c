
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "dumb.h"

/*
 * for reference, may be outdated
 * see dumb.h for latest
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
*/

#define APK_PACKAGE_ID_ALLOWED_CHARS                                           \
  "qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM._"

// defined in dumb-main.c
extern const char *path;

// check string s for allowed characters whitelist. returns true if all
// characters allowed
static bool check_allowed_chars(const char *s, const char *whitelist) {
  while (*s != '\0') {
    if (strchr(whitelist, *s) == NULL)
      return false;
    s++;
  }
  return true;
}

static struct DUMB_PAYLOAD *set_data(struct DUMB_PAYLOAD *payload,
                                     void *new_data, size_t data_size) {
  payload = realloc(payload, sizeof(*payload) + data_size);
  if (payload == NULL)
    return NULL;
  memcpy(payload->payload, new_data, data_size);
  return payload;
}

static int write_to_disk(const struct DUMB_PAYLOAD *payload, size_t size) {
  int err;
  FILE *dest = fopen(path, "wb");
  if (dest == NULL) {
    LOG_ERRNO("failed to open file for writing", errno);
    return errno;
  }
  errno = 0;
  size_t written = fwrite(payload, 1, size, dest);
  if (written != size) {
    err = errno;
    LOG_ERRNO("failed to write to file", err);
    fclose(dest);
    errno = err;
    return err;
  }
  LOG("%zu bytes written to %s", written, path);
  fclose(dest);
  errno = 0;
  return 0;
}

// NULL on fail
static struct DUMB_PAYLOAD *load_or_print_error(size_t *size) {
  struct DUMB_PAYLOAD *payload;
  payload = dp_malloc_load(path, size);
  if (NULL == payload) {
    LOG_ERRNO("failed to read file", errno);
    return NULL;
  }
  return payload;
}

static int write_cmd_wrapper(int argc, const char **argv) {
  if (argc != 1) {
    LOG_ERR("'%s' takes no arguments", argv[0]);
    return EINVAL;
  }
  struct DUMB_PAYLOAD *payload;
  size_t size;
  if (NULL == (payload = load_or_print_error(&size))) {
    return errno;
  }
  strncpy(payload->command, argv[0], COMMAND_SIZE);
  write_to_disk(payload, sizeof(struct DUMB_PAYLOAD));
  free(payload);
  return errno;
}

static int cmd_ok(int argc, const char **argv) {
  return write_cmd_wrapper(argc, argv);
}

static int cmd_installthis(int argc, const char **argv) {
  int err;
  size_t written;
  size_t written_total = 0;
  const size_t write_size = 4096;
  if (argc != 2) {
    LOG_ERR("'%s' takes exactly 1 argument", CODE_CMD_INSTALLTHIS);
    return EINVAL;
  }
  struct DUMB_PAYLOAD *payload;
  size_t size;
  if (NULL == (payload = load_or_print_error(&size))) {
    return errno;
  }
  strncpy(payload->command, CODE_CMD_INSTALLTHIS, COMMAND_SIZE);
  const char *data_path = argv[1];
  LOG_VERBOSE("reading data from '%s'", data_path);
  FILE *data_file = fopen(data_path, "rb");
  if (data_file == NULL) {
    LOG_ERRNO("failed to open file for reading", errno);
    free(payload);
    return errno;
  }
  LOG_VERBOSE("opening '%s' for writing", path);
  FILE *dest = fopen(path, "wb");
  if (dest == NULL) {
    err = errno;
    LOG_ERRNO("failed to open file for writing", errno);
    free(payload);
    fclose(data_file);
    return err;
  }
  errno = 0;
  written = fwrite(payload, 1, sizeof(*payload), dest);
  if (written != sizeof(*payload)) {
    err = errno;
    LOG_ERRNO("error writing to file", errno);
    fclose(dest);
    fclose(data_file);
    free(payload);
    return err;
  }
  void *buf = malloc(write_size);
  if (buf == NULL) {
    err = errno;
    LOG_ERRNO("error allocating memory", err);
    fclose(dest);
    fclose(data_file);
    free(payload);
    return err;
  }
  do {
    clearerr(data_file);
    written = fread(buf, 1, write_size, data_file);
    if (ferror(data_file)) {
      LOG_ERR("error reading from file %s", data_path);
      fclose(dest);
      fclose(data_file);
      free(buf);
      free(payload);
      return EIO;
    }
    written = fwrite(buf, 1, written, dest);
    written_total += written;
  } while (!feof(data_file));
  fclose(dest);
  fclose(data_file);
  free(buf);
  free(payload);
  LOG("%zu bytes written to %s", written_total, path);
  return 0;
}
static int cmd_installpath(int argc, const char **argv) {
  if (argc != 3) {
    LOG_ERR("'%s' takes 2 arguments", CODE_CMD_INSTALL_PATH);
    return EINVAL;
  }
  if (strlen(argv[2]) != 64) {
    LOG_ERR("invalid length detected for sha256");
    return EINVAL;
  }
  const char *apk_path = argv[1];
  int apk_path_size = strlen(apk_path) + 1;
  const char *sha256 = argv[2];
  int sha256_size = strlen(sha256) + 1;
  struct DUMB_PAYLOAD *payload;
  size_t size;
  if (NULL == (payload = load_or_print_error(&size))) {
    return errno;
  }
  strncpy(payload->command, CODE_CMD_INSTALL_PATH, COMMAND_SIZE);
  size = sizeof(*payload) + apk_path_size + sha256_size;
  void *temp_buf = realloc(payload, size);
  if (temp_buf == NULL) {
    LOG_ERRNO("failed allocating memory", errno);
    free(payload);
    return errno;
  }
  payload = temp_buf;
  strcpy(payload->payload, apk_path);
  strcpy(payload->payload + apk_path_size, sha256);
  write_to_disk(payload, size);
  free(payload);
  return errno;
}
static int cmd_file_import_export(int argc, const char **argv) {
  return write_cmd_wrapper(argc, argv);
}
static int cmd_adb_toggle(int argc, const char **argv) {
  return write_cmd_wrapper(argc, argv);
}
static int cmd_wifi_toggle(int argc, const char **argv) {
  return write_cmd_wrapper(argc, argv);
}
static int cmd_oem_toggle(int argc, const char **argv) {
  return write_cmd_wrapper(argc, argv);
}
static int cmd_firewall_flush(int argc, const char **argv) {
  return write_cmd_wrapper(argc, argv);
}
static int cmd_firewall(int argc, const char **argv) {
  if (argc < 2) {
    LOG_ERR("%s requires at least 1 argument", argv[0]);
    return EINVAL;
  }
  struct DUMB_PAYLOAD *payload;
  size_t size;
  size_t total_written;
  int err;
  if (NULL == (payload = load_or_print_error(&size))) {
    return errno;
  }
  LOG_DEBUG("setting command to '%s'", argv[0]);
  snprintf(payload->command, COMMAND_SIZE, "%s", argv[0]);
  LOG_VERBOSE("writing to file '%s'", path);
  FILE *dest = fopen(path, "wb");
  if (dest == NULL) {
    LOG_ERRNO("failed to open file for writing", errno);
    return errno;
  }
  errno = 0;
  total_written = fwrite(payload, 1, sizeof(*payload), dest);
  free(payload);
  if (total_written != sizeof(*payload)) {
    err = errno;
    LOG_ERRNO("failed writing to file", err);
    fclose(dest);
    return err;
  }
  LOG_DEBUG("wrote %zu bytes", total_written);
  for (int i = 1; i < argc; i++) {
    int len = strlen(argv[i]);
    if (!check_allowed_chars(argv[i], APK_PACKAGE_ID_ALLOWED_CHARS)) {
      // don't abort. we've come too far to abort now
      LOG_WARN("invalid app id characters detected in '%s'", argv[i]);
    }
    errno = 0;
    int err = fwrite(argv[i], 1, len + 1, dest);
    if (err != len + 1) {
      err = errno;
      LOG_ERRNO("failed writing to file", err);
      fclose(dest);
      return err;
    }
    LOG_DEBUG("wrote %d bytes", err);
    total_written += err;
  }
  fclose(dest);
  LOG("%zu bytes written to %s", total_written, path);
  return 0;
}
static int cmd_composite(int argc, const char **argv) {
  LOG_ERRNO("", ENOSYS);
  return ENOSYS;
}

int cmd_command_set(int argc, const char **argv) {
  if (argc < 1) {
    LOG_FATAL("not enough arguments");
    return EINVAL;
  }
  const char *cmd = argv[0];
  if (!strcmp(cmd, CODE_CMD_OK)) {
    return cmd_ok(argc, argv);
  } else if (!strcmp(cmd, CODE_CMD_INSTALLTHIS)) {
    return cmd_installthis(argc, argv);
  } else if (!strcmp(cmd, CODE_CMD_INSTALL_PATH)) {
    return cmd_installpath(argc, argv);
  } else if (!strcmp(cmd, CODE_CMD_FILE_IMPORT) ||
             !strcmp(cmd, CODE_CMD_FILE_EXPORT)) {
    return cmd_file_import_export(argc, argv);
  } else if (!strcmp(cmd, CODE_CMD_ADB_ENABLE) ||
             !strcmp(cmd, CODE_CMD_ADB_DISABLE)) {
    return cmd_adb_toggle(argc, argv);
  } else if (!strcmp(cmd, CODE_CMD_WIFI_ENABLE) ||
             !strcmp(cmd, CODE_CMD_WIFI_DISABLE)) {
    return cmd_wifi_toggle(argc, argv);
  } else if (!strcmp(cmd, CODE_CMD_OEM_LOCK) ||
             !strcmp(cmd, CODE_CMD_OEM_UNLOCK)) {
    return cmd_oem_toggle(argc, argv);
  } else if (!strcmp(cmd, CODE_CMD_FW_FLUSH)) {
    return cmd_firewall_flush(argc, argv);
  } else if (!strcmp(cmd, CODE_CMD_FW_ALLOW) ||
             !strcmp(cmd, CODE_CMD_FW_DENY) ||
             !strcmp(cmd, CODE_CMD_FW_TEMP_ADD)) {
    return cmd_firewall(argc, argv);
  } else if (!strcmp(cmd, CODE_CMD_COMPOSITE)) {
    return cmd_composite(argc, argv);
  } else {
    LOG_FATAL("unknown command '%s'", argv[0]);
    return EINVAL;
  }
}
