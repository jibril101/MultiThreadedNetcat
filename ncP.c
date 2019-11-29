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
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>

// function prototypes
void printOptions(int argc, char **argv, struct commandOptions cmdOps);
void *get_in_addr(struct sockaddr *sa);
int get_listener_socket(char *PORT);
void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size);
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count);
void start_server(struct commandOptions cmdOps);


// TODO: Polling client: Same as server code but you have 2 fd only, you dont bind or listen, you just connect to server
// then (second) fd is stdin
int main(int argc, char **argv) {

  	struct commandOptions cmdOps;
	parseOptions(argc, argv, &cmdOps);
 	// printOptions(argc, argv, cmdOps);

	 if (cmdOps.option_l && cmdOps.option_p) {
		 
		start_server(cmdOps);
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
void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size)
{
    // If we don't have room, add more space in the pfds array
    if (*fd_count == *fd_size) {
        *fd_size *= 2; // Double it

        *pfds = realloc(*pfds, sizeof(**pfds) * (*fd_size));
    }

    (*pfds)[*fd_count].fd = newfd;
    (*pfds)[*fd_count].events = POLLIN; // Check ready-to-read

    (*fd_count)++;
}

// Remove an index from the set
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count)
{
    // Copy the one from the end over this one
    pfds[i] = pfds[*fd_count-1];

    (*fd_count)--;
}

void start_server(struct commandOptions cmdOps) {
	char PORT[12];	// port we're listening on
	sprintf(PORT, "%d", cmdOps.source_port);	// convert int to char[]
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
    int fd_size = 10;
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

    fd_count = 1; // For the listener

    pfds[1].fd = 0; // stdin
    pfds[1].events = POLLIN;

    fd_count = 2;

    // Main loop
    for(;;) {
        int poll_count = poll(pfds, fd_count, -1);

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
                    newfd = accept(listener,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        add_to_pfds(&pfds, newfd, &fd_count, &fd_size);

                        printf("pollserver: new connection from %s on "
                            "socket %d\n",
                            inet_ntop(remoteaddr.ss_family,
                                get_in_addr((struct sockaddr*)&remoteaddr),
                                remoteIP, INET_ADDRSTRLEN),
                            newfd);
                    }
                } else if (pfds[i].fd == 0) { // stdin
                    // printf("inside stdin send to clients\n");
                    
                    // create new buffer and flush it
                    int sender_fd = pfds[i].fd;
                    int bytes_read = read(sender_fd, send_buf, sizeof send_buf);
					
                    
                    


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