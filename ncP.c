#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include "commonProto.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <poll.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>




// function declarations
void printOptions(struct commandOptions cmdOps, int argc, char **argv);
int startClient(struct commandOptions cmdOps, int argc, char **argv);

//int poll(struct pollfd fds[], nfds_t nfds, int timeout);

// struct pollfd {
//   int fd; // socket descriptor
//   short events; // bitmap of events interested
//   short revents;  // when poll() returns, bitmap of events that occurred
// };


// Get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Return a listening socket
int get_listener_socket(char PORT[])
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












typedef enum {false, true} bool;

// struct to keep track of client's file descriptors
struct client_fds {
  bool in_use;
  int fd;
};





// Start hosting the server connection
void start_server(int num_connections, char PORT[]) {
	printf("Starting server\n");

  int listening_fd; // listening socket
  int accepted_fd;  // newly accepted socket
	int stdin_fd = 0; // stdin socket

  struct sockaddr_storage remoteaddr; // client address
  socklen_t addrlen;

  char buffer[256]; // buffer for client data
  char remoteIP[INET_ADDRSTRLEN];

  int fd_count = 0;
  int fd_size = num_connections; // number of connections to start accepting
  struct pollfd *poll_fds = malloc(sizeof *poll_fds * fd_size); // pollfd keep track of each fd

  listening_fd = get_listener_socket(PORT); // set up listening socket

  if (listening_fd == -1) {
	fprintf(stderr, "error getting listening socket\n");
	exit(1);
  }

  poll_fds[0].fd = listening_fd;  // add listener to array of sockets to monitor (index 1)
  poll_fds[0].events = POLLIN; // notify ready to read incoming connection
  fd_count = 1; // for listener

// add the stdin fd
	poll_fds[1].fd = stdin_fd; // stdin
  poll_fds[1].events = POLLIN; // alert when I can send data to this socket
  fd_count = 2; // for stdin

//   poll_fds[1].fd = 0; // stdin
//   poll_fds[1].events = POLLOUT; // alert when I can send data to this socket
//   fd_count = 2; // for stdin

  fprintf(stderr, "waiting for connection\n");


   // Main loop
	for(;;) {
		int poll_count = poll(poll_fds, fd_count, -1);

		// TOOD:
		// add another fd = 0 (value 0) inside poll_fds and treat as regular socket to read in input
		// fd = 1 is stdout output

		if (poll_count == -1) {
			perror("poll");
			exit(1);
		}

		// Run through the existing connections looking for data to read
		for(int i = 0; i < fd_count; i++) {


			// Check if someone's ready to read
			if (poll_fds[i].revents & POLLIN) { // We got one!!

				if (poll_fds[i].fd == listening_fd) {
					// If listener is ready to read, handle new connection

					fprintf(stderr, "accepted connection\n");

					addrlen = sizeof remoteaddr;
					accepted_fd = accept(listening_fd, (struct sockaddr *)&remoteaddr, &addrlen);

					if (accepted_fd == -1) {
						perror("could not accept connection\n");
					} else {
						add_to_pfds(&poll_fds, accepted_fd, &fd_count, &fd_size);
						printf("FD count: %d\n", fd_count);
						printf("Poll count after fd count: %d\n", poll_count);

						printf("pollserver: new connection from %s on " "socket %d\n", inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr), remoteIP, INET_ADDRSTRLEN), accepted_fd);

						// 	// read input from stdin and print it out
						// int read_input_happend = poll_fds[1].revents & POLLIN;
						// printf("Revents: %d\n", poll_fds[1].revents);
						// printf("Pollin %d\n", POLLIN);
						// // printf("read %d\n", read_input_happend);
						// // if (read_input_happend) {
						// if (poll_fds[1].fd == stdin_fd) {
						// // readin per character
						// 	fprintf(stderr, "reading input\n");
						// 	int ret_read = read(poll_fds[1].fd, buffer, 256);

						// 	printf("ret_read:\t%zd\nerrno:\t%d\nstrerror:\t%s\nbuff:\t%s\n", ret_read, errno, strerror(errno), buffer);
					

				// }
			//}


							// TODO: need to start accepting stdin and then send message to all clients
							// loop through each pollfd and send message to each fd client
							// use a queue for any connections greater than 10, greater than polling supports


					} 
						// read from the teriminal fgets

						// send it to all the connected clients in the fd using a for loop
					} else if (poll_fds[i].fd == stdin_fd) {
						int ret_read = read(poll_fds[i].fd, buffer, 256);
					}

				} else {
					// If not the listener, we're just a regular client
					int nbytes = recv(poll_fds[i].fd, buffer, sizeof buffer, 0);

					int sender_fd = poll_fds[i].fd;

					if (nbytes <= 0) {
						// Got error or connection closed by client
						if (nbytes == 0) {
							// Connection closed
							printf("pollserver: socket %d hung up\n", sender_fd);
						} else {
							perror("recv");
						}

						close(poll_fds[i].fd); // Bye!

						del_from_pfds(poll_fds, i, &fd_count);

					} else {
						// We got some good data from a client

						for(int j = 0; j < fd_count; j++) {
							// Send to everyone!
							int dest_fd = poll_fds[j].fd;

							// Except the listener and ourselves
							if (dest_fd != listening_fd && dest_fd != sender_fd) {
								if (send(dest_fd, buffer, nbytes, 0) == -1) {
									perror("send");
								}
							}
							printf("%s", buffer);
						}
					}
				} // END handle data from client
			} // END got ready-to-read from poll()
		} // END looping through file descriptors
	} // END for(;;)--and you thought it would never end!






