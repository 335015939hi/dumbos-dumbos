
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
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
