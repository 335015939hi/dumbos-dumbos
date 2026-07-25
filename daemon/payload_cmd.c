
#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "../common.h"
#include "command.h"
#include "util.h"

// commmand handlers.
// return a malloc'ed string for info. put return status into *ret_val.
#ifdef DEBUG_MODE
int payload_cmd_shell(void *script, size_t script_size, int sockfd) {
  int err;
  if (script_size == 0) {
    LOG("empty script. exiting");
    return 0;
  }
  // sanity check:force NULL terminate
  ((char *)script)[script_size - 1] = '\0';
  LOG("cmd_shell()");
  LOG("running '%s'", (char *)script);
  err = system(script);
  if (err < 0) {
    err = errno;
    LOG_ERRNO("failed to execute system()", err);
    write_string(sockfd, "internal error:system()");
    return err;
  }
  if (WIFEXITED(err)) {
    err = WEXITSTATUS(err);
    LOG("script exited with %d:%s", err, strerror(err));
    return err;
  } else {
    LOG_ERR("script exited with unknown error:%d", err);
    write_string(sockfd, "internal error");
    return err;
  }
}

#endif // DEBUG_MODE
int payload_cmd_install_this(void *apk, size_t apk_size, int sockfd,
                             const char *tmpdir) {
  LOG_DEBUG("cmd_install_file()");
  int err;
  int fd;
  LOG_DEBUG("cmd_install_file() apk_size=%ld", apk_size);
  char *path = malloc(PATH_MAX);
  if (path == NULL) {
    err = errno;
    LOG_ERRNO("failed to alloc", err);
    write_string(sockfd, "Out of memory");
    return err;
  }

  err = snprintf(path, PATH_MAX, "%s%s", tmpdir, "tmp.apk");
  if (err < 0) {
    err = errno;
    LOG_ERRNO("snprintf() failed", err);
    free(path);
    write_string(sockfd, "Internal error");
    return err;
  } else if (err >= PATH_MAX) {
    LOG_ERR("path too long");
    free(path);
    write_string(sockfd, "filename too long");
    return ENAMETOOLONG;
  }
  LOG_DEBUG("writing payload to %s", path);

  fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if (fd < 0) {
    err = errno;
    LOG_ERRNO("open() failed", err);
    free(path);
    write_string(sockfd, "internal error");
    return err;
  }
  LOG_DEBUG("fd=%d", fd);

  err = write_all(fd, apk, apk_size);
  if (err < 0) {
    err = errno;
    LOG_ERRNO("write_all() failed", err);
    free(path);
    close(fd);
    unlink(path);
    write_string(sockfd, "internal Error");
    return err;
  }
  close(fd);

  pid_t pid = fork();
  if (pid < 0) {
    err = errno;
    LOG_ERRNO("fork error", errno);
    unlink(path);
    write_string(sockfd, "internal error:fork");
    return err;
  }

  if (pid == 0) {
    execlp("pm", "pm", "install", path, (char *)NULL);
    _exit(127);
  }
  LOG_DEBUG("forked to pid %d, execlp %s %s %s", pid, "pm", "install", path);

  int status;
  if (waitpid(pid, &status, 0) < 0) {
    err = errno;
    LOG_ERRNO("waitpid error", errno);
    unlink(path);
    write_string(sockfd, "internal error:waitpid");
    return err;
  }

  if (WIFEXITED(status)) {
    LOG("exited with %d", WEXITSTATUS(status));
    unlink(path);
    err = WEXITSTATUS(status);
    write_string(sockfd, "done. ");
    return err;
  }

  write_string(sockfd, "pm did not exit normally");
  unlink(path);
  return ECHILD;
}

int payload_cmd_install_path(const char *strings, size_t size, int sockfd) {
  return ENOSYS;
}
int payload_cmd_set_adb_enabled(bool enabled) {
  LOG("%s adb", enabled ? "enabling" : "disabling");
  return set_adb_enabled(enabled);
}
int payload_cmd_set_wifi_enabled(bool enabled) {
  LOG("%s wifi", enabled ? "enabling" : "disabling");
  return set_wifi_enabled(enabled);
}
int payload_cmd_set_oem_unlock_enabled(bool enabled) {
  LOG("%s OEM unlock", enabled ? "enabling" : "disabling");
  return set_oem_lock(!enabled);
}
int payload_cmd_composite(void *data, size_t size, const char *tmpdir,
                          time_t time, int sockfd) {
  if (size < sizeof(short)) {
    LOG_ERR("composite payload: invalid size detected");
    write_string(sockfd, "invalid");
    return EINVAL;
  }
  unsigned short payload_count = (unsigned short)ntohs(*(short *)data);
  LOG("composite payload: counted %hd payloads", payload_count);
  // TODO:
  return ENOSYS;
}

const char *const sdcard_export_source = "/sdcard/export";
const char *const sdcard_import_dest = "/sdcard/import-%lld";
const char *const external_mountpoint = "/tmp/";
const char *const external_export_dest = "%sexport-%lld";
const char *const external_import_source = external_mountpoint;
const char *const external_blockdev = "/dev/block/sde1";
const char *const external_fstype = "exfat";

