#ifndef _DAEMON_DAEMON_H
#define _DAEMON_DAEMON_H

#include <sys/types.h>

struct daemon_opts {
  // temp directory
  const char *tmpdir;
  // server URL
  const char *server;
  // path of socket
  const char *path;
  // sokcet permissions: mode, owner, group, selinux context
  mode_t mode;
  uid_t uid;
  gid_t gid;
  const char *con;
};

// default dumb server
#ifdef DEBUG_MODE
#define DUMB_DEFAULT_SERVER "http://10.0.2.2:8080/"
#else
#define DUMB_DEFAULT_SERVER "https://91-226-221-47.sslip.io:4738/"
#endif
// sets default dumb server to <server>. takes ownership of <server>.
void set_default_server(const char *server);
// gets the URL for the servers for secret codes, and uploads
const char *get_dumb_server_code();
const char *get_dumb_server_upload();
// get/set the path to tmpdir
const char *get_tmpdir();
void set_tmpdir(const char *path);

// main daemon program
int start_daemon(const struct daemon_opts *const opts);

#endif
