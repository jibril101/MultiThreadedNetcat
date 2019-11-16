#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include "commonProto.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>

// function declarations
void printOptions(struct commandOptions cmdOps, int argc, char **argv);

typedef enum {false, true} bool;

// struct to keep track of client's file descriptors
struct client_fds {
  bool in_use;
  int fd;
};

int main(int argc, char **argv) {

  struct commandOptions cmdOps;
  struct client_fds fd_array[10]; // array of structs

  printOptions(cmdOps, argc, argv);

  // TODO: setup socket, bind, and listen
  



 

  return 0;
}

void printOptions(struct commandOptions cmdOps, int argc, char **argv) {

  int retVal = parseOptions(argc, argv, &cmdOps);
  printf("Command parse outcome %d\n", retVal);

  printf("-k = %d\n", cmdOps.option_k);
  printf("-l = %d\n", cmdOps.option_l);
  printf("-v = %d\n", cmdOps.option_v);
  printf("-r = %d\n", cmdOps.option_r);
  printf("-p = %d\n", cmdOps.option_p);
  printf("-p port = %d\n", cmdOps.source_port);
  printf("Timeout value = %d\n", cmdOps.timeout);
  printf("Host to connect to = %s\n", cmdOps.hostname);
  printf("Port to connect to = %d\n", cmdOps.port);  
}
