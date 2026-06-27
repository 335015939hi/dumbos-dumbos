#ifndef _COMMON_H
#define _COMMON_H

#define VERSION_CODE 0
#define VERSION_STRING "v0.0"

//whether to compile with unsafe debug features
#define DEBUG_MODE 1


//parent folder of the default socket, useful for mkdir
#define DEFAULT_SOCKET_DIRNAME "/dev/socket"
//file name of default socket
#define DEFAULT_SOCKET_NAME "dumbos.socket"
//full path of the default socket
#define DEFAULT_SOCKET_PATH DEFAULT_SOCKET_DIRNAME DEFAULT_SOCKET_NAME


void print_error(const char*const);


#endif
