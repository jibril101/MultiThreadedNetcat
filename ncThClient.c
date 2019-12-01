#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
//#include "Thread.h"
#include "commonProto.h"


//#define PORT "9000" // the port client will be connecting to 

#define BUFSIZE 4096

void *handle_std_in(void* arg);
// get sockaddr, IPv4 or IPv6:
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
    /*int *std_in = malloc(sizeof(int));
    *std_in = 0;
    void* std_in_thread = createThread(handle_std_in, std_in);
    runThread(std_in_thread, NULL);*/
	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
	printf("client: connecting to %s\n", s);
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
	 while(1) {
        int ret = 0;
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

