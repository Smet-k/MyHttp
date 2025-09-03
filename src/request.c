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
static char* parse_path(char* url);
static int parse_request_line(char buf[], http_request_line_t* rl);
static int parse_headers(char** bufptr, http_request_t* request);
static int parse_body(char buf[], http_request_t* request);
static const char* get_mime_type(const char* filename);

http_request_t parse_request(char buf[]){
    http_request_t request = {0};

    if(parse_request_line(buf, &request.request_line) < 0)
        perror("failed to parse request line");

    // parse_headers(); TBD
    // Review error handling

    if(parse_body(buf, &request) < 0){
        perror("failed to parse body");
    }

    return request;
}

static int parse_request_line(char buf[], http_request_line_t* rl){
    char method[METHOD_SIZE];
    char url[URL_SIZE];

    char* line_end = strstr(buf, "\r\n");
    
    if (!line_end) {
        rl->method = HTTP_UNKNOWN;
        return -1;
    }

    *line_end = '\0';

    int n = sscanf(buf, "%7s %255s %15s", method, url, rl->version);
    if (n != 3) {
        rl->method = HTTP_UNKNOWN;
        return -1;
    }
    *line_end = '\r';

    strncpy(rl->path,parse_path(url), PATH_SIZE);

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

static char* parse_path(char* url){
    char* path;

    // skipping query
    char* query = strstr(url, "?");
    if (query)
        *query = '\0';
    
    sprintf(path, "%s%s", ROOTFOLDER, url);
    
    if (path[strlen(path) - 1] == '/')
        strcat(path, "index.html");
    
    return path;
}

static int parse_body(char buf[], http_request_t* request){
    if (!buf || !request) return -1;
    
    const char* cl_header = strcasestr(buf, "Content-Length:");
    if (cl_header) {
        cl_header += strlen("Content-Length:");

        while (*cl_header && isspace((unsigned char)*cl_header))
            cl_header++;

        request->content_length = atoi(cl_header);
    } else{
        request->content_length = 0;
    }

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
