
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"

signed long parse_ushort(const char *str) {
  signed long ret;
  char *end;
  errno = 0;
  ret = strtoul(str, &end, 10);
  if (errno) {
    return -1;
  }
  if (end == str || *end != '\0') {
    errno = EINVAL;
    return -1;
  }
  if (ret < 0 || ret > USHRT_MAX) {
    errno = ERANGE;
    return -1;
  }
  return ret;
}

void print_error(const char *const error_string) {
  write(STDERR_FILENO, error_string, strlen(error_string));
}

// return < 0 on error
signed long read_ushort(const int fd) {
  unsigned short val = 0;
  int ret;
  ret = read(fd, &val, sizeof(val));
  if (ret < 0)
    return ret; // read error
  if (ret != sizeof(val)) {
    // TODO: find a better error
    errno = EINVAL; // bad size read
    return -1;
  }
  return val;
}

void print_errno(const char *const s, const int err) {
  const char nl = '\n';
  const char colon = ':';
  write(STDERR_FILENO, s, strlen(s));
  write(STDERR_FILENO, &colon, 1);
  const char *const serr = strerror(err);
  write(STDERR_FILENO, serr, strlen(serr));
  write(STDERR_FILENO, &nl, 1);
}

int write_ushort(const int fd, const unsigned short ushort) {
  int ret;

  ret = write(fd, &ushort, sizeof(ushort));
  if (ret < 0) // write error
    return ret;
  if (ret != sizeof(ushort)) {
    // TODO: better error
    errno = EINVAL;
    return -1;
  }
  return 0;
}

int read_byte(const int fd) {
  int ret;
  unsigned char val;
  ret = read(fd, &val, 1);
  if (ret < 0) {
    return ret;
  }
  if (ret != 1) {
    errno = EINVAL;
    return -1;
  }
  return val;
}

int write_byte(const int fd, const unsigned char c) {
  int ret;
  ret = write(fd, &c, 1);
  if (ret < 0) {
    return ret;
  }
  if (ret != 1) {
    errno = EINVAL;
    return -1;
  }
  return ret;
}

char *malloc_read_string(const int fd) {
  char *dest;
  signed long len;
  int ret;

  len = read_ushort(fd);
  if (len < 0) {
    return NULL;
  }

  dest = malloc(len + 1);
  if (NULL == dest) {
    errno = ENOMEM;
    return NULL;
  }

  ret = read(fd, dest, len + 1);
  if (ret < 0) {
    return NULL;
  }

  return dest;
}

int write_string(const int fd, const char *const s) {
  int ret;
  size_t len;

  len = strlen(s);
  if (len + 1 > USHRT_MAX) {
    errno = EOVERFLOW;
    return -1;
  }

  ret = write_ushort(fd, len);
  if (ret < 0)
    return ret;

  ret = write(fd, s, len + 1);

  if (ret < 0) {
    return ret;
  }

  if (ret != len + 1) {
    errno = EINVAL;
    return -1;
  }

  return 0;
}
