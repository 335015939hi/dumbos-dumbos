#ifndef _DAEMON_DAEMON_H
#define _DAEMON_DAEMON_H

#include <sys/types.h>

struct daemon_opts {
  // temp directory
  const char *tmpdir;
  // server and port
  const char *server;
  const char *port;
  // path of socket
  const char *path;
  // sokcet permissions: mode, owner, group, selinux context
  mode_t mode;
  uid_t uid;
  gid_t gid;
  const char *con;
};

// main daemon program
int start_daemon(const struct daemon_opts *const opts);

#endif
