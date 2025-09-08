#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAXLINE 128

static char* trim(char* str) {
    if(!str){
        fprintf(stderr, "Trimming the NULL string\n");
        return NULL;
    }

    while (*str == ' ' || *str == '\t') str++;

    char* end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }

    return str;
}

int load_config(const char* filename, Config* config) {
    if(!filename || !config) return -1;
    
    FILE* file = fopen(filename, "r");
    if (!file) return -1;

    char line[MAXLINE];

    while (fgets(line, sizeof(line), file) != NULL) {
        char* trimmed = trim(line);

        if (!trimmed || *trimmed == '\0') {
            continue;
        }

        const char* key = strtok(trimmed, "=");
        const char* value = strtok(NULL, "=");

        if (key && value) {
            if (strcmp(key, "port") == 0) {
                config->port = atoi(value);
            } else if (strcmp(key, "threads") == 0) {
                config->threads = atoi(value);
            }
        }
    }
    
    fclose(file);
    return 0;
}