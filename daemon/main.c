
#include <stdio.h>
#include <getopt.h>
#include <errno.h>

#include "../common.h"

static const struct option long_opts[];
static const char * const short_opts;
void display_help();


int main (int argc,char ** argv){
  #ifdef DEBUG_MODE
  printf("WARNING: running in debug mode. if you are not developing this program, please report this to someone.\n");
  #endif

  bool error=0;
  int option;

  while(-1!=(option = getopt_long(argc, argv, short_opts, long_opts, NULL))){
    switch(option){
      case 'h':
        display_help();
        break;
      default:
        printf("unknown option\n");
        error=EINVAL;
        break;
    }
    if (error) return error;
  }

  return 0;
}


static const struct option long_opts[]={
  {"help",no_argument,0,'h'},
  {0,0,0,0}
};
static const char * const short_opts="h";

void display_help(){
  const char * const help_text=
    #ifdef DEBUG_MODE
    "debug mode options:\n"
    #endif
    "options:\n"
    " --help,-h display this help text\n"

  ;
  printf(help_text);
}
