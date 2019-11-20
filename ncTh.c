#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include "commonProto.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>


#define BACKLOG 10 // queue of pending connections

// function declarations
void printOptions(struct commandOptions cmdOps, int argc, char **argv);

typedef enum {false, true} bool;

// struct to keep track of client's file descriptors
struct client_fds {
  bool in_use;
  int fd;
};

char client_message[2000];
char buffer[1024];
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *socketThread(void *arg) {
  int newSocket = *((int *)arg);
  recv(newSocket, client_message, 2000, 0);
}

int main(int argc, char **argv) {

  struct commandOptions cmdOps;
  struct client_fds fd_array[10]; // array of structs
  char portNumber[5];
  
  sprintf(portNumber, cmdOps.source_port); // convert int port to char*
  puts(portNumber);

  printOptions(cmdOps, argc, argv);

  // TODO: setup socket, bind, and listen
  int sock_fd, new_fd; // listen on sock_fd, new connection on new_fd

  int socket;
  struct addrinfo hints, *result, *pointer;
  struct sockaddr_storage connector_addr; // connector's address info
  socklen_t sin_size;
  struct sigaction sa;
  int yes = 1;
  char s[INET6_ADDRSTRLEN];
  int rv;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  






 

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
