#ifndef _DUMBOS_COMMON_H
#define _DUMBOS_COMMON_H

/*
 * defines some common functions and constants and macros
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

#define __STR__(x) #x
#define _STR_(x) __STR__(x)

#define _VERSION_MAJOR 3
#define _VERSION_MINOR 1
#define _VERSION_PATCH 1
#define _VERSION_STRING                                                        \
  "v" _STR_(VERSION_MAJOR) "." _STR_(VERSION_MINOR) "." _STR_(VERSION_PATCH)

// whether to compile with unsafe debug features
// comment this out if you're not debugging
// #define DEBUG_MODE 1

#ifdef DEBUG_MODE
#define VERSION_STRING _VERSION_STRING "-debug"
#define VERSION_MAJOR (-_VERSION_MAJOR)
#define VERSION_MINOR (-_VERSION_MINOR)
#define VERSION_PATCH (-_VERSION_PATCH)
#else
#define VERSION_STRING _VERSION_STRING
#define VERSION_MAJOR _VERSION_MAJOR
#define VERSION_MINOR _VERSION_MINOR
#define VERSION_PATCH _VERSION_PATCH
#endif

// logging functions
#ifndef __ANDROID__
// for non-android:we print to _log_output, or stderr by default
// also include timestamp and PID
#define _LOG_PREFIX(f, p)                                                      \
  fprintf(f, "[%s][%ld]%s ", timestamp(), (long)getpid(), p)
#define _LOG(v, p, s, ...)                                                     \
  do {                                                                         \
    if (log_verbosity >= v) {                                                  \
      _LOG_PREFIX(stderr, p);                                                  \
      fprintf(stderr, s __VA_OPT__(, ) __VA_ARGS__);                           \
      fprintf(stderr, "\n");                                                   \
      fflush(stderr);                                                          \
    }                                                                          \
  } while (0)
#define LOG(s, ...)                                                            \
  _LOG(LOG_VERBOSITY_NORMAL, "[     ]", s __VA_OPT__(, ) __VA_ARGS__)
#define LOG_FATAL(s, ...)                                                      \
  _LOG(LOG_VERBOSITY_FATAL, "[FATAL]", s __VA_OPT__(, ) __VA_ARGS__)
#define LOG_ERR(s, ...)                                                        \
  _LOG(LOG_VERBOSITY_ERROR, "[ERROR]", s __VA_OPT__(, ) __VA_ARGS__)
#define LOG_WARN(s, ...)                                                       \
  _LOG(LOG_VERBOSITY_WARN, "[WARN ]", s __VA_OPT__(, ) __VA_ARGS__)
#define LOG_VERBOSE(s, ...)                                                    \
  _LOG(LOG_VERBOSITY_VERBOSE, "[VRBOS]", s __VA_OPT__(, ) __VA_ARGS__)
#define LOG_DEBUG(s, ...)                                                      \
  _LOG(LOG_VERBOSITY_DEBUG, "[DEBUG]", s __VA_OPT__(, ) __VA_ARGS__)
#else //__ANDROID
// logging for android. we will use android's __android_log_print to print to
// logcat
//__DUMBOS_CLIENT or __DUMBOSD__ should be defined at compile-time as part of
// Android.bp, so automatically defined by the build system
#ifdef __DUMBOSD__
#define LOG_TAG "Dumbos daemon"
#else
#ifdef __DUMBOS_CLIENT
#define LOG_TAG "Dumbos client"
#else
#define LOG_TAG "Dumbos"
#endif
#endif

#define _LOG(v, s, ...)                                                        \
  __android_log_print(v, LOG_TAG, s __VA_OPT__(, ) __VA_ARGS__)
#define LOG(s, ...) _LOG(ANDROID_LOG_INFO, s __VA_OPT__(, ) __VA_ARGS__)
#define LOG_FATAL(s, ...) _LOG(ANDROID_LOG_FATAL, s __VA_OPT__(, ) __VA_ARGS__)
#define LOG_ERR(s, ...) _LOG(ANDROID_LOG_ERROR, s __VA_OPT__(, ) __VA_ARGS__)
#define LOG_WARN(s, ...) _LOG(ANDROID_LOG_WARN, s __VA_OPT__(, ) __VA_ARGS__)
#define LOG_DEBUG(s, ...) _LOG(ANDROID_LOG_DEBUG, s __VA_OPT__(, ) __VA_ARGS__)
#define LOG_VERBOSE(s, ...)                                                    \
  _LOG(ANDROID_LOG_VERBOSE, s __VA_OPT__(, ) __VA_ARGS__)

#endif //__ANDROID__

// alias for LOG_ERR
#define LOG_ERROR(s, ...) LOG_ERR(s __VA_OPT__(, ) __VA_ARGS__)
// prints message and converts a POSIX errno to error message
#define LOG_ERRNO(s, err) LOG_ERR("%s:%s", s, strerror(err))
#define LOG_ERROR_ERRNO(s, err) LOG_ERRNO(s, err)
#define LOG_FATAL_ERRNO(s, err) LOG_FATAL("%s:%s", s, strerror(err))
#define LOG_WARN_ERRNO(s, err) LOG_WARN("%s:%s", s, strerror(err))
// global variable controlling log verbosity
// no effect for android
extern int log_verbosity;
// verbosity defs, the lower the less verbose
// no effect for android.
// FIXME: swap verbose and debug, to be the same as android
#define LOG_VERBOSITY_NONE 0
#define LOG_VERBOSITY_FATAL 7
#define LOG_VERBOSITY_ERROR 124
#define LOG_VERBOSITY_WARN 200
#define LOG_VERBOSITY_NORMAL 387
#define LOG_VERBOSITY_VERBOSE 1244
#define LOG_VERBOSITY_DEBUG 4732
#define LOG_VERBOSITY_MAX 9999
#define LOG_NONE_NAME "none"
#define LOG_FATAL_NAME "fatal"
#define LOG_ERROR_NAME "error"
#define LOG_WARN_NAME "warn"
#define LOG_NORMAL_NAME "normal"
#define LOG_VERBOSE_NAME "verbose"
#define LOG_DEBUG_NAME "debug"
#define LOG_MAX_NAME "max"
// sets log verbosity. no effect for android.
// accepts one of LOG_*_NAME (case insensative), returns 0
// on success
int set_log_verbosity(const char *const lvl);

// takes a string and tries to parse it as unsigned base 10 integer short.
// returns -1 on fail, setting errno
signed long parse_ushort(const char *str);

// returns timestamp. beware the returned string is statically allocated and
// will be overwritten on next call
const char *timestamp(void);

// reads unsigned short from fd, return <0 on error and sets errno
signed long read_ushort(const int fd);
// writes unsigned short to fd, <0 on error and sets errno
int write_ushort(const int fd, const unsigned short ushort);
// write a byte, <0 on error
int write_byte(const int fd, const unsigned char c);
// read a unsigned byte, <0 on error
int read_byte(const int fd);
// malloc() and read string (that was written with write_string()).returns NULL
// on error, and sets errno. don't forget to free!
char *malloc_read_string(const int fd);
// write a string, to be read with read_string
int write_string(const int fd, const char *const s);
// read or write len. guranteed to do finish len or fail (return <0 on fail)
ssize_t read_all(int fd, void *buf, size_t len);
ssize_t write_all(int fd, const void *buf, size_t len);

// read and write to file. both return <0 on error and 0 on success
int read_file(const int fd, const char *const dest);
int write_file(const int fd, const char *const path);
// maximum size we will allow, for *_file() above
#define MAX_FILE_SIZE 262144

// parse a string into signed long. return 0 on success and -1 on error. chatgpt
// code.
int parse_long_long(const char *str, signed long long *ret);

#define maybe_free(buf)                                                        \
  do {                                                                         \
    if (buf) {                                                                 \
      free(buf);                                                               \
      buf = NULL;                                                              \
    }                                                                          \
  } while (0)

// check string <s> against character whitelist <whitelist>. returns true if all
// characters in <s> are in <whitelist>, false otherwise
bool check_allowed_chars(const char *s, const char *whitelist);

// convert a buffer to hex string, or write a hex string into a buffer. caller
// owns returned buffer. returns NULL and sets errno on failure
char *malloc_buf_to_hex(const void *buf, size_t size);
void *malloc_hex_to_buf(const char *hex);

#endif
