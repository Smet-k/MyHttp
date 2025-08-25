#include <stdio.h>
#include <unistd.h>
#include "server.h"

int main() {
    Config cfg;
    struct sockaddr_in server_addr;
    int server_sock;

    if(load_config("config.cfg", &cfg) < 0){
        perror("Couldn't parse the config");
    }

    startup(&server_sock, &server_addr, &cfg);

    accept_requests(server_sock, cfg);

    close(server_sock);
    return 0;
}





