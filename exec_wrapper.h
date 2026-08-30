#ifndef _EXEC_WRAPPER_H
#define _EXEC_WRAPPER_H

// a wrapper around fork() and execv()
// returns -1 and sets errno if some component fails (e.g. fork() or execv())
// returns as if executed command in sh otherwise (return value, or signal
// number+128)
int execv_wrapper(const char *path, char *const argv[]);

#endif
