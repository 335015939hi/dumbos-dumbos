
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <openssl/err.h>
#include <openssl/evp.h>
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

      err = mkdir(tmp, 0o700);
      if (err < 0 && errno != EEXIST) {
        LOG_WARN("mkdir '%s' failed:%s", tmp, strerror(errno));
        free(tmp);
        return err;
      }

      *p = '/';
    }
  }

  LOG_VERBOSE("creating '%s/'", tmp);
  err = mkdir(tmp, 0o700);
  if (err < 0 && errno != EEXIST) {
    LOG_WARN("mkdir '%s' failed:%s", tmp, strerror(errno));
    free(tmp);
    return err;
  }

  LOG_DEBUG("mkdir_p exiting");

  free(tmp);
  return 0;
}

// TODO:
static const char pubkey[] = {0};
int verify_sig(const char *file, const char *sig) {
  int sfd;
  int ffd;
  off_t sigsize;
  off_t filesize;
  void *sigbuf;
  void *filebuf;
  int err;
  struct stat st;

  LOG_DEBUG("verify_sig() called on '%s' '%s'", file, sig);

  LOG_DEBUG("opening '%s'", sig);
  sfd = open(sig, O_RDONLY);
  if (sfd < 0) {
    LOG_ERR("open '%s' fail:%s", sig, strerror(errno));
    return -1;
  }
  LOG_DEBUG("opening '%s'", file);
  ffd = open(file, O_RDONLY);
  if (ffd < 0) {
    err = errno;
    LOG_ERR("open '%s' fail:%s", file, strerror(errno));
    close(sfd);
    errno = err;
    return -1;
  }
  LOG_DEBUG("ffd=%d sfd=%d", ffd, sfd);

  LOG_DEBUG("getting %s size", sig);
  err = fstat(sfd, &st);
  if (err < 0) {
    err = errno;
    LOG_ERR("failed fstat() on '%s':%s", sig, strerror(err));
    close(sfd);
    close(ffd);
    errno = err;
    return -1;
  }
  sigsize = st.st_size;
  LOG_DEBUG("%s size is %ld", sig, sigsize);

  LOG_DEBUG("getting %s size", file);
  err = fstat(ffd, &st);
  if (err < 0) {
    err = errno;
    LOG_ERR("failed fstat() on '%s':%s", file, strerror(err));
    close(sfd);
    close(ffd);
    errno = err;
    return -1;
  }
  filesize = st.st_size;
  LOG_DEBUG("%s size is %ld", file, filesize);

  errno = EINVAL;
  return -1;
}
