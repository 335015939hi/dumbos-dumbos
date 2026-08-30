
#include <curl/curl.h>
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
#include "../exec_wrapper.h"
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

char *find_removable_blockdev(void) {
  DIR *d;
  struct dirent *ent;
  char path[PATH_MAX];
  char buf[8];
  char *ret = NULL;

  LOG_DEBUG("find_removable_blockdev()");

  d = opendir("/sys/block");
  if (d == NULL) {
    LOG_ERRNO("failed to open /sys/block", errno);
    return NULL;
  }

  while ((ent = readdir(d)) != NULL) {
    // skip non-physical devices
    if (strncmp(ent->d_name, "sd", 2) != 0 &&
        strncmp(ent->d_name, "mmcblk", 6) != 0 &&
        strncmp(ent->d_name, "nvme", 4) != 0)
      continue;

    snprintf(path, sizeof(path), "/sys/block/%s/removable", ent->d_name);
    int fd = open(path, O_RDONLY);
    if (fd < 0)
      continue;
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0)
      continue;
    buf[n] = '\0';
    // trim trailing newline
    char *nl = strchr(buf, '\n');
    if (nl)
      *nl = '\0';
    if (buf[0] != '1')
      continue;

    LOG_VERBOSE("found removable device %s", ent->d_name);

    ret = strdup(ent->d_name);
    break;
  }

  closedir(d);
  if (ret == NULL) {
    LOG_ERR("no removable block device found");
    errno = ENODEV;
  }
  return ret;
}

// TODO: set_oem_lock() and set_wifi_enabled() and set_adb_enabled(): better
// error checking (and also signal checking), maybe use something other than
// system()
int set_oem_lock(bool status) {
  const char *cmd;
  int err;
  if (status) {
    // OEM lock
    err = execv_wrapper("/system/bin/service",
                        (char *const[]){"/system/bin/service", "call",
                                        "oem_lock", "4", "i32", "0", NULL});
  } else {
    // OEM unlock
    err = execv_wrapper("/system/bin/service",
                        (char *const[]){"/system/bin/service", "call",
                                        "oem_lock", "4", "i32", "1", NULL});
  }
  if (err < 0) {
    return errno;
  } else {
    return err;
  }
}
int set_wifi_enabled(bool status) {
  const char *cmd;
  int err;
  if (status) {
    err = execv_wrapper("/system/bin/start",
                        (char *const[]){"/system/bin/start", "wificond", NULL});
  } else {
    err = execv_wrapper("/system/bin/stop",
                        (char *const[]){"/system/bin/stop", "wificond", NULL});
  }
  if (err < 0) {
    return errno;
  } else {
    return err;
  }
}
int set_adb_enabled(bool status) {
  const char *cmd;
  int err;
  if (status) {
    err = execv_wrapper("/system/bin/settings",
                        (char *const[]){"/system/bin/settings", "put", "global",
                                        "development_settings_enabled", "1",
                                        NULL});
    if (err == 0) {
      err = execv_wrapper("/system/bin/settings",
                          (char *const[]){"/system/bin/settings", "put",
                                          "global", "adb_enabled", "1", NULL});
    }
  } else {
    err = execv_wrapper("/system/bin/settings",
                        (char *const[]){"/system/bin/settings", "put", "global",
                                        "adb_enabled", "0", NULL});
    if (err == 0) {
      err = execv_wrapper(
          "/system/bin/settings",
          (char *const[]){"/system/bin/settings", "put", "global",
                          "development_settings_enabled", "0", NULL});
    }
  }
  if (err < 0) {
    return errno;
  } else {
    return err;
  }
}

static size_t geturltime_header_callback(char *buffer, size_t size,
                                         size_t nitems, void *userdata) {
  size_t total_size = size * nitems;
  time_t *remote_epoch = (time_t *)userdata;

  // Check if the current header line starts with "date:" (case-insensitive)
  if (total_size > 5 && strncasecmp(buffer, "date:", 5) == 0) {
    // Strip the "date:" prefix and leading whitespace
    char *date_value = buffer + 5;
    while (*date_value == ' ' || *date_value == '\t') {
      date_value++;
    }

    // Trim trailing carriage returns and newlines from the header line
    char *end = buffer + total_size - 1;
    while (end >= date_value && (*end == '\r' || *end == '\n')) {
      *end = '\0';
      end--;
    }

    // Use libcurl's built-in parser to convert the HTTP date string to epoch
    // seconds
    time_t parsed_time = curl_getdate(date_value, NULL);
    if (parsed_time != -1) {
      *remote_epoch = parsed_time;
    }
  }
  return total_size;
}

char *geturltime(void) {
  LOG_DEBUG("geturltime()");
  CURL *curl;
  CURLcode res;
  time_t remote_epoch = -1;
  char *result_str = NULL;

  curl = curl_easy_init();
  if (!curl) {
    LOG_ERRNO("curl_easy_init() fail", errno);
    errno = ENOMEM;
    return NULL;
  }

  // Configure the request to a highly reliable HTTPS server
  curl_easy_setopt(curl, CURLOPT_URL, "https://google.com");

  // We only need the headers to get the time, so issue a HEAD request to save
  // bandwidth
  curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

  // Enforce strict SSL/TLS validation to prevent middleman time-spoofing
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

  // Set up the header processing callback
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, geturltime_header_callback);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &remote_epoch);

  // Fail explicitly on 4xx/5xx server errors
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

  // Set a reasonable network timeout (e.g., 5 seconds)
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

  res = curl_easy_perform(curl);
  curl_easy_cleanup(curl);

  // Verify network transfer succeeded and our callback extracted a valid
  // timestamp
  if (res != CURLE_OK || remote_epoch == -1) {
    LOG_ERRNO("curl failed", errno);
    errno = (res == CURLE_OUT_OF_MEMORY) ? ENOMEM : ECOMM;
    return NULL;
  }

  // Allocate memory for the returned string (enough for a 64-bit integer)
  result_str = malloc(24);
  if (!result_str) {
    errno = ENOMEM;
    return NULL;
  }

  snprintf(result_str, 24, "%ld", (long)remote_epoch);
  return result_str;
}
