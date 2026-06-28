#ifndef _COMMON_H
#define _COMMON_H

#define _VERSION_MAJOR 0
#define _VERSION_MINOR 1
#define _VERSION_PATCH 0
#define _VERSION_STRING "v0.1.0"

// whether to compile with unsafe debug features
// comment this out if you're not debugging
#define DEBUG_MODE 1

// full path of the default socket
#define DEFAULT_SOCKET_PATH "/dev/socket/dumbosd.socket"
// default server
#define DEFAULT_SERVER "192.168.12.1"
// default port
#define DEFAULT_PORT 3850

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

void print_error(const char *const);

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

#endif
