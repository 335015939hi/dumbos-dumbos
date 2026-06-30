
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "common.h"

#ifdef DEBUG_MODE
int log_verbosity = LOG_VERBOSITY_MAX;
#else
int log_verbosity = LOG_VERBOSITY_NORMAL;
#endif

int set_log_verbosity(const char *const lvl) {
  if (0 == strcasecmp(LOG_NONE_NAME, lvl)) {
    log_verbosity = LOG_VERBOSITY_NONE;
  } else if (0 == strcasecmp(LOG_FATAL_NAME, lvl)) {
    log_verbosity = LOG_VERBOSITY_FATAL;
  } else if (0 == strcasecmp(LOG_ERROR_NAME, lvl)) {
    log_verbosity = LOG_VERBOSITY_ERROR;
  } else if (0 == strcasecmp(LOG_WARN_NAME, lvl)) {
    log_verbosity = LOG_VERBOSITY_WARN;
  } else if (0 == strcasecmp(LOG_NORMAL_NAME, lvl)) {
    log_verbosity = LOG_VERBOSITY_NORMAL;
  } else if (0 == strcasecmp(LOG_VERBOSE_NAME, lvl)) {
    log_verbosity = LOG_VERBOSITY_VERBOSE;
  } else if (0 == strcasecmp(LOG_DEBUG_NAME, lvl)) {
    log_verbosity = LOG_VERBOSITY_DEBUG;
  } else if (0 == strcasecmp(LOG_MAX_NAME, lvl)) {
    log_verbosity = LOG_VERBOSITY_MAX;
  } else {
    errno = EINVAL;
    return -EINVAL;
  }
  return 0;
}

static ssize_t write_all(int fd, const void *buf, size_t len) {
  const char *p = buf;
  const int written = len;

  while (len) {
    ssize_t r = write(fd, p, len);
    if (r < 0) {
      return -1;
    } else if (r == 0) {
      errno = EPIPE;
      return -1;
    }
    p += r;
    len -= r;
  }
  return written;
}
static ssize_t read_all(int fd, void *buf, size_t len) {
  char *p = buf;
  const int readed = len; //'read' is already taken

  while (len) {
    ssize_t r = read(fd, p, len);
    if (r < 0) {
      return -1;
    } else if (r == 0) {
      errno = EPIPE;
      return -1;
    }
    p += r;
    len -= r;
  }

  return readed;
}

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

// return < 0 on error
signed long read_ushort(const int fd) {
  unsigned short val = 0;
  int ret;
  ret = read_all(fd, &val, sizeof(val));
  if (ret < 0)
    return ret; // read error
  return ntohs(val);
}

int write_ushort(const int fd, const unsigned short ushort) {
  int ret;
  unsigned short ns;

  ns = htons(ushort);

  ret = write_all(fd, &ns, sizeof(ns));
  if (ret < 0) // write error
    return ret;
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
    errno = EPIPE;
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
    errno = EPIPE;
    return -1;
  }
  return 1;
}

char *malloc_read_string(const int fd) {
  char *dest;
  signed long len;
  int ret;

  len = read_ushort(fd);
  if (len < 0) {
    return NULL;
  }

  if (len >= MAX_STRING) {
    errno = EOVERFLOW;
    return NULL;
  }

  dest = malloc(len + 1);
  if (NULL == dest) {
    errno = ENOMEM;
    return NULL;
  }

  ret = read_all(fd, dest, len + 1);
  if (ret < 0) {
    free(dest);
    return NULL;
  }

  // security: error if the last byte isn't a \0, probably someone's trying to
  // attack us
  if (dest[len] != '\0') {
    errno = EACCES;
    return NULL;
  }

  return dest;
}

int write_string(const int fd, const char *const s) {
  int ret;
  size_t len;

  len = strlen(s);
  if (len + 1 > USHRT_MAX || len >= MAX_STRING) {
    errno = EOVERFLOW;
    return -1;
  }

  ret = write_ushort(fd, len);
  if (ret < 0)
    return ret;

  ret = write_all(fd, s, len + 1);

  if (ret < 0) {
    return ret;
  }

  return 0;
}

int write_file(const int fd, const char *const path) {
  struct stat st;
  int srcfd;
  int err;
  void *buf;
  unsigned long size;
  unsigned long nl;

  // open the file
  srcfd = open(path, O_RDONLY);
  if (srcfd < 0) {
    return srcfd;
  }

  err = fstat(srcfd, &st);
  if (err < 0) {
    close(srcfd);
    return err;
  }

  if (!S_ISREG(st.st_mode)) {
    close(srcfd);
    errno = EINVAL;
    return -1;
  }

  size = st.st_size;
  // weight control
  if (size > MAX_FILE_SIZE || size <= 0) {
    close(srcfd);
    errno = EFBIG;
    return -1;
  }

  buf = malloc(size);
  if (buf == NULL) {
    close(srcfd);
    return -1;
  }

  err = read_all(srcfd, buf, size);
  if (err < 0) {
    close(srcfd);
    free(buf);
    return -1;
  }

  // send filesize
  nl = htonl(size);
  err = write_all(fd, &nl, sizeof(nl));
  if (err < 0) {
    close(srcfd);
    free(buf);
    return -1;
  }

  // now send the real thing
  err = write_all(fd, buf, size);
  if (err < 0) {
    close(srcfd);
    free(buf);
    return -1;
  }
  free(buf);
  close(srcfd);
  return 0;
}
int read_file(const int fd, const char *const dest) {
  int destfd;
  int err;
  void *buf;
  unsigned long size;

  // open for writing
  destfd = creat(dest, 0o600);
  if (destfd < 0) {
    return -1;
  }

  // recieve file size
  err = read_all(fd, &size, sizeof(size));
  if (err < 0) {
    close(destfd);
    unlink(dest);
    return -1;
  }
  size = ntohl(size);

  // weight control
  if (size > MAX_FILE_SIZE || size < 0) {
    close(destfd);
    unlink(dest);
    errno = EFBIG;
    return -1;
  }

  // allocate a temporary buffer to store file in
  buf = malloc(size);
  if (buf == NULL) {
    unlink(dest);
    close(destfd);
    return -1;
  }

  // read file to buffer
  err = read_all(fd, buf, size);
  if (err < 0) {
    unlink(dest);
    close(destfd);
    free(buf);
    return -1;
  }

  // write buffer to file
  err = write_all(destfd, buf, size);
  if (err < 0) {
    free(buf);
    unlink(dest);
    close(destfd);
    return -1;
  }

  free(buf);
  close(destfd);
  return 0;
}

// warning: not thread safe
const char *timestamp() {
  static char ret[32] = {0};
  time_t t = time(NULL);
  struct tm tm;
  localtime_r(&t, &tm);
  strftime(ret, 32, "%Y-%m-%d %H:%M:%S", &tm);
  return ret;
}
