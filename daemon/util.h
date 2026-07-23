#ifndef _DAEMON_UTIL_H
#define _DAEMON_UTIL_H

#include <stdbool.h>

// something like rm $path/*
int rm_r(const char *const path);
// something like mkdir -p $path
int mkdir_p(const char *path);
// alloc a string and put current epoch seconds from network, or NULL and set
// errno on error
char *geturltime(void);

// true=OEM lock, false=OEM unlock
int set_oem_lock(bool);
// true=enable wifi, false=disable
int set_wifi_enabled(bool);
// true=enable ADB, false=disable
int set_adb_enabled(bool);

#endif //
