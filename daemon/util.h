#ifndef _DAEMON_UTIL_H
#define _DAEMON_UTIL_H

#include <stdbool.h>
#include <stdint.h>

// something like rm $path/*
int rm_r(const char *const path);
// something like mkdir -p $path
int mkdir_p(const char *path);
// get the time from network
// returns <0 and sets errno on failure
int64_t get_network_time(void);

// find first removable block device. returns malloc'd path (e.g.
// "/dev/block/sda1") or NULL on fail. caller must free.
char *find_removable_blockdev(void);

// true=OEM lock, false=OEM unlock
int set_oem_lock(bool);
// true=enable wifi, false=disable
int set_wifi_enabled(bool);
// true=enable ADB, false=disable
int set_adb_enabled(bool);

int mount_copy_unmount_ns(const char *device, const char *mountpoint,
                          const char *fstype, unsigned long mount_flags,
                          const char *mount_data, const char *copy_src,
                          const char *copy_dst, int _dumbos_client_socketfd);
// construct the URL with query parameters for dumb server requests.
// set <code> to NULL if it's not needed
// return NULL and set errno on error
char *alloc_construct_request_URL(const char *baseURL, const char *user,
                                  const char *requestid, const char *requestsig,
                                  int64_t net_time, const char *code);

// used be main() on startup
// sets the text on the lockscreen, from the 'fancy name' of the user
// sets errno and returns non-0 on error, 0 on success
// errno==EOWNERDEAD means no user available (finally! a place to use that error
// code)
int maybe_set_lockscreen_text(void);

#endif //
