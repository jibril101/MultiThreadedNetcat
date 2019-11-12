#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include "commonProto.h"

void usage(char *progName) {

  fprintf(stderr, "Usage: %s [-k] [-l] [-v] [-r]  [-p source_port] [-w timeout] [hostname] [port[s]]\n", progName);
  fprintf(stderr, "     -k Forces nc to stay listening for another connection after its\n");
  fprintf(stderr, "        current connection is completed. \n");
  fprintf(stderr, "     -p source_port   Specifies the source port nc should use,\n");
  fprintf(stderr, "        subject to privilege restrictions and availability.\n");
  fprintf(stderr, "     -v Print more verbose output to standard error.\n");
  fprintf(stderr, "     -w timeout  If a connection and stdin are idle for more than timeout\n");
  fprintf(stderr, "        seconds, then the connection is silently closed.  The -w flag has\n");
  fprintf(stderr, "        no effect on the -l option.\n");
  fprintf(stderr, "     -r This option can only be used with the -l option. When this option is\n");
  fprintf(stderr, "        specified the server is to work with up to 10 connections simultaneously.\n"); 
  fprintf(stderr, "        any data read from a network connection is resent to all the other connections\n");
}
