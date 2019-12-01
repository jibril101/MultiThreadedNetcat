#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include "commonProto.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>

#define MAXDATASIZE 100 // max number of bytes we can get at once 

char PORT[12];	// port server is listening on
int verbose = 0;


// function prototypes
void printOptions(int argc, char **argv, struct commandOptions cmdOps);
void *get_in_addr(struct sockaddr *sa);
int get_listener_socket(char *PORT);
int add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size);
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count);
int start_server(struct commandOptions cmdOps, int num_cons);
void *get_in_port(struct sockaddr *sa);
int start_client(struct commandOptions cmdOps);



int main(int argc, char **argv) {

  	struct commandOptions cmdOps;
	parseOptions(argc, argv, &cmdOps);

    // print verbose output
    if (cmdOps.option_v) {
        verbose = 1;
 	    printOptions(argc, argv, cmdOps);
    }

	 if (cmdOps.option_k && cmdOps.option_l == 0) {
		 fprintf(stderr, "error: cannot use -k without -l option\n");
         return 0;
	 } else if (cmdOps.option_r && cmdOps.option_l) {
         // run server with 10 connections allowed, close server after all connections closed
         start_server(cmdOps, 10);
     } else if (cmdOps.option_k && cmdOps.option_l) {
		 // keep listening for new connections even after last connection closes, do not close server
         // only allow one connection to be made
		 start_server(cmdOps, 1);
     } else if (cmdOps.option_l && cmdOps.option_p) {
         fprintf(stderr, "error: cannot use -p with -l option\n");
         return 0;
     } else if ((cmdOps.option_l && cmdOps.timeout) || cmdOps.option_l) {
         // run server with 1 connection allowed, ignoring timeout -w option, close server after connection closed
         start_server(cmdOps, 1);
     }

	 if (cmdOps.hostname != NULL) {
		 start_client(cmdOps);
	 }


    


  return 0;
}

void printOptions(int argc, char **argv, struct commandOptions cmdOps) {
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

// Get sockaddr, IPv4
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Get port
void *get_in_port(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_port);
    }
	return &(((struct sockaddr_in6*)sa)->sin6_port);
}

// Return a listening socket
int get_listener_socket(char *PORT)
{
    int listener;     // Listening socket descriptor
    int yes=1;        // For setsockopt() SO_REUSEADDR, below
    int rv;

    struct addrinfo hints, *ai, *p;

    // Get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        if (verbose)
            fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }
    
    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) { 
            continue;
        }
        
        // Lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // If we got here, it means we didn't get bound
    if (p == NULL) {
        return -1;
    }

    freeaddrinfo(ai); // All done with this

    // Listen
    if (listen(listener, 10) == -1) {
        return -1;
    }

    return listener;
}

// Add a new file descriptor to the set
int add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size)
{

	// Does not accept more than 10 connections
	if (*fd_count <= *fd_size) {
		(*pfds)[*fd_count].fd = newfd;
    	(*pfds)[*fd_count].events = POLLIN; // Check ready-to-read

    	(*fd_count)++;
		return 0;
	} else {
        // cannot add fd to fd array since it would exceed it's size. Blocks new connection if num_con limit is reached.
		return -1;
	}
}

// Remove an index from the set
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count)
{
    // Copy the one from the end over this one
    pfds[i] = pfds[*fd_count-1];

    (*fd_count)--;
}

int get_connector_socket(struct commandOptions cmdOps) {
	int sockfd, numbytes;  
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET_ADDRSTRLEN];

	sprintf(PORT, "%d", cmdOps.port);	// convert int to char[]
	
    if (verbose)
        printf("PORT: %s\n", PORT);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;



    if ((rv = getaddrinfo(cmdOps.hostname, PORT, &hints, &servinfo)) != 0) {
        if (verbose)
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
                    if (verbose)
                        perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo);

	return sockfd;
}

