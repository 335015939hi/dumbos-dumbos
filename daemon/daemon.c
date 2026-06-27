
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <stdlib.h>

#include "../common.h"
#include "daemon.h"

int start_daemon(const char* const socket_path){
  int socket_fd;
  
  socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    print_error("socket");
    return 1;
  }
  
  return 0;
}
