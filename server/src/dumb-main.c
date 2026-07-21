
#define _GNU_SOURCE

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
#define CMD_HELP "help"

#define PUBKEY_HEADER "key_public.h"
#define PRIVKEY_HEADER "key_private.h"

static const char *path;

static int verify_payload(const char *key) {
  struct DUMB_PAYLOAD *payload;
  size_t size;

  payload = dp_malloc_load(path, &size);
  if (NULL == payload) {
    LOG_ERRNO("failed to read file", errno);
    return errno;
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
  struct DUMB_PAYLOAD *new = malloc(sizeof(struct DUMB_PAYLOAD));
  if (new == NULL) {
    abort();
  }
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

  int err = write_all(fd, new, sizeof(*new));
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
         "Commands:\n" CMD_HELP
         " [cmd] prints generic help, or more details on a specific command. "
         "<file> is ignored\n" CMD_NEW " creates a new payload\n" CMD_KEY
         " generates new keypair to files " PUBKEY_HEADER " and " PRIVKEY_HEADER
         ". <file> ignored. \n" CMD_EXPIRE
         " <epoch> sets new expiry date. YOU are responsible for making sure "
         "<epoch> is a valid epoch time\n" CMD_VERIFY
         " <pubkeyhex> verifies payload using <pubkeyhex>\n",

         argv0);
}
static int cmd_help(const char *argv0, const char *cmd) {
  const char *help_text;
  if (!strcmp(cmd, CMD_NEW)) {
    help_text =
        "Usage: %s <file> " CMD_NEW "\n"
        " this command creates a new payload and writes to filename <file>\n";
  } else if (!strcmp(cmd, CMD_HELP)) {
    help_text =
        "Usage: %s (ignored) " CMD_HELP " [cmd]\n"
        " this command display general help, or specific help for [cmd] is "
        "given. the  1st argument '(ignored)' (argv[1]) is ignored.\n";
  } else if (!strcmp(cmd, CMD_EXPIRE)) {
    help_text =
        "Usage: %s <file> " CMD_HELP " [epoch]\n"
        " this command gets (no 3rd argument) or sets the expiry date of the "
        "payload file <file> "
        "to <epoch>. epoch is seconds since the Epoch, or Epoch Time "
        "(not to be confused with The Epoch Times)\n"
        " <epoch> should look something like '1784588405' for the date "
        "'July 20, 2026 11:00:05 PM UTC'\n"
        " an invalid time will result in the code expireing after "
        "first use automatically (which is default behavior)\n"
        "a time prefixed with ':' will be interpreter as <epoch> seconds after "
        "first use, i.e. expire that many seconds after someone uses it\n"
        "a time of 0 is no expiry\n";
  } else if (!strcmp(cmd, CMD_KEY)) {
    help_text = "Usage: %s (ignored) " CMD_KEY "\n"
                " this command generates a new ed25519 keypair and writes to "
                "the C header files " PUBKEY_HEADER " and " PRIVKEY_HEADER ".\n"
                " You are responsible for moving the resultant files into the "
                "appropriate directory (or just execute inside appropriate "
                "directory), which should be <project_root>/keys/\n"
                " the first argument (argv[1]) is ignored.\n";

  } else if (!strcmp(cmd, CMD_VERIFY)) {
    help_text = "help for this not yet written\n";
  } else {
    help_text = "No help available for this option\n";
  }
  printf(help_text, argv0);
  return 0;
}

