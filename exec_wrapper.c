
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "exec_wrapper.h"

/*
 * Executes path in a child process and waits for it.
 *
 * Returns:
 *   >= 0  - child exit status, or 128 + signal number if killed by signal
 *   -1    - fork(), waitpid(), pipe(), or communication failure
 *
 * If execv() fails, errno is set to the error from execv().
 */
int execv_wrapper(const char *path, char *const argv[]) {
  int exec_pipe[2];

  if (pipe(exec_pipe) < 0)
    return -1;

  /*
   * The write end is automatically closed when execv() succeeds.
   * Therefore, data arriving on the pipe means execv() failed.
   */
  if (fcntl(exec_pipe[1], F_SETFD, FD_CLOEXEC) < 0) {
    int saved_errno = errno;
    close(exec_pipe[0]);
    close(exec_pipe[1]);
    errno = saved_errno;
    return -1;
  }

  pid_t pid = fork();

  if (pid < 0) {
    int saved_errno = errno;
    close(exec_pipe[0]);
    close(exec_pipe[1]);
    errno = saved_errno;
    return -1;
  }

  if (pid == 0) {
    close(exec_pipe[0]);

    execv(path, argv);

    /*
     * execv() failed. Save errno and communicate it to the parent.
     *
     * write() is async-signal-safe, unlike stdio functions, so it
     * is safe to use after fork().
     */
    int exec_errno = errno;

    ssize_t written = write(exec_pipe[1], &exec_errno, sizeof(exec_errno));

    (void)written;
    _exit(127);
  }

  close(exec_pipe[1]);

  /*
   * If execv() fails, the child writes errno to the pipe.
   * If execv() succeeds, FD_CLOEXEC closes the pipe during exec,
   * producing EOF here.
   */
  int exec_errno = 0;
  ssize_t n;

  do {
    n = read(exec_pipe[0], &exec_errno, sizeof(exec_errno));
  } while (n < 0 && errno == EINTR);

  int read_errno = errno;

  close(exec_pipe[0]);

  if (n < 0) {
    /*
     * We have to reap the child before returning, otherwise it
     * could become a zombie.
     */
    int status;
    while (waitpid(pid, &status, 0) < 0) {
      if (errno != EINTR)
        break;
    }

    errno = read_errno;
    return -1;
  }

  if (n != 0 && n != (ssize_t)sizeof(exec_errno)) {
    /*
     * The child should either send the complete errno value or
     * send nothing because exec succeeded.
     */
    int status;
    while (waitpid(pid, &status, 0) < 0) {
      if (errno != EINTR)
        break;
    }

    errno = EIO;
    return -1;
  }

  if (n == (ssize_t)sizeof(exec_errno)) {
    /*
     * execv() failed. Reap the child, then report the original
     * execv() errno to the caller.
     */
    int status;

    while (waitpid(pid, &status, 0) < 0) {
      if (errno != EINTR) {
        int saved_errno = errno;
        errno = saved_errno;
        return -1;
      }
    }

    errno = exec_errno;
    return -1;
  }

  /*
   * EOF means execv() succeeded and the child is now running the
   * requested program.
   */
  int status;

  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR)
      return -1;
  }

  if (WIFEXITED(status))
    return WEXITSTATUS(status);

  if (WIFSIGNALED(status))
    return 128 + WTERMSIG(status);

  /*
   * With waitpid(..., 0), this should not normally be reachable.
   */
  errno = ECHILD;
  return -1;
}