int payload_cmd_files_import(int sockfd) {
  char *import_path;
  int err;
  if (asprintf(&import_path, sdcard_import_dest, (long long)time(NULL)) < 0) {
    err = errno;
    LOG_ERRNO("asprintf failed", err);
    write_string(sockfd, "internal error");
    return err;
  }
  LOG("importing files from external drive to '%s'", import_path);
  err =
      mount_copy_unmount_ns(external_blockdev, external_mountpoint,
                            external_fstype, MS_NODEV | MS_NOSUID | MS_NOEXEC,
                            NULL, external_import_source, import_path, sockfd);
  free(import_path);
  if (err != 0) {
    err = errno;
    LOG_ERRNO("copy failed", err);
    write_string(sockfd, "copying failed");
    write_string(sockfd, strerror(err));
    return err;
  }
  write_string(sockfd, "done");
  return 0;
}
int payload_cmd_files_export(int sockfd) {
  char *export_path;
  int err;
  if (asprintf(&export_path, external_export_dest, external_mountpoint,
               (long long)time(NULL)) < 0) {
    err = errno;
    LOG_ERRNO("asprintf failed", errno);
    write_string(sockfd, "internal error");
    return err;
  }
  LOG("exporting files to '%s'", export_path);
  err = mount_copy_unmount_ns(external_blockdev, external_mountpoint,
                              external_fstype, MS_NODEV | MS_NOSUID | MS_NOEXEC,
                              "fmask=00117,dmask=00007,uid=0,gid=0",
                              sdcard_export_source, export_path, sockfd);
  free(export_path);
  if (err != 0) {
    err = errno;
    LOG_ERRNO("copy failed", err);
    write_string(sockfd, "copying failed");
    write_string(sockfd, strerror(err));
    if (err == ENOENT) {
      char *msg;
      // TODO:check for asprintf fail
      asprintf(
          &msg,
          "make sure '%s' exists! that should be the location of all the files "
          "to export (or possibly your USB is unplugged or not detected)",
          sdcard_export_source);
      write_string(sockfd, msg);
      free(msg);
    }
    return err;
  }
  write_string(sockfd, "done");
  return 0;
}

enum FIREWALL_POLICY {
  FIREWALL_POLICY_ALLOW,
  FIREWALL_POLICY_DENY,
  FIREWALL_POLICY_ALLOW_TEMP,
};

static int firewall_helper(int sockfd, enum FIREWALL_POLICY policy, char *data,
                           size_t size) {
  LOG_DEBUG("firewall_helper()");
  if (size == 0) {
    LOG_VERBOSE("firewall_helper(): empty data");
    return 0;
  }
  if (*data == '\0' || size <= 1) {
    LOG_ERR("firewall_helper(): malformed data");
    return EINVAL;
  }
  char *data_end = data + size;
  int str_num = 0;
  char **str_list = NULL;
  // safety:force last byte to be NULL
  *data_end = '\0';
  while (data < data_end) {
    str_num++;
    // why +3: we're going to pass this directly to execve, so leave 2 spaces
    // for argv[0] and argv[1], and 1 space at the end for NULL string
    str_list = reallocarray(str_list, str_num + 3, sizeof(char *));
    // skip 2 spaces for argv[0] and argv[1]
    str_list[str_num + 1] = data;
    LOG_DEBUG("found app id '%s'", data);
    data += strlen(data) + 1;
  }
  str_list[0] = "/system/bin/dumbos-firewallctl.sh";
  if (policy == FIREWALL_POLICY_ALLOW) {
    str_list[1] = "allow";
  } else if (policy == FIREWALL_POLICY_DENY) {
    str_list[1] = "deny";
  } else {
    str_list[1] = "allow-temp";
  }
  str_list[str_num] = NULL;

  int err;
  pid_t pid = fork();
  if (pid < 0) {
    err = errno;
    free(str_list);
    LOG_ERRNO("firewall_helper(): failed to fork", err);
    write_string(sockfd, "internal error");
    return err;
  }

  if (pid == 0) {
    LOG_DEBUG("firewall_helper() child");
    LOG_DEBUG("execve %s", str_list[0]);
    execve(str_list[0], str_list, NULL);
    // if execve succeeded wo would not reach here
    err = errno;
    free(str_list);
    LOG_ERRNO("execve fail", err);
    write_string(sockfd, "internal error");
    return err;
  } else {
    free(str_list);
    LOG_DEBUG("firewall_helper():forked. chiled pid=%d", pid);
    int status;
    err = waitpid(pid, &status, 0);
    if (err < 0) {
      err = errno;
      LOG_ERRNO("waitpid failed", err);
      write_string(sockfd, "internal error");
      return err;
    }
    if (WIFEXITED(status)) {
      status = WEXITSTATUS(status);
      if (status == 0) {
        LOG("done");
        return 0;
      } else {
        LOG_ERRNO("child failed", status);
        write_string(sockfd, "internal error");
        return status;
      }
    } else {
      LOG_ERR("child exited abnormally");
      write_string(sockfd, "internal error");
      return ECHILD;
    }
    return 0;
  }
}
int payload_cmd_firewall_flush() { return ENOSYS; }
int payload_cmd_firewall_add(int sockfd, void *data, size_t size) {
  return firewall_helper(sockfd, FIREWALL_POLICY_ALLOW, data, size);
}
int payload_cmd_firewall_remove(int sockfd, void *data, size_t size) {
  return firewall_helper(sockfd, FIREWALL_POLICY_DENY, data, size);
}
int payload_cmd_firewall_add_temp(int sockfd, void *data, size_t size) {
  return firewall_helper(sockfd, FIREWALL_POLICY_ALLOW_TEMP, data, size);
}
