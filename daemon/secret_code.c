
#include <arpa/inet.h>
#include <curl/curl.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/limits.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../common.h"
#include "../dumb.h"
#include "../requestid.h"
#include "command.h"
#include "curl.h"
#include "util.h"

// this header file should define static const char * const
// _ed25519_public_key_hex as a 64 character +1 byte NULL (32 decoded bytes)
// hexadecimal string that contained the public ed25519 public key used for
// verification
#include "../keys/key_public.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define streq(a, b) (!strcmp(a, b))

int handle_one_payload(int sockfd, struct DUMB_PAYLOAD *payload, time_t time,
                       size_t payload_size, const char *tmpdir) {
  int err;

  if (dp_is_expired_compare(payload, time)) {
    LOG_ERR("code expired");
    write_string(sockfd, "code expired");
    return EKEYEXPIRED;
  }

  if (0 != dp_verify(payload, payload_size + sizeof(*payload),
                     _ed25519_public_key_hex)) {
    LOG_ERRNO("invalid signature", errno);
    err = errno;
    write_string(sockfd, "invalid signature");
    return err;
  }
  LOG("verified");
  write_string(sockfd, "loading...");

  // sanity: force NULL terminate command
  payload->command[COMMAND_SIZE - 1] = '\0';
  LOG("command=%s", payload->command);

  write_string(sockfd, "command is");
  write_string(sockfd, payload->command);

  const char *cmd = payload->command;
#ifdef DEBUG_MODE
  if (streq(cmd, CODE_CMD_SHELL)) {
    err = payload_cmd_shell(payload->payload, payload_size, sockfd);
  } else
#endif
      if (streq(cmd, CODE_CMD_OK)) {
    LOG("nothing happens");
    write_string(sockfd, "ok");
    err = 0;
  } else if (streq(cmd, CODE_CMD_INSTALLTHIS)) {
    err = payload_cmd_install_this(payload->payload, payload_size, sockfd,
                                   tmpdir);
  } else if (streq(cmd, CODE_CMD_INSTALL_PATH)) {
    err = payload_cmd_install_path(payload->payload, payload_size, sockfd);
  } else if (streq(cmd, CODE_CMD_FILE_EXPORT)) {
    err = payload_cmd_files_export(sockfd);
  } else if (streq(cmd, CODE_CMD_FILE_IMPORT)) {
    err = payload_cmd_files_import(sockfd);
  } else if (streq(cmd, CODE_CMD_ADB_ENABLE)) {
    err = payload_cmd_set_adb_enabled(true);
  } else if (streq(cmd, CODE_CMD_ADB_DISABLE)) {
    err = payload_cmd_set_adb_enabled(false);
  } else if (streq(cmd, CODE_CMD_WIFI_ENABLE)) {
    err = payload_cmd_set_wifi_enabled(true);
  } else if (streq(cmd, CODE_CMD_WIFI_DISABLE)) {
    err = payload_cmd_set_wifi_enabled(false);
  } else if (streq(cmd, CODE_CMD_OEM_UNLOCK)) {
    err = payload_cmd_set_oem_unlock_enabled(true);
  } else if (streq(cmd, CODE_CMD_OEM_LOCK)) {
    err = payload_cmd_set_oem_unlock_enabled(false);
  } else if (streq(cmd, CODE_CMD_COMPOSITE)) {
    err = payload_cmd_composite(payload->payload, payload_size, tmpdir, time,
                                sockfd);
  } else if (streq(cmd, CODE_CMD_FW_ALLOW)) {
    err = payload_cmd_firewall_add(sockfd, payload->payload, payload_size);
  } else if (streq(cmd, CODE_CMD_FW_DENY)) {
    err = payload_cmd_firewall_remove(sockfd, payload->payload, payload_size);
  } else if (streq(cmd, CODE_CMD_FW_TEMP_ADD)) {
    err = payload_cmd_firewall_add_temp(sockfd, payload->payload, payload_size);
  } else if (streq(cmd, CODE_CMD_FW_FLUSH)) {
    err = payload_cmd_firewall_flush();
  }
  // TODO:implement other commands

  else {
    LOG_ERR("unknown command:%s", payload->command);
    err = ENOSYS;
    write_string(sockfd, "unknown command:");
    write_string(sockfd, payload->command);
  }

  return err;
}

