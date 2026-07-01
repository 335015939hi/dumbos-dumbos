#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../common.h"
#include "secret.h"
#include "server.h"

static int handle_client(const int fd, const char *const code_dir,
                         const char *const code_used_dir);

int do_server(const char *const addr, const char *const port,
              const char *const _code_dir, const char *const _code_used_dir) {
  struct addrinfo hints;
  struct addrinfo *res;
  struct addrinfo *p;
  int serverfd;
  int err;
  int len;
  char *code_dir;
  char *code_used_dir;

  // TODO:check for bad malloc
  // add a slash to code*dir if not exist
  len = strlen(_code_dir);
  code_dir = malloc(len + 1 + 1); // plus 1 for NULL, plus 1 for maybe slash
  strcpy(code_dir, _code_dir);
  if (code_dir[len] != '/') {
    code_dir[len] = '/';
    code_dir[len + 1] = '\0';
  }
  len = strlen(_code_used_dir);
  code_used_dir = malloc(len + 1 + 1); // plus 1 for NULL, plus 1 for maybe
                                       // slash
  strcpy(code_used_dir, _code_used_dir);
  if (code_used_dir[len] != '/') {
    code_used_dir[len] = '/';
    code_used_dir[len + 1] = '\0';
  }

  LOG_VERBOSE("addr=%s", addr);
  LOG_VERBOSE("port=%s", port);
  LOG("code_dir=%s", code_dir);
  LOG("code_used_dir=%s", code_used_dir);

  hints = (struct addrinfo){
      .ai_family = AF_UNSPEC,
      .ai_socktype = SOCK_STREAM,
      .ai_flags = AI_PASSIVE,
  };

  err = getaddrinfo(addr, port, &hints, &res);
  if (err) {
    if (err < 0)
      err = -err;
    LOG_FATAL_ERRNO("getaddrinfo failed", err);
    free(code_dir);
    free(code_used_dir);
    return err;
  }

  for (p = res; p != NULL; p = p->ai_next) {
    serverfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (serverfd < 0) {
      continue;
    }
    int yes = 1;
    if (setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
      LOG_WARN_ERRNO("setsockopt SO_REUSEADDR failed", errno);
    }

    if (bind(serverfd, p->ai_addr, p->ai_addrlen) == 0)
      break;
    err = errno;
    close(serverfd);
  }

  if (NULL == p) {
    LOG_FATAL_ERRNO("failed to bind", err);
    freeaddrinfo(res);
    free(code_dir);
    free(code_used_dir);
    return err;
  }
  freeaddrinfo(res);

  err = listen(serverfd, 16);
  if (err < 0) {
    LOG_FATAL_ERRNO("failed to listen", errno);
    free(code_dir);
    free(code_used_dir);
    return errno;
  }

  LOG("listening on %s:%s", addr, port);

  struct sockaddr_storage *client;
  socklen_t client_len;
  pid_t childp;
  int clientfd;
  char *host;
  char *serv;

  host = calloc(1, NI_MAXHOST);
  serv = calloc(1, NI_MAXSERV);
  client_len = sizeof(*client);
  client = malloc(client_len);

  for (;;) {

    clientfd = accept(serverfd, (struct sockaddr *)client, &client_len);
    LOG_VERBOSE("found a connection");
    if (clientfd < 0) {
      LOG_ERRNO("accept client failed", errno);
      continue;
    }

    childp = fork();
    if (childp < 0) {
      LOG_ERRNO("failed to fork", errno);
      close(clientfd);
      continue;
    } else if (childp != 0) {
      LOG("forked to accept client. PID=%d", childp);
      close(clientfd);
      continue;
    }

    err = getnameinfo((struct sockaddr *)client, client_len, host, NI_MAXHOST,
                      serv, NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV);
    if (err != 0) {
      LOG_ERRNO("failed to getnameinfo", errno);
      break;
    }

    LOG("connection from %s:%s", host, serv);

    err = handle_client(clientfd, code_dir, code_used_dir);
    LOG_DEBUG("handle_client() exited with %d", err);
    if (err != 0) {
      LOG_ERR("handle_client() exited with %d", err);
    } else {
      LOG_VERBOSE("handle_client() exited with 0");
    }

    close(clientfd);
    break;
  };

  LOG_VERBOSE("do_server() returning,code %d", err);

  free(code_dir);
  free(code_used_dir);
  free(host);
  free(serv);
  free(client);

  return err;
}

static int handle_client(const int fd, const char *const code_dir,
                         const char *const code_used_dir) {
  int err;
  int ret;
  char *client_v_str;
  unsigned short client_v_maj;
  unsigned short client_v_min;
  unsigned short client_v_pat;
  unsigned short command;

  LOG_DEBUG("handle_client() started");
  LOG_DEBUG("fd=%d", fd);

  client_v_str = malloc_read_string(fd);
  if (client_v_str == NULL) {
    LOG_ERRNO("failed to read client version string", errno);
    return errno;
  }

  err = read_ushort(fd); // major
  if (err >= 0) {
    client_v_maj = (unsigned short)err;
    err = read_ushort(fd); // minor
    if (err >= 0) {
      client_v_min = (unsigned short)err;
      err = read_ushort(fd); // patch
      client_v_pat = (unsigned short)err;
    }
  }
  if (err < 0) {
    LOG_ERRNO("failed to read client version", errno);
    free(client_v_str);
    return errno;
  }

  LOG("client %s (%hu.%hu.%hu)", client_v_str, client_v_maj, client_v_min,
      client_v_pat);

  // version sanity check, semantic versioning
  if ((unsigned short)VERSION_MAJOR != client_v_maj ||
      (unsigned short)VERSION_MINOR < client_v_min) {
    free(client_v_str);
    write_ushort(fd, EPROTO);
    return EPROTO;
  }

  // tell client connection is good
  write_ushort(fd, 0);
  free(client_v_str);

  // get command
  err = read_ushort(fd);
  if (err < 0) {
    LOG_ERRNO("failed to read command", errno);
    return errno;
  }
  command = err;
  LOG("recieved command %d", command);

  switch (command) {
  case SERVER_CMD_SECRET_CODE:
    ret = handle_secret(fd, code_dir, code_used_dir);
    LOG_DEBUG("handle_secret() exited with %d", ret);
    break;
  case SERVER_CMD_OK:
    ret = 0;
    break;
  default:
    LOG_ERR("unknown command %d", command);
    ret = EINVAL;
    break;
  }

  write_ushort(fd, ret);
  return ret;
}
