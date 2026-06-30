#ifndef _SERVER_SERVER_H
#define _SERVER_SERVER_H

#include <netinet/in.h>

int do_server(const char *const addr, const char *const port,
              const char *const code_dir, const char *const code_persist_dir);

#endif
