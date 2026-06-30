#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../common.h"
#include "server.h"

static int handle_client(const int fd);

int do_server(const char *const addr, const char *const port,
              const char *const code_dir, const char *const code_persist_dir) {
  struct addrinfo hints;
  struct addrinfo *res;
  struct addrinfo *p;
  int serverfd;
  int err;

  hints = (struct addrinfo){
      .ai_family = AF_UNSPEC,
      .ai_socktype = SOCK_STREAM,
      .ai_flags = AI_PASSIVE,
  };

  err = getaddrinfo(addr, port, &hints, &res);
  if (err) {
    if (err < 0)
      err = -err;
    print_errno("getaddrinfo failed", err);
    return err;
  }

  for (p = res; p != NULL; p = p->ai_next) {
    serverfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (serverfd < 0) {
      continue;
    }

    setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, NULL, 0);
    if (bind(serverfd, p->ai_addr, p->ai_addrlen) == 0)
      break;
    err = errno;
    close(serverfd);
  }

  if (NULL == p) {
    print_errno("failed to bind", err);
    freeaddrinfo(res);
    return err;
  }
  freeaddrinfo(res);

  err = listen(serverfd, 16);
  if (err < 0) {
    print_errno("failed to listen", errno);
    return errno;
  }

  printf("listening on %s:%s\n", addr, port);

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
    if (clientfd < 0) {
      print_errno("accept client failed", errno);
      continue;
    }

    childp = fork();
    if (childp < 0) {
      print_errno("failed to fork", errno);
      close(clientfd);
      continue;
    } else if (childp != 0) {
      printf("forked to accept client. PID=%d\n", childp);
      close(clientfd);
      continue;
    }

    err = getnameinfo((struct sockaddr *)client, client_len, host, NI_MAXHOST,
                      serv, NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV);
    if (err != 0) {
      print_errno("failed to getnameinfo", errno);
      break;
    }

    printf("[PID%d] connection from %s:%s\n", getpid(), host, serv);

    err = handle_client(clientfd);

    close(clientfd);
    break;
  };

  free(host);
  free(serv);
  free(client);

  return err;
}

static int handle_client(const int fd) {
  int err;
  char *client_v_str;
  unsigned short client_v_maj;
  unsigned short client_v_min;
  unsigned short client_v_pat;

  client_v_str = malloc_read_string(fd);
  if (client_v_str == NULL) {
    print_errno("failed to read client version string", errno);
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
    print_errno("failed to read client version", errno);
    free(client_v_str);
    return errno;
  }

  printf("[PID%d] client %s (%hu.%hu.%hu)\n", getpid(), client_v_str,
         client_v_maj, client_v_min, client_v_pat);

  // version sanity check, semantic versioning
  if ((unsigned short)VERSION_MAJOR != client_v_maj ||
      (unsigned short)VERSION_MINOR < client_v_min) {
    free(client_v_str);
    write_ushort(fd, EPROTO);
    return EPROTO;
  }

  write_ushort(fd, 0);

  free(client_v_str);
  return 0;
}