int main(int argc, char **argv) {
	printf("Inside main function\n");
	struct commandOptions cmdOps;
	parseOptions(argc, argv, &cmdOps);

	//printOptions(cmdOps, argc, argv);

	char host_port[12];
	sprintf(host_port, "%d", cmdOps.source_port);    // convert int to string
	printf("Host port: %s\n", host_port);

// Need to create own poll client
// TODO: want a poll_fd for stdin
// Server accepts one connection only until it is closed, closes server
	printf("option_l %d\n", cmdOps.option_l);
	printf("option_p %d\n", cmdOps.option_p);

  if (cmdOps.option_l == 1 && cmdOps.option_r == 1 && cmdOps.option_p == 1) {
	  printf("Inside server connection\n");

	int num_connections = 1;
	start_server(num_connections, host_port);
  }


// CLIENT MODE
// 2 fd, monitor stdin, and socket to server
// if server send you msg then print out
// if client send msg then allow stdin to send to server

// client mode
if (argc == 3 && cmdOps.option_l == 0) {
	int retValue = startClient(cmdOps, argc, argv);
}

  // SERVER MODE
  if (cmdOps.option_l && cmdOps.option_p) {
	  fprintf(stderr, "error: cannot use option -p with -l");
  }

  // TODO: act as server, accepts a connection


  // Server accepts one connection only until it is closed, then coninue to listen for another connection
  if (cmdOps.option_l && cmdOps.option_k) {

  }

  // Server accepts multiple connections at the same time
  if (cmdOps.option_l && cmdOps.option_r) {

  }

  // After first connection accepted, once connection closes, server listens forever
  if (cmdOps.option_l && cmdOps.option_k && cmdOps.option_r) {

  }


  // TODO: handle port number

  // TODO: need to scan poll_fds array for which elements occurred the event
  // stop scanning array after you find the number of elements

  // TODO: to delete elements from poll_fds, set the fd field to -1 and poll will ignore it



  return 0;
}

int startClient(struct commandOptions cmdOps, int argc, char **argv) {
	printf("Inside client connection\n");

	char server_port[6];
	sprintf(server_port, "%d", cmdOps.port);
	int MAXDATASIZE = 100;

	int sockfd, numbytes;
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET_ADDRSTRLEN];

	if (argc != 3) {
		fprintf(stderr,"usage: client hostname\n");
		exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET; // force IPv4
	hints.ai_socktype = SOCK_STREAM;


	if ((rv = getaddrinfo(argv[1], server_port, &hints, &servinfo)) != 0) {
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

	printf("Enter message: ");

	freeaddrinfo(servinfo); // all done with this structure
printf("Enter message: ");
	if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
		perror("recv");
		exit(1);
	}
printf("Enter message: ");
	buf[numbytes] = '\0';

	printf("client: received '%s'\n",buf);

	close(sockfd);
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


