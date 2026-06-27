#ifndef _COMMON_H
#define _COMMON_H

#define VERSION_CODE 0
#define VERSION_STRING "v0.0"

//whether to compile with unsafe debug features
#define DEBUG_MODE 1


//full path of the default socket
#define DEFAULT_SOCKET_PATH "/dev/socket/dumbosd.socket"
//default server
#define DEFAULT_SERVER "192.168.12.1"
//default port
#define DEFAULT_PORT 3850


void print_error(const char*const);


#endif
