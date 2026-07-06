
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/limits.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../common.h"
#include "util.h"

int rm_r(const char *const path) {
  char *buf;
  DIR *d;
  struct dirent *ent;

  LOG_DEBUG("rm_r(\"%s\") started", path);

  LOG_VERBOSE("clearing directory %s", path);

  d = opendir(path);
  if (d == NULL) {
    LOG_WARN("directory '%s' not exist", path);
    return -1;
  }

  buf = malloc(PATH_MAX);
  if (buf == NULL) {
    LOG_ERRNO("malloc failed", errno);
    return -1;
  }

  while ((ent = readdir(d))) {
    // make sure were not going to ./ and ../
    if (strcmp(ent->d_name, "..") && strcmp(ent->d_name, ".")) {
      snprintf(buf, PATH_MAX, "%s/%s", path, ent->d_name);
      LOG_VERBOSE("unlinking '%s'", buf);
      if (unlink(buf) < 0) {
        LOG_WARN("unlink %s failed:%s", buf, strerror(errno));
      }
    }
  }

  closedir(d);
  free(buf);
  LOG_DEBUG("rm_r() exiting");
  return 0;
}

int mkdir_p(const char *path) {
  char *tmp;
  size_t len;
  char *p;
  int err;

  LOG_DEBUG("mkdir_p(\"%s\") starting", path);

  len = strlen(path);
  if (len == 0) {
    return 0;
  }

  if (len >= PATH_MAX) {
    errno = ENAMETOOLONG;
    return -1;
  }

  tmp = malloc(PATH_MAX);
  if (tmp == NULL) {
    return -1;
  }

  strcpy(tmp, path);

  if (tmp[len - 1] == '/') {
    tmp[len - 1] = '\0';
    len--;
  }

  for (p = tmp + 1; *p != '\0'; p++) {
    if (*p == '/') {
      *p = '\0';
      LOG_VERBOSE("creating '%s/'", tmp);

      err = mkdir(tmp, 0700);
      if (err < 0 && errno != EEXIST) {
        LOG_WARN("mkdir '%s' failed:%s", tmp, strerror(errno));
        free(tmp);
        return err;
      }

      *p = '/';
    }
  }

  LOG_VERBOSE("creating '%s/'", tmp);
  err = mkdir(tmp, 0700);
  if (err < 0 && errno != EEXIST) {
    LOG_WARN("mkdir '%s' failed:%s", tmp, strerror(errno));
    free(tmp);
    return err;
  }

  LOG_DEBUG("mkdir_p exiting");

  free(tmp);
  return 0;
}
