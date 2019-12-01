#include "commonProto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


// This is the start of the code to parse the command line options. It should be
// fairly complete but hasn't been extensively tested.

int parseOptions(int argc, char * argv[], struct commandOptions * co) {

  // First set the command options structure to known values

  co->option_k = 0;
  co->option_l = 0;
  co->option_v = 0;
  co->option_r = 0;
  co->option_p = 0;
  co->source_port = 0;
  co->timeout = 0;
  co->hostname = NULL;
  co->port = 0;
  
  int i;
  int lastTwo = 0;
  
  // iterate over each argument and classify it and
  // store information in the co structure;
  
  for (i = 1; i < argc; i++) {
    // This next line is for illustraction purposes only and needs to be removed
    // once things are working 
    // fprintf(stderr, "Arg %d is: %s\n", i, argv[i]);

    // Check for the various options
    if ((strcmp(argv[i], K_OPTION) == 0) && (!lastTwo)) {
      co->option_k = 1;
    } else if ((strcmp(argv[i], L_OPTION) == 0) && (!lastTwo)) {
      co->option_l = 1;
    } else if ((strcmp(argv[i], V_OPTION) ==0 ) && (!lastTwo)) {
      co->option_v = 1;
    } else if ((strcmp(argv[i], R_OPTION) == 0 ) && (!lastTwo)){
      co->option_r = 1;
    } else if ((strcmp(argv[i], P_OPTION) == 0) && (!lastTwo)) {
      // got a port match, check next argument for port number
      i++;
      if (i >= argc) {
	// not enough arguments
	return PARSE_ERROR;
      } else { // extract port number
	// See man page for strtoul() as to why
	// we check for errors by examining errno
	errno = 0;
	co->source_port = strtoul(argv[i], NULL, 10);
	if (errno != 0) {
	  return PARSE_ERROR;
	} else if (co->source_port < 1024 || co->source_port > 65535) {
    return PARSE_PORT_OUT_OF_RANGE; // port number must not be out of range or reserved
  } else {
	  co->option_p = 1;
	}
      }
    } else if ((strcmp(argv[i], W_OPTION) == 0) && (!lastTwo)) {
      // got a W match, check next argument for timeout value
      i++;
      if (i >= argc) {
	// not enough arguments
	return PARSE_ERROR;
      } else { // extract timeout value
	// See man page for strtoul() as to why
	// we check for errors by examining errno, see err
	errno = 0;
	co->timeout = strtoul(argv[i], NULL, 10);
	if (errno != 0) {
	  return PARSE_ERROR;
	}
      }
      // Things are tricker now as this must be either the hostname or port number
      // and if there are more parameters on the line then this is a bug
    } else if (lastTwo == 1) { // hostname
      co->hostname = argv[i];
      lastTwo++;
    } else if (lastTwo == 0) { // port
      errno = 0;
      co->port = strtoul(argv[i], NULL, 10);
      if (errno != 0) {
	return PARSE_ERROR;
      } else if (co->port < 1024 || co->port > 65535) {
    return PARSE_PORT_OUT_OF_RANGE; // port number must not be out of range or reserved
  }
      lastTwo++;
    
    
    } else { // TOO many parameters
      return PARSE_TOOMANY_ARGS;
    }
  }

  // At this point all the command line arguments have been parsed but they
  // haven't been checked for validity. It might make sense to check things
  // like port numbers to verify that they are in the valid range before
  // returning success and that the options don't contradict each other
  // It is up to you to decide how you want to proceed.
  return PARSE_OK;
}
