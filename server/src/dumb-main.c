
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "dumb.h"
#include "ed25519.h"

#define CMD_NEW "new"
#define CMD_EXPIRE "expire"
#define CMD_KEY "mkkey"
#define CMD_VERIFY "verify"

static const char *path;

static int verify_payload(const char *key) {
  struct DUMB_PAYLOAD *payload;
  size_t size;

  payload = dp_malloc_load(path, &size);
  if (NULL == payload) {
    LOG_ERRNO("failed to read file", errno);
  }

  if (0 > dp_verify(payload, size, key)) {
    maybe_free(payload);
    LOG_ERRNO("bad signature", errno);
    return EACCES;
  }
  maybe_free(payload);
  LOG("good signature");
  return 0;
}

static int new_payload() {
  struct DUMB_PAYLOAD *new = malloc(sizeof(struct DUMB_PAYLOAD) + 500);
  *new = (struct DUMB_PAYLOAD){
      "signature here (dont touch)",
      "expire here",
      "cmd here",
  };
  int fd = open(path, O_CREAT | O_RDWR, 0600);
  if (fd < 0) {
    maybe_free(new);
    return errno;
  }

  new->signature[ED25519_SIGNATURE_HEX_SIZE - 1] =
      new->expire[EXPIRE_SIZE - 1] = new->command[COMMAND_SIZE - 1] = '\n';
  strcpy(new->payload, "data here");

  int err = write_all(fd, new, sizeof(*new) + strlen(new->payload));
  if (err < 0) {
    maybe_free(new);
    close(fd);
    return errno;
  }
  LOG("%d bytes written", err);
  maybe_free(new);
  close(fd);
  return 0;
}
static void display_help(const char *argv0) {
  printf("Usage:%s <file> <cmd> [args]\n"
         "Commands:\n" CMD_NEW " creates a new payload\n" CMD_KEY
         " generates new keypair to stdout. <file> ignored. \n" CMD_EXPIRE
         "<epoch> sets new expiry date. YOU are responsible for making sure "
         "<epoch> is a valid epoch time\n" CMD_VERIFY
         "<pubkeyhex> verifies payload using <pubkeyhex>\n",
         argv0);
}

static int set_expire(const char *expire) {
  int err;
  size_t size;
  int fd;
  struct DUMB_PAYLOAD *payload = dp_malloc_load(path, &size);
  if (payload == NULL) {
    LOG_ERRNO("failed to load payload file", errno);
    maybe_free(payload);
    return errno;
  }
  strncpy(payload->expire, expire, EXPIRE_SIZE - 1);
  LOG("new expire time:%s", payload->expire);
  fd = open(path, O_WRONLY);
  if (fd < 0) {
    LOG_ERRNO("failed to open file for writing", errno);
    maybe_free(payload);
    return errno;
  }
  err = write_all(fd, payload, size);
  if (err < 0) {
    LOG_ERRNO("failed to write to file", errno);
    close(fd);
    maybe_free(payload);
    return errno;
  }
  LOG("%zu bytes written", size);
  close(fd);
  maybe_free(payload);
  return 0;
}

static void generate_key(void) {
  char publickey[ED25519_PUBLIC_KEY_HEX_SIZE];
  char privatekey[ED25519_PUBLIC_KEY_HEX_SIZE];
  ed25519_generate_keypair_hex(publickey, privatekey);
  printf("public  = %s\nprivate = %s\n", publickey, privatekey);
}

int main(int argc, char **argv) {
  if (argc >= 3) {
    path = argv[1];
    if (strcmp(CMD_NEW, argv[2]) == 0) {
      if (argc == 3) {
        return new_payload();
      }
    } else if (strcmp(CMD_EXPIRE, argv[2]) == 0) {
      if (argc == 4) {
        return set_expire(argv[3]);
      }
    } else if (strcmp(CMD_KEY, argv[2]) == 0) {
      generate_key();
      return 0;
    } else if (strcmp(CMD_VERIFY, argv[2]) == 0) {
      if (argc == 4) {
        return verify_payload(argv[3]);
      }
    }
  }
  display_help(argv[0]);
  return 1;
}
