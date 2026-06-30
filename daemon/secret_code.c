
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../common.h"
#include "command.h"

// caller expects returned string to be free()able, so we cant just 'return
// "whatever";'
static char *malloc_str(const char *const s) {
  char *ret = malloc(strlen(s) + 1);
  if (ret == NULL)
    abort();
  strcpy(ret, s);
  return ret;
}

char *secret_code(int argc, char **argv, int *ret_val, const char *const host,
                  const char *const port) {
  if (argc != 1) {
    *ret_val = EINVAL;
    print_error("secret_code: expected exactly ONE code\n");
    return malloc_str("secret_code: expected exactly ONE code\n");
  }

  struct addrinfo *hints;
  struct addrinfo *res;
  struct addrinfo *p;
  int err;
  int fd;

  hints = malloc(sizeof(struct addrinfo));
  memset(hints, 0, sizeof(struct addrinfo));
  hints->ai_family = AF_UNSPEC; // IPv4 or IPv6
  hints->ai_socktype = SOCK_STREAM;

  fprintf(stdout, "using server=%s:%s\n", host, port);
  err = getaddrinfo(host, port, hints, &res);
  free(hints);
  if (err != 0) {
    *ret_val = err;
    print_errno("failed to getaddr", err);
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
    print_errno("failed to connect", err);
    *ret_val = err;
    return NULL;
  }

  //write version to server
  err=write_string(fd,VERSION_STRING);
  if(err==0){
    err=write_ushort(fd,(unsigned short)VERSION_MAJOR);
    if(err==0){
      err=write_ushort(fd,(unsigned short)VERSION_MINOR);
      if(err==0){
        err=write_ushort(fd,(unsigned short)VERSION_PATCH);
      }
    }
  }
  if(err!=0){
    *ret_val=errno;
    print_errno("failed to send version",errno);
    return NULL;
  }

  //read status
  err=read_ushort(fd);
  if(err<0){
    *ret_val=errno;
    print_errno("failedd to read server status",errno);
    return NULL;
  }else if(err!=0){
    *ret_val=err;
    print_errno("server doesn't like us",err);
    return malloc_str(strerror(err));
  }


  *ret_val = 0;
  return NULL;
}