static int get_or_set_expire(const char *expire) {
  int err;
  size_t size;
  int fd;
  struct DUMB_PAYLOAD *payload = dp_malloc_load(path, &size);
  if (payload == NULL) {
    LOG_ERRNO("failed to load payload file", errno);
    maybe_free(payload);
    return errno;
  }
  if (expire == NULL) {
    expire = payload->expire;
    printf("raw expiry time is '%s'\n", expire);
    long long expire_real = dp_get_expire_or_set(payload);
    printf("effective expire time (as if used right now) is\n");
    printf("               %lld\n", expire_real);
    printf("system time is %lld\n", time(NULL));
    if (dp_is_expired(payload)) {
      printf("the code is expired\n");
    } else {
      printf("the code is valid\n");
    }
    maybe_free(payload);
    return 0;
  } else {
    if (strlen(expire) > EXPIRE_SIZE - 1) {
      LOG_ERR("expire string too long, max length is %d", EXPIRE_SIZE - 1);
      maybe_free(payload);
      return E2BIG;
    }
    memset(payload->expire, 0, EXPIRE_SIZE);
    snprintf(payload->expire, EXPIRE_SIZE, "%s", expire);
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
}

static int generate_key(void) {
  char publickey[ED25519_PUBLIC_KEY_HEX_SIZE];
  char privatekey[ED25519_PRIVATE_KEY_HEX_SIZE];
  ed25519_generate_keypair_hex(publickey, privatekey);
  FILE *pubkey_file = fopen(PUBKEY_HEADER, "w");
  FILE *privkey_file = fopen(PRIVKEY_HEADER, "w");
  int err;
  char *text;
  int textlen;
  // check if files opened for writing
  if (pubkey_file == NULL || privkey_file == NULL) {
    err = errno;
    LOG_ERRNO("failed to open " PUBKEY_HEADER " or " PRIVKEY_HEADER
              " for writing",
              err);
    if (pubkey_file)
      fclose(pubkey_file);
    if (privkey_file)
      fclose(privkey_file);
    return err;
  }

  textlen = err =
      asprintf(&text,
               "#ifndef _PUBKEY_HEADER_H\n"
               "#define _PUBKEY_HEADER_H\n"
               "static const char _ed25519_public_key_hex[] =\"%s\";\n"
               "#endif",
               publickey);
  if (err < 0) {
    err = errno;
    LOG_ERRNO("failed to asprintf", err);
    fclose(pubkey_file);
    fclose(privkey_file);
    return err;
  }
  errno = 0;
  err = fwrite(text, 1, textlen, pubkey_file);
  free(text);
  if (err != textlen) {
    err = errno;
    LOG_ERRNO("failed to write to " PUBKEY_HEADER, errno);
    fclose(pubkey_file);
    fclose(privkey_file);
    return err;
  }
  textlen = err =
      asprintf(&text,
               "#ifndef _PRIVKEY_HEADER_H\n"
               "#define _PRIVKEY_HEADER_H\n"
               "static const char _ed25519_private_key_hex[] =\"%s\";\n"
               "#endif",
               privatekey);
  if (err < 0) {
    err = errno;
    LOG_ERRNO("failed to asprintf", err);
    fclose(pubkey_file);
    fclose(privkey_file);
    return err;
  }
  errno = 0;
  err = fwrite(text, 1, textlen, privkey_file);
  free(text);
  if (err != textlen) {
    err = errno;
    LOG_ERRNO("failed to write to " PRIVKEY_HEADER, errno);
    fclose(pubkey_file);
    fclose(privkey_file);
    return err;
  }
  fclose(pubkey_file);
  fclose(privkey_file);
  return 0;
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
        return get_or_set_expire(argv[3]);
      } else if (argc == 3) {
        return get_or_set_expire(NULL);
      }
    } else if (strcmp(CMD_KEY, argv[2]) == 0) {
      return generate_key();
    } else if (strcmp(CMD_VERIFY, argv[2]) == 0) {
      if (argc == 4) {
        return verify_payload(argv[3]);
      }
    } else if (strcmp(CMD_HELP, argv[2]) == 0) {
      if (argc == 4) {
        return cmd_help(argv[0], argv[3]);
      } else {
        display_help(argv[0]);
        return 0;
      }
    }
  }
  display_help(argv[0]);
  return 1;
}
