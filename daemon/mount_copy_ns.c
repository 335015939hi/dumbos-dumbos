#define _GNU_SOURCE

// chatgpt code, mostly

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "../common.h"

#define CLONE_STACK_SIZE (1024U * 1024U)
#define COPY_BUFFER_SIZE (1024U * 1024U)

static int dumbos_client_socketfd;

struct copy_progress {
  uint64_t done;
  uint64_t total;
  struct timespec last_print;
};

struct child_status {
  int rc;
  int err;
};

struct child_args {
  const char *device;
  const char *mountpoint;
  const char *fstype;
  unsigned long mount_flags;
  const char *mount_data;
  const char *copy_src;
  const char *copy_dst;
  int status_read_fd;
  int status_write_fd;
};

static int chatgpt_write_all(int fd, const void *buf, size_t len) {
  const unsigned char *p = buf;

  while (len != 0) {
    ssize_t n = write(fd, p, len);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    p += (size_t)n;
    len -= (size_t)n;
  }

  return 0;
}

static int chatgpt_read_all(int fd, void *buf, size_t len) {
  unsigned char *p = buf;

  while (len != 0) {
    ssize_t n = read(fd, p, len);
    if (n == 0) {
      errno = EIO;
      return -1;
    }
    if (n < 0) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    p += (size_t)n;
    len -= (size_t)n;
  }

  return 0;
}

static char *join_path(const char *parent, const char *name) {
  char *result = NULL;

  if (asprintf(&result, "%s%s%s", parent,
               parent[0] != '\0' && parent[strlen(parent) - 1] == '/' ? ""
                                                                      : "/",
               name) < 0) {
    return NULL;
  }

  return result;
}

static int measure_tree(const char *path, uint64_t *total) {
  struct stat st;

  if (lstat(path, &st) < 0)
    return -1;

  if (S_ISREG(st.st_mode)) {
    uint64_t size = (uint64_t)st.st_size;
    if (UINT64_MAX - *total < size) {
      errno = EOVERFLOW;
      return -1;
    }
    *total += size;
    return 0;
  }

  if (S_ISLNK(st.st_mode))
    return 0;

  if (!S_ISDIR(st.st_mode)) {
    errno = ENOTSUP;
    return -1;
  }

  DIR *dir = opendir(path);
  if (dir == NULL)
    return -1;

  int rc = 0;
  int saved_errno = 0;

  for (;;) {
    errno = 0;
    struct dirent *entry = readdir(dir);
    if (entry == NULL) {
      if (errno != 0) {
        rc = -1;
        saved_errno = errno;
      }
      break;
    }

    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    char *child = join_path(path, entry->d_name);
    if (child == NULL) {
      rc = -1;
      saved_errno = errno;
      break;
    }

    if (measure_tree(child, total) < 0) {
      rc = -1;
      saved_errno = errno;
      free(child);
      break;
    }

    free(child);
  }

  if (closedir(dir) < 0 && rc == 0) {
    rc = -1;
    saved_errno = errno;
  }

  if (rc < 0)
    errno = saved_errno;
  return rc;
}

static void print_progress(struct copy_progress *progress, int force) {
  struct timespec now;

  if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
    return;

  long elapsed_ms = (now.tv_sec - progress->last_print.tv_sec) * 1000L +
                    (now.tv_nsec - progress->last_print.tv_nsec) / 1000000L;

  if (!force && elapsed_ms < 100)
    return;

  progress->last_print = now;

  char *progress_str;

  if (progress->total != 0) {
    double percent = 100.0 * (double)progress->done / (double)progress->total;
    asprintf(&progress_str, "\rCopied %.2f MiB / %.2f MiB (%5.1f%%)",
             (double)progress->done / (1024.0 * 1024.0),
             (double)progress->total / (1024.0 * 1024.0), percent);
  } else {
    asprintf(&progress_str, "\rCopied %.2f MiB",
             (double)progress->done / (1024.0 * 1024.0));
  }
  LOG("%s", progress_str);
  write_string(dumbos_client_socketfd, progress_str);
}

static int copy_regular_file(const char *src, const char *dst,
                             const struct stat *src_st,
                             struct copy_progress *progress) {
  int in_fd = -1;
  int out_fd = -1;
  void *buffer = NULL;
  int rc = -1;
  int saved_errno = 0;

  in_fd = open(src, O_RDONLY | O_CLOEXEC);
  if (in_fd < 0)
    goto out;

  out_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW,
                src_st->st_mode & 07777);
  if (out_fd < 0 && errno == ELOOP) {
    if (unlink(dst) < 0)
      goto out;
    out_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW,
                  src_st->st_mode & 07777);
  }
  if (out_fd < 0)
    goto out;

  buffer = malloc(COPY_BUFFER_SIZE);
  if (buffer == NULL)
    goto out;

  for (;;) {
    ssize_t nread = read(in_fd, buffer, COPY_BUFFER_SIZE);
    if (nread == 0)
      break;
    if (nread < 0) {
      if (errno == EINTR)
        continue;
      goto out;
    }

    if (chatgpt_write_all(out_fd, buffer, (size_t)nread) < 0)
      goto out;

    progress->done += (uint64_t)nread;
    print_progress(progress, 0);
  }

  if (fchmod(out_fd, src_st->st_mode & 07777) < 0)
    goto out;

  rc = 0;

