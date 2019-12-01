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
#include <semaphore.h>

#define BUFSIZE 4096
#define MAXCLIENT 10

// function declarations
void printOptions(struct commandOptions cmdOps, int argc, char **argv);
void *handle_connection(void* arg);
void release_socket(int fd);
void *get_in_addr(struct sockaddr *sa);
int no_connections_left();
int client(int p_opt, unsigned int src_port, int timeout, int log_mode, unsigned int port, char * hostname);
void *handle_std_in(void* arg);
void set_socket(struct addrinfo **p, struct addrinfo *servinfo, const int *yes);
void std_in_thread(); 

int sockfd;
int clients[MAXCLIENT];
sem_t sem;
int k;
int log_mode;
char port[12];

int main(int argc, char *argv[])
{
    //check command line options
    struct commandOptions cmdOps;
    parseOptions(argc, argv, &cmdOps);


    int new_socket;  // listen on sock_fd, new connection on new_socket
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    int client_limit = 1;
    log_mode = 0;

    if(cmdOps.option_v) {
        log_mode = 1;
        printOptions(cmdOps, argc, argv);
    }

    if(cmdOps.option_l != 1) {
        int p_opt;
        int src_port;
        int w; 
        int timeout;
        p_opt = cmdOps.option_p;
        src_port = cmdOps.source_port;
        timeout = cmdOps.timeout;

        if(cmdOps.port == 0 || cmdOps.hostname == NULL) {
            fprintf(stderr,"ERROR:server port/server hostname not provided\n");
            exit(0);
        }
        int retval = client(p_opt,src_port,timeout,log_mode, cmdOps.port, cmdOps.hostname);
        if(retval ==2 ) {
            exit(0);
        }
    }

    k = cmdOps.option_k;

    if(cmdOps.option_r) {
        client_limit = 10;
    }

    if (sem_init(&sem, 0, client_limit) == -1) {
        perror("sem initialization\n");
    }
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    sprintf(port,"%d",cmdOps.port);
    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    set_socket(&p, servinfo, &yes);

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, 10) == -1) {
        perror("listen");
        exit(1);
    }
    fprintf(stderr,"server: waiting for connections...\n");
     //set all sockets to -1 
    for(int i =0; i < MAXCLIENT; i++ ){
        clients[i] = -1;
    }

    // create new thread for standard input
    std_in_thread();

    while(1) {  // main accept() loop
        int limit_reached = 1;
        int idx = 0;
        for(int i =0; i < MAXCLIENT; i++) { 

            if (clients[i] == -1) {
                limit_reached = 0;
                idx = i;
                break;
            }
        }

        sin_size = sizeof their_addr;
        sem_wait(&sem);
        new_socket = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        clients[idx] = new_socket;
        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        fprintf(stderr, "server: got connection from %s\n", s);

        int *client_socket = malloc(sizeof(int));
        *client_socket = new_socket;

        void* thread = createThread(handle_connection, client_socket);
        runThread(thread, NULL);

        if (new_socket == -1) {
            perror("accept failed");
            continue;
        }
    }
  return 0;
}

// handle the client and standard input connection
void *handle_connection(void* arg) {
    char buffer[BUFSIZE];
    int fd = *((int *)arg);
    while(1) {
        int rv = 0;
        if (fd == 0) {
            rv = read(fd, buffer, BUFSIZE);
            buffer[rv] = '\0';
        } else {
            rv = recv(fd, buffer, BUFSIZE, 0);
            buffer[rv] = '\0';
        }
        if(rv == -1 ) {
             perror("client: recv failed");
        }
        if (rv == 0) {
            printf("client connection closed\n");
            release_socket(fd);
            close(fd);
            free(arg);
            break;
        }

        if (fd != 0) {
            fprintf(stdout, buffer);
        }
        for(int i = 0 ; i < MAXCLIENT;i++) {
            int socket = clients[i];
            if (socket != fd && socket !=-1) {
               rv =  send(socket, buffer, BUFSIZE, 0);
               if(rv == -1) {
                   clients[i] = -1;
                   perror("client: send failed");
               }
            }
        }
        bzero(buffer, BUFSIZE*sizeof(char));
    }
    sem_post(&sem);
    if(no_connections_left() && k == 0) {
        if(log_mode) {
            printf("Closing server, Last client exited\n");
        }
        close(sockfd);
        exit(EXIT_SUCCESS);
    }
}

int no_connections_left() {
    for(int i =0; i < MAXCLIENT; i++ ) {
        if(clients[i] != -1) {
            return 0;
        }
    }
    return 1;
}
// when socket connection is closed, set its value to -1 in clients array 
void release_socket(int fd) {
    for(int i = 0; i < MAXCLIENT; i++) {
        if(clients[i] == fd) {
            if(log_mode) {
                printf("releasing socket %d\n", fd);
            }
            clients[i] = -1;
            break;
        }
    }
}

void *get_in_addr(struct sockaddr *sa)
{
	return &(((struct sockaddr_in*)sa)->sin_addr);
}

void std_in_thread() {
    int *std_in = malloc(sizeof(int));
    *std_in = 0;
    void* std_in_thread = createThread(handle_connection, std_in);
    runThread(std_in_thread, NULL);
}
void set_socket(struct addrinfo **p, struct addrinfo *servinfo, const int *yes) {
    for(*p = servinfo; *p != NULL; *p = (*p)->ai_next) {
        if ((sockfd = socket((*p)->ai_family, (*p)->ai_socktype,
                             (*p)->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }
        
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, yes,
                       sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }
        
        if (bind(sockfd, (*p)->ai_addr, (*p)->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        
        break;
    }
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

//#################################CLIENT SIDE#################################

char server_port[12];
int sockfd;
int client(int p_opt, unsigned int src_port, int timeout, int log_mode, unsigned int port, char * hostname)
{
	int numbytes;  
	char buf[BUFSIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	sprintf(server_port,"%d", port);
	if ((rv = getaddrinfo(hostname, server_port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			perror("client: connect");
			close(sockfd);
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}
    freeaddrinfo(servinfo); // all done with this structure
    //create new thread for standard input
    int *std_in = malloc(sizeof(int));
    *std_in = 0;
    void* std_in_thread = createThread(handle_std_in, std_in);
    runThread(std_in_thread, NULL);
	//inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    if(log_mode) {
        printf("client: connected to %s at port %d\n", hostname, port);
    }
    while(1) {
		numbytes = recv(sockfd, buf, BUFSIZE-1, 0);
		if (numbytes == -1) {
	    perror("recv");
	    exit(1);
		}
		fprintf(stdout,buf);
		buf[numbytes] = '\0';
    }

	return 0;
}

void *handle_std_in(void* arg) {
	char buffer[BUFSIZE];
    int fd = *((int *)arg);
    int ret = 0;
	 while(1) {
        ret = read(fd, buffer, BUFSIZE);
        buffer[ret] = '\0';
		if(ret == -1 ) {
            perror("client: recv failed");
        }
		ret = send(sockfd, buffer, BUFSIZE,0);
            if(ret == -1) {
            	perror("client: send failed");
            }
	 }
}
