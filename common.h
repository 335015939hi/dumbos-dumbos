#ifndef _DUMBOS_COMMON_H
#define _DUMBOS_COMMON_H

#include <string.h>
#include <unistd.h>

#define __STR__(x) #x
#define _STR_(x) __STR__(x)

#define _VERSION_MAJOR 1
#define _VERSION_MINOR 0
#define _VERSION_PATCH 0
#define _VERSION_STRING                                                        \
  "v" _STR_(VERSION_MAJOR) "." _STR_(VERSION_MINOR) "." _STR_(VERSION_PATCH)

// whether to compile with unsafe debug features
// comment this out if you're not debugging
#define DEBUG_MODE 1

// full path of the default socket
#define DEFAULT_SOCKET_PATH "/dev/socket/dumbosd.socket"
// default server
#define DEFAULT_SERVER "192.168.12.1"
// default port
#define DEFAULT_PORT "3850"
// default socket permissions
#define DEFAULT_SOCKET_MODE "666"
// default socket ownership
#define DEFAULT_SOCKET_OWNER "root:system"

// extension of signature file
#define EXT_SIG ".sig"
// extension of code (not secret code) file
#define EXT_CODE ""

// max string length, including NULL terminator
// strings over this length will throw error somewhere
#define MAX_STRING 4096

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
// #ifdef LOG_USE_PID
#define _LOG_PREFIX(f, p)                                                      \
  fprintf(f, "[%s][%ld]%s ", timestamp(), (long)getpid(), p)
// #else
// #define _LOG_PREFIX(f, p) fprintf(f, "[%s]%s", timestamp(), p)
// #endif
#define _LOG(v, f, p, s, ...)                                                  \
  do {                                                                         \
    if (log_verbosity >= v) {                                                  \
      _LOG_PREFIX(f, p);                                                       \
      fprintf(f, s, ##__VA_ARGS__);                                            \
      fprintf(f, "\n");                                                        \
    }                                                                          \
  } while (0)
#define LOG(s, ...)                                                            \
  _LOG(LOG_VERBOSITY_NORMAL, stderr, "[     ]", s, ##__VA_ARGS__)
#define LOG_FATAL(s, ...)                                                      \
  _LOG(LOG_VERBOSITY_FATAL, stderr, "[FATAL]", s, ##__VA_ARGS__)
#define LOG_ERR(s, ...)                                                        \
  _LOG(LOG_VERBOSITY_ERROR, stderr, "[ERROR]", s, ##__VA_ARGS__)
#define LOG_WARN(s, ...)                                                       \
  _LOG(LOG_VERBOSITY_WARN, stderr, "[WARN ]", s, ##__VA_ARGS__)
#define LOG_VERBOSE(s, ...)                                                    \
  _LOG(LOG_VERBOSITY_VERBOSE, stderr, "[VRBOS]", s, ##__VA_ARGS__)
#define LOG_DEBUG(s, ...)                                                      \
  _LOG(LOG_VERBOSITY_DEBUG, stderr, "[DEBUG]", s, ##__VA_ARGS__)
#define LOG_WARN_ERRNO(s, err) LOG_WARN("%s:%s", s, strerror(err))
#define LOG_ERRNO(s, err) LOG_ERR("%s:%s", s, strerror(err))
#define LOG_ERROR_ERRNO(s, err) LOG_ERRNO(s, err)
#define LOG_FATAL_ERRNO(s, err) LOG_FATAL("%s:%s", s, strerror(err))
// global variable controlling log verbosity
extern int log_verbosity;
// verbosity defs, the lower the less verbose
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
// sets log verbosity. accepts one of LOG_*_NAME (case insensative), returns 0
// on success
int set_log_verbosity(const char *const lvl);

// command codes, for server to daemon communication. type should be unsigned
// short
#define SERVER_CMD_SECRET_CODE 357
#define SERVER_CMD_OK 128

// allowed characters for the secret code
#define SECRET_CODE_ALLOWED_CHARS "qwertyuiopasdfghjklzxcvbnm0123456789-_=+,"

// takes a string and tries to parse it as unsigned base 10 integer short.
// returns -1 on fail, setting errno
signed long parse_ushort(const char *str);

// returns timestamp. beware the returned string is statically allocated and
// will be overwritten on next call
const char *timestamp();

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

#endif
