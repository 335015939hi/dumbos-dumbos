
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
#include "exec_wrapper.h"
#include "util.h"

// location to write to for init to copy to persist location
// see also dumbosd.rc
#define DUMBOS_USER_DATA_NEW "/dev/dumbos-userdata-new"
// system property to write to to trigger init, see above
// see also dumbosd.rc
#define DUMBOS_USER_DATA_NEW_PROP "dumbos.newuser"
#define DUMBOS_USER_DATA_NEW_PROP_VAL "1"

static const char *const commands_list[] = {
    "ok",          "notok",       "code",         "get_name",
    "oem_lock",    "version",     "set_name",
#ifdef DEBUG_MODE
    "oem_unlock",  "enable_wifi", "disable_wifi", "enable_adb",
    "disable_adb", "shell",
#endif
};
// the enum and commands_list must match!
enum {
  CMD_OK = 0,
  CMD_NOTOK,
  CMD_CODE,
  CMD_GET_USERNAME,
  CMD_OEM_LOCK,
  CMD_VERSION,
  CMD_SETNAME,
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
int cmd_version(int sockfd);
// sets the DumbOS user data, only if it's not already set. this integrates with
// dumbosd.rc, because getting dumbosd r/w on /mnt/vendor/persist requires
// waging war on selinux
int cmd_setname(int sockfd, int argc, char **argv);

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
    ret = oem_locking(sockfd, true);
    break;
  case CMD_VERSION:
    ret = cmd_version(sockfd);
    break;
#ifdef DEBUG_MODE
  case CMD_SHELL:
    ret = cmd_shell(argc, argv);
    break;
  case CMD_OEM_UNLOCK:
    ret = oem_locking(sockfd, false);
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
  case CMD_SETNAME:
    ret = cmd_setname(sockfd, argc, argv);
    break;
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

int cmd_setname(int sockfd, int argc, char **argv) {
  int err;
  LOG_DEBUG("cmd_setname()");
  if (argc < 1) {
    LOG_ERR("cmd_setname(): not enough arguments");
    write_string(sockfd, "not enough data provided");
    return EINVAL;
  }
  char *data = malloc_hex_to_buf(argv[0]);
  if (data == NULL) {
    LOG_ERRNO("malloc_hex_to_buf() fail", errno);
    if (errno == EINVAL) {
      write_string(sockfd, "bad data");
      return errno;
    } else {
      write_string(sockfd, "internal error");
      return errno;
    }
  }

  errno = 0;
  void *olduserdata = dumbos_alloc_get_user();
  if (olduserdata != NULL || errno != ENOENT) {
    LOG_ERR("a user already exists");
    write_string(sockfd, "a user has already been set");
    maybe_free(olduserdata);
#ifndef DEBUG_MODE
    free(data);
    return EUSERS;
#else
    LOG_ERR("We are in debug mode. continuing anyway");
    write_string(sockfd, "We are in debug mode. continuing anyway");
#endif
  }

  LOG_DEBUG("opening file %s for writing", DUMBOS_USER_DATA_NEW);
  FILE *destfile = fopen(DUMBOS_USER_DATA_NEW, "wb");
  if (destfile == NULL) {
    LOG_ERRNO("open file failed", errno);
    free(data);
    return errno;
  }
  // malloc_hex_to_buf() should already gurantee that strlen(argv[0]) is
  // divisible by 2
  size_t datasize = strlen(argv[0]) / 2;
  if (datasize != fwrite(data, 1, datasize, destfile)) {
    err = errno;
    fclose(destfile);
    free(data);
    LOG("failed writing to file");
    write_string(sockfd, "internal error");
    return err;
  }
  fclose(destfile);
  free(data);

  LOG_DEBUG("calling setprop");
  err = execv_wrapper("/system/bin/setprop",
                      (char *const[]){"/system/bin/setprop",
                                      DUMBOS_USER_DATA_NEW_PROP,
                                      DUMBOS_USER_DATA_NEW_PROP_VAL, NULL});
  if (err < 0) {
    LOG_ERRNO("execv_wrapper() failed", errno);
    write_string(sockfd, "internal error");
    return errno;
  } else if (err != 0) {
    LOG_ERRNO("setprop failed", err);
    write_string(sockfd, "internal error");
    return err;
  }

  write_string(sockfd, "done.");

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
int cmd_version(int sockfd) {
  write_string(sockfd, VERSION_STRING);
  return 0;
}