// Starts the client
int start_client(struct commandOptions cmdOps) {

	int sockfd;
	int stdin_fd = 0;

	char recv_buf[256];	// buffer for received data from server
	bzero(recv_buf, sizeof recv_buf);	// flush buffer

	char send_buf[256];	// buffer from client data to send to server
	bzero(send_buf, sizeof send_buf);	// flush buffer

	int fd_count = 0;
	int fd_size = 2;
	struct pollfd *pfds = malloc(sizeof *pfds * fd_size);

	sockfd = get_connector_socket(cmdOps);

	if (sockfd == -1) {
		fprintf(stderr, "error getting connection socket\n");
		exit(1);
	}

    // client failed to connect to server (server may not be running)
    if (sockfd == 2) {
        exit(1);
    }

	// add the sock fd to the fd array
	pfds[0].fd = sockfd;
	pfds[0].events = POLLIN;

	// add the stdin fd to the fd array
	pfds[1].fd = stdin_fd;
	pfds[1].events = POLLIN;

	fd_count = 2;

	// main loop
    for(;;) {
        int poll_count = poll(pfds, fd_count, -1);	// TODO: add timeout

        if (poll_count == -1) {
            perror("poll");
            exit(1);
        }

        // loop through existing connections looking for data to read
        for(int i = 0; i < fd_count; i++) {
            // fd is ready to read
            if (pfds[i].revents & POLLIN) {
				
				if (pfds[i].fd == stdin_fd) {
                    // create new buffer and flush it
                    int sender_fd = pfds[i].fd;
                    int bytes_read = read(sender_fd, send_buf, sizeof send_buf);	// read input from terminal
					
                    // send data from client to server
                    for (int i = 0; i < fd_count; i++) {
                        int dest_fd = pfds[i].fd;

                        // don't send to sockfd and stdin sockets
                        if (dest_fd != sender_fd && dest_fd != stdin_fd) {
                            if (send(dest_fd, send_buf, bytes_read, 0) == -1) {
                                fprintf(stderr, "send error: data not sent from client");
                            }
                        }
                    }
                } else {
					// receiving from server
					int receive_fd = pfds[i].fd;
                    int nbytes = recv(pfds[i].fd, recv_buf, sizeof recv_buf, 0);	// read input from terminal

					// connection closed or error
                    if (nbytes <= 0) {
						// connection closed
                        if (nbytes == 0) {
                            fprintf(stderr, "server closed the connection\n");
                        } else {
                            fprintf(stderr, "recv error");
                        }
                        close(pfds[i].fd); // close the connection
                        del_from_pfds(pfds, i, &fd_count); // remove socket fd from fd array
                    } else {
                        // received data from server to all clients
                        for(int j = 0; j < fd_count; j++) {
                            int dest_fd = pfds[j].fd;

                            // don't send to sockfd and stdin sockets
                            if (dest_fd != receive_fd && dest_fd != stdin_fd) {
                                if (send(dest_fd, recv_buf, nbytes, 0) == -1) {
                                    fprintf(stderr, "send error: data not received from server");
                                }
                            }
                        }
                        printf("%s", recv_buf);	// print received data sent by server
                        bzero(recv_buf, sizeof recv_buf); // flush the buffer
                    }
                } // END handle data from client
            } // END got ready-to-read from poll()
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
}

// Starts the server
int start_server(struct commandOptions cmdOps, int num_cons) {
    sprintf(PORT, "%d", cmdOps.port);	// convert int to char[]
	//printf("PORT: %s\n", PORT);

  	int listener;     // Listening socket descriptor

	int newfd;        // Newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // Client address
    socklen_t addrlen;

    char buf[256];    // Buffer for client data
	bzero(buf, sizeof buf); // flush the buffer
    char send_buf[256]; // Buffer for server data
	bzero(send_buf, sizeof send_buf); // flush the buffer

    char remoteIP[INET_ADDRSTRLEN];

    // Start off with room for 10 connections
    // (We'll realloc as necessary)

    int listener_con = 1;

    int fd_count = 0;
    int fd_size = listener_con + num_cons;
    struct pollfd *pfds = malloc(sizeof *pfds * fd_size);

    // Set up and get a listening socket
    listener = get_listener_socket(PORT);

    if (listener == -1) {
        fprintf(stderr, "error getting listening socket\n");
        exit(1);
    }

    // Add the listener to set
    pfds[0].fd = listener;
    pfds[0].events = POLLIN; // Report ready to read on incoming connection

    //fd_count = 1; // For the listener

    pfds[1].fd = 0; // stdin
    pfds[1].events = POLLIN;

    fd_count = 2;

    int initial_fd_count = 2;

	fprintf(stderr, "--- waiting for connection ---\n");

    printf("fdcount %d\n", fd_count);
    printf("num_con %d\n", fd_size);

    // Main loop
    for(;;) {
        int poll_count = poll(pfds, fd_count, -1);	// timeout feature at -1

        if (poll_count == -1) {
            perror("poll");
            exit(1);
        }

        // Run through the existing connections looking for data to read
        for(int i = 0; i < fd_count; i++) {
		
            // Check if someone's ready to read
            if (pfds[i].revents & POLLIN) { // We got one!!
				

                if (pfds[i].fd == listener) {
                    // If listener is ready to read, handle new connection

                    addrlen = sizeof remoteaddr;
					// TODO: show port number of client, can use the client code (save port as global variable maybe)
					// int client_port = get_in_port((struct sockaddr*)&remoteaddr);	// get the port from the struct
                    newfd = accept(listener,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else if (add_to_pfds(&pfds, newfd, &fd_count, &fd_size) == -1) { 
                        // don't add new connection fd if reached num_cons limit
					} else {
                        
						fprintf(stderr, "accepted connection\n");

                        printf("new connection from %s on "
                            "socket %d\n",
                            inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr), remoteIP, INET_ADDRSTRLEN), newfd);
                    }

                    printf("fdcount %d\n", fd_count);
                    printf("num_con %d\n", fd_size);
                } else if (pfds[i].fd == 0) { // stdin
                    // printf("inside stdin send to clients\n");
                    
                    // create new buffer and flush it
                    int sender_fd = pfds[i].fd;
                    int bytes_read = read(sender_fd, send_buf, sizeof send_buf);
					
                    
                    
					// TODO: What if bytes_Read <= 0? See code below

                    // create for loop to go through all the clients and send it to them except for listener and stdin
                    for (int i = 0; i < fd_count; i++) {
                        int dest_fd = pfds[i].fd;

                        // Don't send to sockets that are listener and ourselves
                        if (dest_fd != listener && dest_fd != sender_fd && dest_fd != 0) {
                            if (send(dest_fd, send_buf, bytes_read, 0) == -1) {
                                    perror("send");
                                }
                        }
                    }
					// bzero(send_buf, sizeof send_buf); // flush the buffer

                } else {
                    // If not the listener, we're just a regular client
                    int nbytes = recv(pfds[i].fd, buf, sizeof buf, 0);

                    int sender_fd = pfds[i].fd;

                    if (nbytes <= 0) {
                        // Got error or connection closed by client
                        if (nbytes == 0) {
                            // client closed the connection
                            fprintf(stderr, "--- client closed the connection ---\n");
                        } else {
                            perror("recv");
                        }

                        close(pfds[i].fd); // Bye!

                        del_from_pfds(pfds, i, &fd_count);

                        // TODO: close server here!!!
                        printf("fdcount %d\n", fd_count);
                        printf("num_con %d\n", fd_size);
                        if (cmdOps.option_k == 0 && fd_count <= initial_fd_count) {
                            fprintf(stderr, "--- closing server ---\n");
                            exit(1);
                        } else {
                            fprintf(stderr, "--- waiting for connection ---\n");
                        }

                    } else {
                        // We got some good data from a client
                        // printf("inside good data from client\n");

                        for(int j = 0; j < fd_count; j++) {
                            // Send to everyone!
                            int dest_fd = pfds[j].fd;

                            // Don't send to sockets that are listener, ourselves, and stdin
                            if (dest_fd != listener && dest_fd != sender_fd && dest_fd != 0) {
                                if (send(dest_fd, buf, nbytes, 0) == -1) {
                                    perror("send error:");
                                }
                            }

                        }
                        printf("%s", buf);
                        bzero(buf, sizeof buf); // flush the buffer
                    }
                } // END handle data from client
            } // END got ready-to-read from poll()
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
}