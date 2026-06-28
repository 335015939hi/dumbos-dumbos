
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "common.h"

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
