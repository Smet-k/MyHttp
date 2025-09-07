#include "request.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define ROOTFOLDER "html"

static const char* http_method_str[] = {
    "GET",
    "POST",
    "UNKNOWN"
};

static int parse_method(char* method, http_method_t* methodptr);
static void parse_path(char* url, char* path);
static int parse_request_line(char buf[], http_request_line_t* rl);
static int parse_headers(char buf[], http_request_t* request);
static int parse_body(char buf[], http_request_t* request);

http_request_t parse_request(char buf[]){
    http_request_t request = {0};
    if(parse_request_line(buf, &request.request_line) < 0)
        perror("failed to parse request line");
    
    if(parse_headers(buf, &request) < 0)
        perror("failed to parse request headers");

    if(parse_body(buf, &request) < 0)
        perror("failed to parse request body");
    
    return request;
}

static int parse_headers(char buf[], http_request_t* request){
    char* headers_end = strstr(buf, "\r\n\r\n");
    *headers_end = '\0'; // making strtok not destroy head-body delimiter

    char* save_ptr;
    char* line = strtok_r(buf,"\r\n", &save_ptr);
    if(!line) return -1;

    int i = 0;
    while(line) {
        if (*line == '\0') {
            break;
        }

        char* value;
        char* key = strtok_r(line, ":", &value);

        if(!key || !value) return -1;

        while(*value == ' ') value++;

        http_header_t header;        
        strncpy(header.name,key, sizeof(header.name) - 1);
        strncpy(header.value, value, sizeof(header.value) - 1); 

        header.name[sizeof(header.name) - 1] = '\0';
        header.value[sizeof(header.value) - 1] = '\0';

        if(strcasestr(header.name, "Content-Length"))
            request->content_length = atoi(value);

        request->headers[i++] = header;

        line = strtok_r(NULL, "\r\n", &save_ptr);
    }
    *headers_end = '\r';
    strcpy(buf, save_ptr);
    return 0;
}

static int parse_request_line(char buf[], http_request_line_t* rl){
    char method[METHOD_SIZE];
    char url[URL_SIZE];
    char format_string[32];
    char* save_ptr;
    snprintf(format_string, sizeof(format_string),
    "%%%ds %%%ds %%%ds", METHOD_SIZE - 1, URL_SIZE - 1, VERSION_SIZE - 1);
    
    // char* line_end = strstr(buf, "\r\n");
    char* line = strtok_r(buf, "\r\n", &save_ptr);
    
    if (!line) {
        rl->method = HTTP_UNKNOWN;
        return -1;
    }

    // *line_end = '\0';

    int n = sscanf(line, format_string, method, url, rl->version);
    if (n != 3) {
        rl->method = HTTP_UNKNOWN;
        return -1;
    }
    strcpy(buf, save_ptr+1); // +1 removes the \n

    // *line_end = '\r';
    
    parse_path(url, rl->path);

    if(parse_method(method, &rl->method) < 0)
        return -1;

    return 0;
}

static int parse_method(char* method, http_method_t* methodptr){
    if(!method){
        *methodptr = HTTP_UNKNOWN;
        return -1;
    }

    for(int i = 0;i < HTTP_UNKNOWN; i++){
        if(strcmp(method, http_method_str[i]) == 0){
            *methodptr = (http_method_t)i;
            return 0;
        }
    }
    *methodptr = (http_method_t)HTTP_UNKNOWN;
    return 0;
}

static void parse_path(char* url, char* path){

    // skipping query
    char* query = strstr(url, "?");
    if (query)
        *query = '\0';
    
    snprintf(path, PATH_SIZE, "%s%s", ROOTFOLDER, url);
    
    if (path[strlen(path) - 1] == '/')
        strncat(path, "index.html", PATH_SIZE - strlen(path) - 1);

}

static int parse_body(char buf[], http_request_t* request){
    if (!buf || !request) return -1;
    
    if (request->content_length > 0){
        char* body = strstr(buf, "\r\n\r\n");
        if (body) {
            body += 4;
            request->body = malloc(request->content_length + 1);
            if (!request->body) return -1;

            if (request->body) {
                strncpy(request->body, body, request->content_length);
                request->body[request->content_length] = '\0';
            }
        }
    }
    return 0;
}
