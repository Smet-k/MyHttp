#ifndef SERVER_H
#define SERVER_H
#include <netinet/in.h>
#include "config.h"

void run_server(Config cfg);
// void startup(int* server_fd, struct sockaddr_in* server_addr, Config* cfg);
// void accept_requests(int server_fd, Config cfg);

#endif