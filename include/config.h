#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    int port;
    int threads;
} Config;

int load_config(const char* filename, Config* config);

#endif