#ifndef _DAEMON_UTIL_H
#define _DAEMON_UTIL_H

// something like rm $path/*
int rm_r(const char *const path);
// something like mkdir -p $path
int mkdir_p(const char *path);
// alloc a string and put current epoch seconds from network, or NULL and set
// errno on error
char *geturltime(void);

#endif //