out:
  saved_errno = errno;
  free(buffer);
  if (out_fd >= 0 && close(out_fd) < 0 && rc == 0) {
    rc = -1;
    saved_errno = errno;
  }
  if (in_fd >= 0 && close(in_fd) < 0 && rc == 0) {
    rc = -1;
    saved_errno = errno;
  }
  if (rc < 0)
    errno = saved_errno;
  return rc;
}

static int copy_symlink(const char *src, const char *dst,
                        const struct stat *st) {
  size_t size = st->st_size > 0 ? (size_t)st->st_size + 1U : 256U;
  char *target = NULL;

  for (;;) {
    char *new_target = realloc(target, size);
    if (new_target == NULL) {
      free(target);
      return -1;
    }
    target = new_target;

    ssize_t n = readlink(src, target, size);
    if (n < 0) {
      free(target);
      return -1;
    }
    if ((size_t)n < size) {
      target[n] = '\0';
      break;
    }

    if (size > SIZE_MAX / 2U) {
      free(target);
      errno = EOVERFLOW;
      return -1;
    }
    size *= 2U;
  }

  if (unlink(dst) < 0 && errno != ENOENT) {
    free(target);
    return -1;
  }

  int rc = symlink(target, dst);
  int saved_errno = errno;
  free(target);
  errno = saved_errno;
  return rc;
}

static int copy_tree(const char *src, const char *dst,
                     struct copy_progress *progress) {
  struct stat st;

  if (lstat(src, &st) < 0)
    return -1;

  if (S_ISREG(st.st_mode))
    return copy_regular_file(src, dst, &st, progress);

  if (S_ISLNK(st.st_mode))
    return copy_symlink(src, dst, &st);

  if (!S_ISDIR(st.st_mode)) {
    errno = ENOTSUP;
    return -1;
  }

  if (mkdir(dst, st.st_mode & 07777) < 0) {
    if (errno != EEXIST)
      return -1;

    struct stat dst_st;
    if (lstat(dst, &dst_st) < 0)
      return -1;
    if (!S_ISDIR(dst_st.st_mode)) {
      errno = ENOTDIR;
      return -1;
    }
  }

  DIR *dir = opendir(src);
  if (dir == NULL)
    return -1;

  int rc = 0;
  int saved_errno = 0;

  for (;;) {
    errno = 0;
    struct dirent *entry = readdir(dir);
    if (entry == NULL) {
      if (errno != 0) {
        rc = -1;
        saved_errno = errno;
      }
      break;
    }

    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    char *src_child = join_path(src, entry->d_name);
    char *dst_child = join_path(dst, entry->d_name);
    if (src_child == NULL || dst_child == NULL) {
      rc = -1;
      saved_errno = errno;
      free(src_child);
      free(dst_child);
      break;
    }

    if (copy_tree(src_child, dst_child, progress) < 0) {
      rc = -1;
      saved_errno = errno;
      free(src_child);
      free(dst_child);
      break;
    }

    free(src_child);
    free(dst_child);
  }

  if (closedir(dir) < 0 && rc == 0) {
    rc = -1;
    saved_errno = errno;
  }

  if (rc == 0 && chmod(dst, st.st_mode & 07777) < 0) {
    rc = -1;
    saved_errno = errno;
  }

  if (rc < 0)
    errno = saved_errno;
  return rc;
}

static int sync_destination_filesystem(const char *dst) {
  int fd = open(dst, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
  if (fd < 0)
    return -1;

  int rc = syncfs(fd);
  int saved_errno = errno;
  if (close(fd) < 0 && rc == 0) {
    rc = -1;
    saved_errno = errno;
  }

  if (rc < 0)
    errno = saved_errno;
  return rc;
}

static void send_child_status(int fd, int rc, int err) {
  struct child_status status = {
      .rc = rc,
      .err = err,
  };

  (void)chatgpt_write_all(fd, &status, sizeof(status));
}

static int child_main(void *opaque) {
  const struct child_args *args = opaque;
  int mounted = 0;
  int rc = -1;
  int saved_errno = 0;

  close(args->status_read_fd);

  if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) < 0) {
    saved_errno = errno;
    LOG_ERRNO("making mount propagation private failed", saved_errno);
    goto out;
  }

  if (mount(args->device, args->mountpoint, args->fstype, args->mount_flags,
            args->mount_data) < 0) {
    saved_errno = errno;
    LOG_ERR("mount(%s, %s) failed:%s\n", args->device, args->mountpoint,
            strerror(saved_errno));
    goto out;
  }
  mounted = 1;

  struct copy_progress progress = {0};
  if (clock_gettime(CLOCK_MONOTONIC, &progress.last_print) < 0) {
    saved_errno = errno;
    goto out;
  }

  if (measure_tree(args->copy_src, &progress.total) < 0) {
    saved_errno = errno;
    LOG_ERR("measuring %s failed: %s\n", args->copy_src, strerror(saved_errno));
    goto out;
  }

  print_progress(&progress, 1);

  if (copy_tree(args->copy_src, args->copy_dst, &progress) < 0) {
    saved_errno = errno;
    LOG_ERR("copy %s -> %s failed: %s\n", args->copy_src, args->copy_dst,
            strerror(saved_errno));
    goto out;
  }

  print_progress(&progress, 1);

  if (sync_destination_filesystem(args->copy_dst) < 0) {
    saved_errno = errno;
    LOG_ERR("syncfs(%s) failed: %s\n", args->copy_dst, strerror(saved_errno));
  }

  rc = 0;

