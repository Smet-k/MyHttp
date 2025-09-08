#include <stdio.h>
#include <unistd.h>
#include "server.h"

int main() {
    Config cfg;

    if(load_config("config.cfg", &cfg) < 0)
        perror("Couldn't parse the config");
    

    run_server(cfg);

    return 0;
}





