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
#include "Thread.h"

#define BUFSIZE 4096

//#define BACKLOG 1 // queue of pending connections

// function declarations
void printOptions(struct commandOptions cmdOps, int argc, char **argv);

typedef enum {false, true} bool;

// struct to keep track of client's file descriptors
typedef struct client_fd {
  int fd;
  bool in_use;
} Client; 

Client clients[11];

// get sockaddr, IPv4 
void *get_in_addr(struct sockaddr *sa)
{
return &(((struct sockaddr_in*)sa)->sin_addr);
}

void *handle_connection(void* arg) {
    int count = 0;
    printf("running client thread\n");
    char buffer[BUFSIZE];
    int fd = *((int *)arg);
    while(true && count < 100) {
        count++;
        printf("waiting for request from client\n");
        if(recv(fd, buffer, BUFSIZE, 0) == -1 ) {
             perror("client: recv");
        }
        printf("CLIENT REQUEST: %s\n", buffer);
    }
    close(fd);
}
int main(int argc, char *argv[])
{

    //Client *clients = malloc(12*sizeof(Client));

    int sockfd, new_socket;  // listen on sock_fd, new connection on new_socket
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    // Check that port is provided as an argument
    if (argc != 2){
        fprintf(stderr, "usage server portnumber\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, 10) == -1) {
        perror("listen");
        exit(1);
    }
    printf("server: waiting for connections...\n");

    int index = 0;
    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_socket = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_socket == -1) {
            perror("accept failed");
            continue;
        }
         //for threading, put it into the list of active fds
        Client client = {new_socket, true};
        while(index < 12) {
            clients[index] = client;
            index++;
        }
        // do whatever needs to be done with the connection
        //handle_connection(new_socket);

        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        printf("server: got connection from %s\n", s);

        void* thread = createThread(handle_connection, &new_socket);
        printf("created thread\n");
        runThread(thread, NULL);
        printf("after run thread\n");


        //close connection
        close(new_socket);
        //exit server
        exit(0);
    }

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
