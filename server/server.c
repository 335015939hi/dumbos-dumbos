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

  freeaddrinfo(res);

  if (NULL == p) {
    print_errno("failed to bind", err);
    return err;
  }

  err = listen(serverfd, 16);
  if (err < 0) {
    print_errno("failed to listen", errno);
    return errno;
  }

  printf("listening on %s:%s", addr, port);

  struct sockaddr_storage *client;
  socklen_t client_len;
  pid_t childp;
  int clientfd;
  char *host;
  char *serv;

  host = malloc(NI_MAXHOST);
  serv = malloc(NI_MAXSERV);
  client_len = sizeof(client);
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

    getnameinfo((struct sockaddr *)&client, client_len, host, NI_MAXHOST, serv,
                NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV);

    printf("%d connection from %s:%s\n", getpid(), host, serv);

    write_ushort(clientfd, 1234);

    close(clientfd);
    break;
  };

  free(host);
  free(serv);
  free(client);

  return 0;
}
