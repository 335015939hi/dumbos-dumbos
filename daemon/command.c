
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../common.h"
#include "../requestid.h"
#include "command.h"
#include "util.h"

static const char *const commands_list[] = {
    "ok",         "notok",       "code",         "get_name",   "oem_lock",
#ifdef DEBUG_MODE
    "oem_unlock", "enable_wifi", "disable_wifi", "enable_adb", "disable_adb",
    "shell",
#endif
};
// the enum and commands_list must match!
enum {
  CMD_OK = 0,
  CMD_NOTOK,
  CMD_CODE,
  CMD_GET_USERNAME,
  CMD_OEM_LOCK,
#ifdef DEBUG_MODE
  CMD_OEM_UNLOCK,
  CMD_ENABLE_WIFI,
  CMD_DISABLE_WIFI,
  CMD_ENABLE_ADB,
  CMD_DISABLE_ADB,
  CMD_SHELL,
#endif
  // this must be the last one!
  CMDLIST_SIZE
} commands;

int oem_locking(int client_sockfd, bool lock);
#ifdef DEBUG_MODE
int cmd_shell(int argc, char **argv);
int toggle_wifi(int client_sockfd, bool enable);
int toggle_adb(int client_sockfd, bool enable);
#endif
int cmd_get_username(int client_sockfd);

int do_command(int argc, char **argv, int sockfd, const char *const server,
               const char *tmpdir) {
  LOG("recieved command %s", argv[0]);

  int command = -1;
  for (int i = 0; i < CMDLIST_SIZE; i++) {
    if (0 == strcmp(commands_list[i], argv[0])) {
      command = i;
      break;
    }
  }
  if (command < 0) {
    LOG_ERRNO("Bad command", EINVAL);
    write_string(sockfd, "Bad command");
    return EINVAL;
  }

  argc--;
  argv = &argv[1];

  int ret;

  switch (command) {
  case CMD_OK:
    ret = 0;
    break;
  case CMD_NOTOK:
    ret = 1;
    break;
  case CMD_CODE:
    ret = secret_code(argc, argv, sockfd, server, tmpdir);
    break;
  case CMD_GET_USERNAME:
    ret = cmd_get_username(sockfd);
    break;
  case CMD_OEM_LOCK:
    ret = oem_locking(sockfd, command == CMD_OEM_LOCK);
    break;
#ifdef DEBUG_MODE
  case CMD_SHELL:
    ret = cmd_shell(argc, argv);
    break;
  case CMD_OEM_UNLOCK:
    ret = oem_locking(sockfd, command == CMD_OEM_LOCK);
    break;
  case CMD_ENABLE_ADB:
  case CMD_DISABLE_ADB:
    ret = toggle_adb(sockfd, command == CMD_ENABLE_ADB);
    break;
  case CMD_ENABLE_WIFI:
  case CMD_DISABLE_WIFI:
    ret = toggle_wifi(sockfd, command == CMD_ENABLE_WIFI);
    break;
#endif
  default:
    LOG_ERR("Bad command code:%d", command);
    ret = EINVAL;
    break;
  }

  return ret;
}

int cmd_get_username(int sockfd) {
  struct DUMBOS_USER_DATA *userdata = dumbos_alloc_get_user();
  int err;
  if (userdata == NULL) {
    err = errno;
    LOG_ERRNO("failed to get userdata", err);
    write_string(sockfd, "internal error");
    return err;
  }
  write_string(sockfd, userdata->username);
  free(userdata);
  return 0;
}

#ifdef DEBUG_MODE
int toggle_wifi(int client_sockfd, bool enable) {
  if (enable) {
    write_string(client_sockfd, "enabling wifi");
    return set_wifi_enabled(true);
  } else {
    write_string(client_sockfd, "disabling wifi");
    return set_wifi_enabled(false);
  }
}
int toggle_adb(int client_sockfd, bool enable) {
  if (enable) {
    write_string(client_sockfd, "enabling adb");
    return set_adb_enabled(true);
  } else {
    write_string(client_sockfd, "disabling adb");
    return set_adb_enabled(false);
  }
}
int cmd_shell(int argc, char **argv) {
  pid_t child_p;
  int status;

  if (argc < 1) {
    return 0;
  }

  child_p = fork();
  if (child_p < 0) {
    LOG_ERRNO("bad fork", errno);
    return errno;
  }

  if (child_p == 0) { // child
    execvp(argv[0], argv);
    LOG_ERRNO("bad execve", errno);
    _exit(127);
  } else {
    if (0 > waitpid(child_p, &status, 0))
      LOG_ERRNO("bad wait", errno);
  }
  return WEXITSTATUS(status);
}
#endif
int oem_locking(int client_sockfd, bool lock) {
  if (!lock) {
    write_string(client_sockfd, "enabling OEM unlock");
    return set_oem_lock(false);
  } else {
    write_string(client_sockfd, "disabling OEM unlock");
    return set_oem_lock(true);
  }
}