out:
  if (mounted) {
    if (umount2(args->mountpoint, 0) < 0) {
      int unmount_errno = errno;
      LOG_ERR("umount(%s) failed: %s\n", args->mountpoint,
              strerror(unmount_errno));
      if (rc == 0) {
        rc = -1;
        saved_errno = unmount_errno;
      }
    }
  }

  if (rc == 0)
    send_child_status(args->status_write_fd, 0, 0);
  else
    send_child_status(args->status_write_fd, -1,
                      saved_errno != 0 ? saved_errno : EIO);

  close(args->status_write_fd);
  return rc == 0 ? 0 : 1;
}

/*
 * Mount device at mountpoint inside a new mount namespace, copy copy_src to
 * copy_dst, sync the destination filesystem, unmount, and wait for completion.
 *
 * Returns 0 on success. Returns -1 on failure and sets errno to the child's
 * reported error where possible.
 */
int mount_copy_unmount_ns(const char *device, const char *mountpoint,
                          const char *fstype, unsigned long mount_flags,
                          const char *mount_data, const char *copy_src,
                          const char *copy_dst, int _dumbos_client_socketfd) {
  dumbos_client_socketfd = _dumbos_client_socketfd;
  LOG_DEBUG("mount_copy_unmount_ns");
  if (device == NULL || mountpoint == NULL || fstype == NULL ||
      copy_src == NULL || copy_dst == NULL) {
    LOG_ERR("mount_copy_unmount_ns:invalid arguments");
    errno = EINVAL;
    return -1;
  }

  int status_pipe[2];
  if (pipe2(status_pipe, O_CLOEXEC) < 0) {
    LOG_ERRNO("pipe2 failed", errno);
    return -1;
  }

  void *stack = mmap(NULL, CLONE_STACK_SIZE, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
  if (stack == MAP_FAILED) {
    int saved_errno = errno;
    LOG_ERRNO("mmap failed", errno);
    close(status_pipe[0]);
    close(status_pipe[1]);
    errno = saved_errno;
    return -1;
  }

  struct child_args args = {
      .device = device,
      .mountpoint = mountpoint,
      .fstype = fstype,
      .mount_flags = mount_flags,
      .mount_data = mount_data,
      .copy_src = copy_src,
      .copy_dst = copy_dst,
      .status_read_fd = status_pipe[0],
      .status_write_fd = status_pipe[1],
  };

  void *stack_top = (char *)stack + CLONE_STACK_SIZE;
  pid_t pid = clone(child_main, stack_top, CLONE_NEWNS | SIGCHLD, &args);
  if (pid < 0) {
    int saved_errno = errno;
    LOG_ERRNO("clone() failed", errno);
    munmap(stack, CLONE_STACK_SIZE);
    close(status_pipe[0]);
    close(status_pipe[1]);
    errno = saved_errno;
    return -1;
  }
  LOG_VERBOSE("clone()'ed to mount and copy. PID=%d", pid);

  close(status_pipe[1]);

  int wait_status;
  while (waitpid(pid, &wait_status, 0) < 0) {
    if (errno == EINTR)
      continue;
    int saved_errno = errno;
    LOG_ERRNO("waitpid() failed", errno);
    close(status_pipe[0]);
    munmap(stack, CLONE_STACK_SIZE);
    errno = saved_errno;
    return -1;
  }

  struct child_status child_status;
  int read_rc =
      chatgpt_read_all(status_pipe[0], &child_status, sizeof(child_status));
  int read_errno = errno;

  close(status_pipe[0]);
  munmap(stack, CLONE_STACK_SIZE);

  if (read_rc < 0) {
    errno = WIFSIGNALED(wait_status) ? ECHILD : read_errno;
    LOG_ERRNO("failed to read child's pipe", errno);
    return -1;
  }

  if (!WIFEXITED(wait_status) || WEXITSTATUS(wait_status) != 0 ||
      child_status.rc != 0) {
    errno = child_status.err != 0 ? child_status.err : EIO;
    LOG_ERRNO("mount_copy_unmount_ns(): error in child", errno);
    return -1;
  }

  return 0;
}
