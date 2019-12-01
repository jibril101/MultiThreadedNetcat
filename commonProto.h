#ifndef COMMONPROTO
#define COMMONPROTO
#define RESERVED_PORT 1023


struct commandOptions {
  int option_k;   // indicates if -k was provided.
  int option_l;   // indicates if -l provided
  int option_v;   // indicates if -v provided
  int option_r;   // indicates if -r provided
  int option_p;   // indicates if -p provided
  unsigned int source_port;  // port number for -p
  unsigned int timeout;      // timeout value to use
  char * hostname;           // The hostname or IP address to connect to
                             // when in client mode
  unsigned int port;               // Port to connect to when in client mode
};

//Add the function prototypes to this file for any functions 
//shared across the two implementations. Also make sure to 
//include this file in the source file that defines any of 
//functions specified here. (This will ensure that the 
//prototypes match the actual code.) 

#define PARSE_OK 0
#define PARSE_ERROR -1
#define PARSE_TOOMANY_ARGS -2;
#define PARSE_PORT_OUT_OF_RANGE -3;

#define K_OPTION "-k"
#define L_OPTION "-l"
#define V_OPTION "-v"
#define R_OPTION "-r"
#define P_OPTION "-p"
#define W_OPTION "-w"

int parseOptions(int argc, char * argv[], struct commandOptions * co);
void usage(char *);


#endif