int secret_code(int argc, char **argv, int sockfd, const char *const host,
                const char *const tmpdir) {
  size_t secret_len_max;
  struct DUMB_PAYLOAD *payload = NULL;
  size_t payload_size;
  int err;
  char *net_time_str = NULL;
  long long net_time;

  if (argc != 1) {
    LOG_FATAL("secret_code: expected exactly ONE code");
    write_string(sockfd, "secret_code: expected exactly ONE code\n");
    return EINVAL;
  }

  // safety:set umask
  umask(0177);

  secret_len_max = PATH_MAX - (strlen(tmpdir) + 1 // NULL terminator
                              );
  LOG_VERBOSE("secret code maximum length is %zu", secret_len_max - 1);
  if (strlen(argv[0]) > secret_len_max) {
    LOG_FATAL("secret code '%s' too long", argv[0]);
    write_string(sockfd, "secret_code: secret too long");
    return ENAMETOOLONG;
  }

  // get time from network (dont trust system time)
  // save and use that time to calculate expiry (in case download takes a long
  // time and it expires meanwhile)
  LOG_DEBUG("getting time from network");
  net_time_str = geturltime();
  if (NULL == net_time_str) {
    write_string(sockfd,
                 "failed to get time from network (check your connection?)");
    return errno;
  }
  if (parse_long_long(net_time_str, &net_time) != 0) {
    LOG_ERRNO("invalid time from server", errno);
    write_string(sockfd,
                 "server returned invalid time (check your connection?)");
    free(net_time_str);
    return EPROTO;
  }
  free(net_time_str);
  LOG("network time is %lld", net_time);

  char *url = NULL;
  struct DUMBOS_USER_DATA *userdata = dumbos_alloc_get_user();
  const char *requestid = request_id_generate();
  char *request_signature = malloc(ED25519_SIGNATURE_HEX_SIZE);
  err = request_id_sign(request_signature, userdata->username, requestid,
                        userdata->priv_key_hex);
  if (err != 0) {
    LOG_ERR("request_id_sign() failed", err);
    free(request_signature);
    free(userdata);
    return err;
  }

  err = asprintf(&url, "%s?code=%s&user=%s&requestid=%s&requestsig=%s", host,
                 argv[0], userdata->username, requestid, request_signature);
  free(request_signature);
  free(userdata);
  if (err < 0) {
    err = errno;
    LOG_ERR("asprintf() failed", err);
    return err;
  }
  free(request_signature);
  free(userdata);

  LOG("Downloading from %s", url);
  write_string(sockfd, "downloading...");

  payload = download_url(url, &payload_size, NULL);
  if (payload == NULL) {
    err = errno;
    free(url);
    LOG_ERRNO("failed to download payload from server", errno);
    if (errno == ECONNREFUSED) {
      write_string(sockfd, "check your connection");
    } else if (errno == EPROTO) {
      write_string(sockfd, "invalid code");
    } else {
      write_string(sockfd,
                   "failed to download, either bad code or bad connection");
    }
    return err;
  }
  free(url);

  if (payload_size < sizeof(struct DUMB_PAYLOAD)) {
    LOG_ERR("bad payload size, expected >=%zu, got %zu",
            sizeof(struct DUMB_PAYLOAD), payload_size);
    free(payload);
    write_string(sockfd, "invalid");
    return EINVAL;
  }

  payload_size -= sizeof(*payload);

  err = handle_one_payload(sockfd, payload, net_time, payload_size, tmpdir);

  free(payload);
  return err;
}
