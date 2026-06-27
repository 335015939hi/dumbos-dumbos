
#include <unistd.h>
#include <string.h>

#include "common.h"

void print_error(const char*const error_string){
  write(STDERR_FILENO,error_string,strlen(error_string));
}
