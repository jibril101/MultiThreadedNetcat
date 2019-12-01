#ifndef _NCTHCLIENT_H
#define _NCTHCLIENT_H


void *handle_std_in(void* arg);
int client(int p, unsigned int src_port, int timeout, int log_mode, unsigned int port, char * hostname);
#endif