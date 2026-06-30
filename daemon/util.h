#ifndef _DAEMON_UTIL_H
#define _DAEMON_UTIL_H

// something like rm $path/*
int rm_r(const char *const path);
// something like mkdir -p $path
int mkdir_p(const char *path);
// verify signature, 0 on success (verified), <0 on error (including bad
// signature) both arguments are paths
int verify_sig(const char *file, const char *sig);

#endif //
