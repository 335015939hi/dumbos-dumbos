
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "dumb.h"
#include "ed25519.h"
#include "requestid.h"

#define CMD_NEW "new"
#define CMD_EXPIRE "expire"
#define CMD_KEY "mkkey"
#define CMD_VERIFY "verify"
#define CMD_HELP "help"
#define CMD_DUMP_DATA "data-dump"
#define CMD_DATA_SET "data-set"
#define CMD_COMMAND_SET_RAW "set-cmd-raw"
#define CMD_GET_COMMAND "get-cmd"
#define CMD_COMMAND_SET "set-cmd"
#define CMD_NEW_USER "new-user"

#define PUBKEY_HEADER "key_public.h"
#define PRIVKEY_HEADER "key_private.h"

#define USER_BLOB_SUFFIX "blob"
#define SERVER_USER_PUBKEY_FILE "pubkey"

const char *path;

// defined in dumb-tool-cmd-set.c
int cmd_command_set(int argc, const char **argv);

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
  printf(
      "Usage:%s <file> <cmd> [args]\n"
      "Commands:\n" CMD_HELP
      " [cmd] prints generic help, or more details on a specific command. "
      "<file> is ignored\n" CMD_NEW " creates a new payload\n" CMD_KEY
      " generates new keypair to files " PUBKEY_HEADER " and " PRIVKEY_HEADER
      ". <file> ignored. \n" CMD_EXPIRE
      " <epoch> sets new expiry date. YOU are responsible for making sure "
      "<epoch> is a valid epoch time\n" CMD_VERIFY
      " <pubkeyhex> verifies payload using <pubkeyhex>\n" CMD_COMMAND_SET_RAW
      " <command> sets the command, withou being friendly. not "
      "recommended\n" CMD_COMMAND_SET
      " <command> [command-specific] sets the command, with auto formatting "
      "data\n" CMD_GET_COMMAND " prints command\n" CMD_DATA_SET
      " <file> dumps contents of file as the data\n" CMD_DUMP_DATA
      " dumps data to stdout. you may want to pipe into file\n" CMD_NEW_USER
      " creates a new user, where <file> is both username and file written.\n",

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
    help_text = "Usage:%s <file> " CMD_VERIFY " <pubkey>\n"
                "This command verifies the payload against <pubkey>, which is "
                "a 32-byte (64 character) hex string. note that payloads are "
                "automatically signed before sending to devices, and the "
                "signature is not written to disk\n";
  } else if (!strcmp(cmd, CMD_GET_COMMAND)) {
    help_text = "Usage:%s <file> " CMD_GET_COMMAND "\n"
                "this command prints the current command specified in the "
                "file. doesn't do much else. you may have to read source code "
                "to see if the command is valid or not\n";
  } else if (!strcmp(cmd, CMD_COMMAND_SET_RAW)) {
    help_text =
        "Usage:%s <file> " CMD_COMMAND_SET_RAW " <command>\n"
        "sets the command in the file to <command>. does not check if command "
        "is valid, nor sets and formats data\n"
        "you are not recommended to use this command, use " CMD_COMMAND_SET
        " instead\n";
  } else if (!strcmp(cmd, CMD_COMMAND_SET)) {
    help_text =
        "Usage:%s <file> " CMD_COMMAND_SET
        " <command> [command arguments described below]\n"
        "sets payload command, and then sets the data according to "
        "command-specific arguments and stuff. \n"
        "details of each command:\n"
        "'" CODE_CMD_OK "' (no arguments) -- a no-op. useful for testing\n"
        "'" CODE_CMD_INSTALLTHIS "' <apk> -- installes a apk file given in "
        "data. <apk> is the apk file to embed\n"
        "'" CODE_CMD_INSTALL_PATH
        "' <path> <sha256sum> -- installed a apk file by local path (on "
        "recieving device), verifying it using the provided sha256  checksum. "
        "this can be useful to avoid large downloads. "
        "Warning:we do a basic length check for the sha256, but otherwise it "
        "is YOUR responsibility to make sure it is a valid sha256 checksum.\n"
        "'" CODE_CMD_FILE_EXPORT "' (no argument) -- mounts an external drive "
        "and copies internal files out\n"
        "'" CODE_CMD_FILE_IMPORT "' (no argument) -- mounts an external drive "
        "and copies drive files in\n"
        "'" CODE_CMD_ADB_ENABLE "/" CODE_CMD_ADB_DISABLE
        "' (no argument) -- enables or disables ADB, respectively\n"
        "'" CODE_CMD_WIFI_ENABLE "/" CODE_CMD_WIFI_DISABLE
        "' (no argument) -- enables or disables wifi, respectively\n"
        "'" CODE_CMD_OEM_UNLOCK "/" CODE_CMD_OEM_LOCK
        "' (no argument) -- enables or disables OEM/bootloader unlocking, "
        "respectively\n"
        "'" CODE_CMD_COMPOSITE
        "' <file1> [file2 [...]] -- packs other payloads into one. each "
        "payload file will still be verified seperately, so make sure they're "
        "signed and won't expire before the master code\n"
        "'" CODE_CMD_FW_ALLOW "/" CODE_CMD_FW_DENY
        "' <packageID> [packageID2 [...]] -- allow or deny, respectively, "
        "certain apps internet, by the apps' package ID. note that this is UID "
        "based, and some apps (especially system apps) share a UID, so "
        "allowing one app in such a group will allow all apps too. if you're "
        "denying, you have to make sure all apps in the group are denied; one "
        "app still allowed will keep all apps allowed\n"
        "'" CODE_CMD_FW_FLUSH
        "' (no argument) -- resets internet allowed apps.\n"
        "'" CODE_CMD_FW_TEMP_ADD
        "' <packageID> [packageID2 [...]] -- temporarily allow some apps "
        "internet, until the next time firewall is refreshed (e.g. "
        "with " CODE_CMD_FW_ALLOW " or " CODE_CMD_FW_FLUSH " or a reboot)\n";
  } else if (!strcmp(cmd, CMD_DUMP_DATA)) {
    help_text = "not yet written\n";

  } else if (!strcmp(cmd, CMD_DATA_SET)) {
    help_text = "not yet written\n";

  } else if (!strcmp(cmd, CMD_NEW_USER)) {
    help_text =
        "Usage:%s <username> " CMD_NEW_USER "\n"
        "generates a keypair and creates directory <username>, and write "
        "public key (for server) to <username>/" SERVER_USER_PUBKEY_FILE
        ", and creates a blob for device at <username>." USER_BLOB_SUFFIX
        ". push <username>." USER_BLOB_SUFFIX
        " to the (userdebug or eng build) device with 'adb root;adb push "
        "<username>.dumb /mnt/vendor/persist/dumbos_user;adb shell chmod 600 "
        "/mnt/vendor/persist/dumbos_user' and then flash the user build";
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
    printf("system time is %ld\n", time(NULL));
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

static int cmd_command_set_raw(const char *command) {
  if (strlen(command) + 1 > COMMAND_SIZE) {
    LOG_ERR("command '%s' too long, max length is %d", command,
            COMMAND_SIZE - 1);
  }
  size_t size;
  struct DUMB_PAYLOAD *payload = dp_malloc_load(path, &size);
  if (payload == NULL) {
    LOG_ERRNO("failed to load payload file", errno);
    maybe_free(payload);
    return errno;
  }
  LOG("setting command to '%s'", command);
  strcpy(payload->command, command);
  int fd = open(path, O_WRONLY);
  if (fd < 0) {
    LOG_ERRNO("failed to open file for writing", errno);
    maybe_free(payload);
    return errno;
  }
  int err = write_all(fd, payload, size);
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

static int cmd_get_command() {
  size_t size;
  struct DUMB_PAYLOAD *payload = dp_malloc_load(path, &size);
  if (payload == NULL) {
    LOG_ERRNO("failed to load payload file", errno);
    maybe_free(payload);
    return errno;
  }
  printf("%s\n", payload->command);
  maybe_free(payload);
  return 0;
}

static int cmd_dump_data() {
  size_t size;
  struct DUMB_PAYLOAD *payload = dp_malloc_load(path, &size);
  if (payload == NULL) {
    LOG_ERRNO("failed to load payload file", errno);
    maybe_free(payload);
    return errno;
  }
  fwrite(payload->payload, 1, size - sizeof(*payload), stdout);
  maybe_free(payload);
  return 0;
}
static int cmd_set_data(const char *data_path) {
  size_t size;
  struct DUMB_PAYLOAD *payload = dp_malloc_load(path, &size);
  if (payload == NULL) {
    LOG_ERRNO("failed to load payload file", errno);
    maybe_free(payload);
    return errno;
  }
  FILE *data = fopen(data_path, "r");
  if (data == NULL) {
    LOG_ERRNO("failed to open data file for reading", errno);
    maybe_free(payload);
    return errno;
  }
  FILE *dest = fopen(path, "w");
  if (dest == NULL) {
    LOG_ERRNO("failed to open file for writing", errno);
    fclose(data);
    maybe_free(payload);
    return errno;
  }

  fwrite(payload, 1, sizeof(*payload), dest);

  int written;
  int chunk_written = -1;
  const int buf_size = 4096;
  char *buf = malloc(buf_size);
  do {
    chunk_written++;
    written = fread(buf, 1, buf_size, data);
    written = fwrite(buf, 1, written, dest);
  } while (written == buf_size);
  LOG("%zu bytes written, of which %d bytes is data",
      chunk_written * buf_size + written + sizeof(*payload),
      chunk_written * buf_size + written);

  maybe_free(buf);
  maybe_free(payload);
  fclose(dest);
  fclose(data);
  return 0;
}

static int cmd_new_user() {
  struct DUMBOS_USER_DATA *new_user;
  int err;
  char privkey[ED25519_PRIVATE_KEY_HEX_SIZE];
  char pubkey[ED25519_PUBLIC_KEY_HEX_SIZE];
  if (0 > ed25519_generate_keypair_hex(pubkey, privkey)) {
    LOG_ERRNO("failed to generate keypair", errno);
    return errno;
  }
  new_user = dumbos_alloc_new_user(path, privkey);
  if (new_user == NULL) {
    LOG_ERRNO("failed to generate new user blob", errno);
    return errno;
  }
  if (0 > mkdir(path, 00700)) {
    LOG_ERRNO("mkdir failed", errno);
    free(new_user);
    return errno;
  }

  char *server_pubkey_path;
  char *user_blob_path;

  if (0 >
      asprintf(&server_pubkey_path, "%s/%s", path, SERVER_USER_PUBKEY_FILE)) {
    LOG_ERRNO("asprintf failed", errno);
    free(new_user);
    return errno;
  }
  FILE *server_pubkey_file = fopen(server_pubkey_path, "wb");
  free(server_pubkey_path);
  if (NULL == server_pubkey_file) {
    LOG_ERRNO("failed to open file for writing", errno);
    free(new_user);
    return 0;
  }
  if (ED25519_PUBLIC_KEY_HEX_SIZE - 1 !=
      fwrite(pubkey, 1, ED25519_PUBLIC_KEY_HEX_SIZE - 1, server_pubkey_file)) {
    err = errno;
    LOG_ERRNO("failed writing to file", err);
    free(new_user);
    fclose(server_pubkey_file);
    return err;
  }
  fclose(server_pubkey_file);

  if (0 > asprintf(&user_blob_path, "%s.%s", path, USER_BLOB_SUFFIX)) {
    LOG_ERRNO("asprintf failed", errno);
    free(new_user);
    return errno;
  }
  FILE *user_blob_file = fopen(user_blob_path, "wb");
  free(user_blob_path);
  if (NULL == user_blob_file) {
    LOG_ERRNO("failed to open file for writing", errno);
    free(new_user);
    return errno;
  }
  errno = 0;
  if (sizeof(*new_user) !=
      fwrite(new_user, 1, sizeof(*new_user), user_blob_file)) {
    err = errno;
    LOG_ERRNO("failed writing to file", err);
    fclose(user_blob_file);
    free(new_user);
    return err;
  }
  free(new_user);
  fclose(user_blob_file);
  return 0;
}

int main(int argc, char **argv) {
  for (int i = 0; i < argc; i++) {
    LOG_DEBUG("argv[%d]='%s'", i, argv[i]);
  }
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
        return 1;
      }
    } else if (strcmp(CMD_COMMAND_SET_RAW, argv[2]) == 0) {
      if (argc == 4) {
        return cmd_command_set_raw(argv[3]);
      }
    } else if (strcmp(CMD_COMMAND_SET, argv[2]) == 0) {
      if (argc >= 4) {
        return cmd_command_set(argc - 3, (const char **)(&argv[3]));
      }
    } else if (strcmp(CMD_GET_COMMAND, argv[2]) == 0) {
      if (argc == 3) {
        return cmd_get_command();
      }
    } else if (strcmp(CMD_DUMP_DATA, argv[2]) == 0) {
      if (argc == 3) {
        return cmd_dump_data();
      }
    } else if (strcmp(CMD_DATA_SET, argv[2]) == 0) {
      if (argc == 4) {
        return cmd_set_data(argv[3]);
      }
    } else if (strcmp(CMD_NEW_USER, argv[2]) == 0) {
      if (argc == 3) {
        return cmd_new_user();
      }
    }
  }
  display_help(argv[0]);
  return 1;
}
