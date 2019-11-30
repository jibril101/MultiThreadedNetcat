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


// function prototypes
void printOptions(int argc, char **argv, struct commandOptions cmdOps);
void *get_in_addr(struct sockaddr *sa);
int get_listener_socket(char *PORT);
int add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size);
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count);
int start_server(struct commandOptions cmdOps);
void *get_in_port(struct sockaddr *sa);
int start_client(struct commandOptions cmdOps);



int main(int argc, char **argv) {

  	struct commandOptions cmdOps;
	parseOptions(argc, argv, &cmdOps);
 	printOptions(argc, argv, cmdOps);

	 if (cmdOps.option_l) {
		 // TODO: need to pass the last argument as the port (error as it now)
		return start_server(cmdOps);
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

// Get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

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
    // If we don't have room, add more space in the pfds array
    // if (*fd_count == *fd_size) {
    //     *fd_size *= 2; // Double it

    //     *pfds = realloc(*pfds, sizeof(**pfds) * (*fd_size));
    // }

	// TODO: Do not accept more than 10 connections
	if (*fd_count <= *fd_size) {
		(*pfds)[*fd_count].fd = newfd;
    	(*pfds)[*fd_count].events = POLLIN; // Check ready-to-read

    	(*fd_count)++;
		return 0;
	} else {
		fprintf(stderr, "server is full, cannot accept new connection\n");
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
	printf("PORT: %s\n", PORT);

    // if (argc != 2) {
    //     fprintf(stderr,"usage: client hostname\n");
    //     exit(1);
    // }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(cmdOps.hostname, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

	// Connection established to server

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
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

    freeaddrinfo(servinfo); // all done with this structure

	return sockfd;
}

int start_client(struct commandOptions cmdOps) {
	// Don't bind and listen
	// TODO: Polling client: Same as server code but you have 2 fd only, you dont bind or listen, you just connect to server
	// then (second) fd is stdin
	// TODO: Set listeners to pollout?
	// TODO: limit connections to only 10 for polling

	int sockfd;
	char buf[256];	// buffer for client data
	bzero(buf, sizeof buf);	// flush buffer

	char send_buf[256];	// buffer for client data
	bzero(send_buf, sizeof send_buf);	// flush buffer

	int fd_count = 0;
	int fd_size = 2;
	struct pollfd *pfds = malloc(sizeof *pfds * fd_size);

	sockfd = get_connector_socket(cmdOps);

	if (sockfd == -1) {
		fprintf(stderr, "error getting connection socket\n");
		exit(1);
	}

	// add the sockfd to the fd array
	pfds[0].fd = sockfd;
	pfds[0].events = POLLIN;

	// add the stdin fd to the fd array
	pfds[1].fd = 0;	// 0 is stdin
	pfds[1].events = POLLIN;

	fd_count = 2;

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
				

				if (pfds[i].fd == 0) { // stdin
                    // printf("inside stdin send to clients\n");
                    
                    // create new buffer and flush it
                    int sender_fd = pfds[i].fd;
                    int bytes_read = read(sender_fd, send_buf, sizeof send_buf);
					
                    
                    
					// TODO: What if bytes_Read <= 0? See code below

                    // create for loop to go through all the clients and send it to them except for listener and stdin
                    for (int i = 0; i < fd_count; i++) {
                        int dest_fd = pfds[i].fd;

                        // Don't send to sockets that are listener and ourselves
                        if (dest_fd != sender_fd && dest_fd != 0) {
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
                            // Connection closed
                            printf("pollserver: socket %d hung up\n", sender_fd);
                        } else {
                            perror("recv");
                        }

                        close(pfds[i].fd); // Bye!

                        del_from_pfds(pfds, i, &fd_count);

						if (cmdOps.option_l) {
							return 0;
						}

                    } else {
                        // We got some good data from a client
                        // printf("inside good data from client\n");

                        for(int j = 0; j < fd_count; j++) {
                            // Send to everyone!
                            int dest_fd = pfds[j].fd;

                            // Don't send to sockets that are listener, ourselves, and stdin
                            if (dest_fd != sender_fd && dest_fd != 0) {
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

    // if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
    //     perror("recv");
    //     exit(1);
    // }

    // buf[numbytes] = '\0';

    // printf("client: received '%s'\n",buf);

    // close(sockfd);

    // return 0;

}

int start_server(struct commandOptions cmdOps) {

	sprintf(PORT, "%d", cmdOps.port);	// convert int to char[]
	printf("PORT: %s\n", PORT);

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
    int fd_count = 0;
    int fd_size = 3; // connections is fd_size - 1
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

	fprintf(stderr, "waiting for connection\n");
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
                    } else if (add_to_pfds(&pfds, newfd, &fd_count, &fd_size) == -1) { // close fd if already have 10 connections
						perror("cannot add to fd array\n");
					} else {
                        
						fprintf(stderr, "accepted connection\n");

                        printf("new connection from %s on "
                            "socket %d\n",
                            inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr), remoteIP, INET_ADDRSTRLEN), newfd);
                    }
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
                            // Connection closed
                            printf("pollserver: socket %d hung up\n", sender_fd);
                        } else {
                            perror("recv");
                        }

                        close(pfds[i].fd); // Bye!

                        del_from_pfds(pfds, i, &fd_count);

						if (cmdOps.option_l) {
							return 0;
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