#ifndef _COMMON_H
#define _COMMON_H

#define _VERSION_CODE 000001
#define _VERSION_STRING "v0.0.1"

//whether to compile with unsafe debug features
//comment this out if you're not debugging
#define DEBUG_MODE 1


//full path of the default socket
#define DEFAULT_SOCKET_PATH "/dev/socket/dumbosd.socket"
//default server
#define DEFAULT_SERVER "192.168.12.1"
//default port
#define DEFAULT_PORT 3850


#ifdef DEBUG_MODE
#define VERSION_STRING _VERSION_STRING "-debug"
#define VERSION_CODE (-_VERSION_CODE)
#else
#define VERSION_STRING _VERSION_STRING
#define VERSION_CODE _VERSION_CODE
#endif

void print_error(const char*const);


#endif
