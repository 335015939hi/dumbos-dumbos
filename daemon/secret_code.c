
#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
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

#define MAX(a, b) ((a) > (b) ? (a) : (b))

// caller expects returned string to be free()able, so we cant just 'return
// "whatever";'
static char *malloc_str(const char *const s) {
  char *ret = malloc(strlen(s) + 1);
  if (ret == NULL)
    abort();
  strcpy(ret, s);
  return ret;
}
static bool secret_str_safe(const char *const s);

char *secret_code(int argc, char **argv, int *ret_val, const char *const host,
                  const char *const port, const char *const tmpdir) {
  if (argc != 1) {
    *ret_val = EINVAL;
    LOG_ERR("secret_code: expected exactly ONE code");
    return malloc_str("secret_code: expected exactly ONE code\n");
  }

  // safety:set umask
  umask(0077);

  struct addrinfo *hints;
  struct addrinfo *res;
  struct addrinfo *p;
  int err;
  int fd;
  int secret_len_max;
  bool can_verify;

  secret_len_max =
      PATH_MAX - (strlen(tmpdir) + MAX(strlen(EXT_SIG), strlen(EXT_CODE)) +
                  1 // NULL terminator
                 );
  LOG_VERBOSE("secret code maximum length is %d", secret_len_max - 1);
  if (strlen(argv[0]) > secret_len_max) {
    LOG_ERR("secret code '%s' too long", argv[0]);
    *ret_val = ENAMETOOLONG;
    return malloc_str("secret_code: secret too long");
  }

  hints = malloc(sizeof(struct addrinfo));
  memset(hints, 0, sizeof(struct addrinfo));
  hints->ai_family = AF_UNSPEC; // IPv4 or IPv6
  hints->ai_socktype = SOCK_STREAM;

  LOG("using server=%s:%s", host, port);
  err = getaddrinfo(host, port, hints, &res);
  free(hints);
  if (err != 0) {
    *ret_val = err;
    LOG_ERRNO("failed to getaddr", err);
    return NULL;
  }

  fd = -1;
  for (p = res; p != NULL; p = p->ai_next) {
    fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (fd < 0)
      continue;

    err = connect(fd, p->ai_addr, p->ai_addrlen);
    if (err == 0) {
      break;
    } else {
      err = errno;
      close(fd);
      fd = -1;
    }
  }

  freeaddrinfo(res);

  if (fd < 0) {
    LOG_ERRNO("failed to connect", err);
    *ret_val = err;
    return NULL;
  }

  // write version to server
  err = write_string(fd, VERSION_STRING);
  if (err == 0) {
    err = write_ushort(fd, (unsigned short)VERSION_MAJOR);
    if (err == 0) {
      err = write_ushort(fd, (unsigned short)VERSION_MINOR);
      if (err == 0) {
        err = write_ushort(fd, (unsigned short)VERSION_PATCH);
      }
    }
  }
  if (err != 0) {
    *ret_val = errno;
    LOG_ERRNO("failed to send version", errno);
    return NULL;
  }

  // read status
  err = read_ushort(fd);
  if (err < 0) {
    *ret_val = errno;
    LOG_ERRNO("failed to read server status", errno);
    return NULL;
  } else if (err != 0) {
    *ret_val = err;
    LOG_ERRNO("server doesn't like us", err);
    return malloc_str(strerror(err));
  }

  // send secret code command
  LOG_VERBOSE("sending command %d to server", SERVER_CMD_SECRET_CODE);
  err = write_ushort(fd, SERVER_CMD_SECRET_CODE);

  LOG("sending secret code %s", argv[0]);
  err = write_string(fd, argv[0]);
  if (err != 0) {
    *ret_val = errno;
    LOG_ERRNO("write secret code failed", errno);
    return NULL;
  }

  LOG_DEBUG("reading what the server thinks of our secret");
  err = read_ushort(fd);
  if (err < 0) {
    *ret_val = errno;
    LOG_ERRNO("could not read secret code acceptance status", errno);
    return NULL;
  }

  if (err == 0) {
    LOG_VERBOSE("secret code accepted");
    // extra security check: sanitize secret code, we will be doing fileio based
    // off it server should have already done this, but who knows if its the
    // right server
    LOG_DEBUG("checking secret code for bad chars");
    if (secret_str_safe(argv[0])) {
      LOG_DEBUG("secret code passed sanitizer");
      char *pathsig;
      char *pathfil;
      pathsig = malloc(PATH_MAX);
      pathfil = malloc(PATH_MAX);
      // TODO:malloc check

      sprintf(pathfil, "%s%s%s", tmpdir, argv[0], EXT_CODE);
      LOG_VERBOSE("reading into '%s'", pathfil);
      err = read_file(fd, pathfil);
      if (err < 0) {
        LOG_ERR("failed to read into '%s':%s", pathfil, strerror(errno));
        free(pathfil);
        free(pathsig);
        *ret_val = errno;
        return NULL;
      }
      sprintf(pathsig, "%s%s%s", tmpdir, argv[0], EXT_SIG);
      LOG_VERBOSE("reading into '%s'", pathsig);
      err = read_file(fd, pathsig);
      if (err < 0) {
        LOG_ERR("failed to read into '%s':%s", pathsig, strerror(errno));
        free(pathfil);
        free(pathsig);
        *ret_val = errno;
        return NULL;
      }

      LOG_VERBOSE("verifying %s with %s", pathfil, pathsig);
      err = verify_sig(pathfil, pathsig);
      LOG_DEBUG("verify_sig() exited with %d", err);
      if (err != 0) {
        LOG_ERRNO("verification failed", errno);
        can_verify = false;
      } else {
        LOG("verification Success. Code is Valid");
        can_verify = true;
      }
      LOG_DEBUG("reading server's return code");
      err = read_ushort(fd);
      if (err < 0) {
        *ret_val = errno;
        LOG_ERRNO("could not read server's return code", errno);
      }
      LOG("server returned %d", err);

      if (can_verify) {
        LOG_DEBUG("preparing to execute script");
        pid_t childp = fork();
        if (childp < 0) {
          LOG_ERRNO("fork failed", errno);
          unlink(pathsig);
          unlink(pathfil);
          free(pathsig);
          free(pathfil);
          *ret_val = errno;
          return malloc_str("Code valid but internal error 265");
        } else if (childp == 0) {
          // child
          LOG("execvp(\"sh\",(char*[]){\"sh\",\"%s\",NULL})", pathfil);
          execvp("sh", (char *[]){"/bin/sh", pathfil, NULL});
          LOG_ERRNO("execvp  failed", errno);
          *ret_val = errno;
          return malloc_str("Code valid but internal error 908");
        } else {
          // parent
          if (waitpid(childp, &err, 0) < 0) {
            *ret_val = errno;
            LOG_ERRNO("waitpid() failed", errno);
            return malloc_str("Code valid but internal error 102");
          }
          LOG_VERBOSE("child %ld exited with raw %d", (long)childp, err);
          if (WIFEXITED(err)) {
            err = WEXITSTATUS(err);
            LOG("Script exited with %d", err);
            *ret_val = err;
            if (err != 0) {
              return malloc_str("Code valid but internal error 173");
            } else {
              return malloc_str("Code valid. Good job");
            }
          } else if (WIFSIGNALED(err)) {
            err = WTERMSIG(err);
            LOG("Script killed by signal %d", err);
            *ret_val = err;
            return malloc_str("Code valid but got signal");
          } else {
            *ret_val = 999;
            return malloc_str("Code valid but internal error 999");
          }
        }

        unlink(pathsig);
        unlink(pathfil);
        free(pathsig);
        free(pathfil);
        *ret_val = err;
        return malloc_str("Code valid");
      } else {
        free(pathsig);
        free(pathfil);
        unlink(pathsig);
        unlink(pathfil);
        *ret_val = EINVAL;
        return malloc_str("Signature invalid");
      }

    } else {
      LOG_ERR("invalid characters detected in secret code");
    }
  } else {
    LOG_ERRNO("server doesn't like our secret", err);
  }

  LOG_DEBUG("reading server's return code");
  err = read_ushort(fd);
  if (err < 0) {
    *ret_val = errno;
    LOG_ERRNO("could not read server's return code", errno);
    return NULL;
  }
  LOG("server returned %d", err);

  *ret_val = err;
  return NULL;
}

static bool secret_str_safe(const char *const s) {
  const char *i;
  const char *j;
  const char *const allow = SECRET_CODE_ALLOWED_CHARS;

  for (i = s; *i != '\0'; i++) {
    for (j = allow; *j != '\0'; j++) {
      if (*i == *j) {
        break;
      }
    }
    if (*j == '\0') {
      return false;
    }
  }
  return true;
}
