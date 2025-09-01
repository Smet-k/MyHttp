#include "server.h"

#include <ctype.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tpool.h"
#define MAX_CLIENTS 1024
#define BUFFER_SIZE 1024
#define METHOD_SIZE 16
#define URL_SIZE 255
#define SERVERSTRING "Server: myHttp\r\n"
#define ROOTFOLDER "html"


static void process_requests(int client);
static void parse_arguments(char buf[], char* method, char* path, int *content_length);

static void respond(int client, int code, const char* reason);
// static void respond(int client, int code, const char* reason, const char* content_type, const char* body);
static void respond_file(int client, const char* content_type, FILE* resource, const char* body, int content_length);
static const char* get_mime_type(const char* filename);

static void handle_get(int client, char* path);
static void handle_post(int client, char* path, int content_length, char* buf);

void startup(int* server_fd, struct sockaddr_in* server_addr, Config* cfg) {
    if ((*server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Server start failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(*server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr->sin_family = AF_INET;
    server_addr->sin_addr.s_addr = INADDR_ANY;

    while (1) {
        server_addr->sin_port = htons(cfg->port);

        if (bind(*server_fd,
                 (struct sockaddr*)server_addr,
                 sizeof(*server_addr)) == 0) {
            break;
        }

        if (errno == EADDRINUSE)
            cfg->port++;
        else {
            perror("bind failed");
            exit(EXIT_FAILURE);
        }

        if (cfg->port > 65535) {
            fprintf(stderr, "No free ports available.\n");
            exit(EXIT_FAILURE);
        }
    }

    if (listen(*server_fd, 10) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Listening at localhost:%u\n", cfg->port);
}

void accept_requests(int server_fd, Config cfg) {
    ThreadPool* tp = threadpool_create(cfg.threads);
    struct sockaddr_in client_addr;
    int client_addr_length = sizeof(client_addr);

    struct pollfd fds[MAX_CLIENTS];
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    nfds_t nfds = 1;

    while (1) {
        int ready = poll(fds, nfds, -1);
        if (ready < 0) {
            perror("poll failed");
            continue;
        }

        for (int i = 0; i < nfds; i++) {
            if (fds[i].revents && POLLIN) {
                if (fds[i].fd == server_fd) {
                    int client_sock = accept(server_fd,
                                             (struct sockaddr*)&client_addr,
                                             &client_addr_length);

                    if (client_sock >= 0) {
                        fds[nfds].fd = client_sock;
                        fds[nfds].events = POLLIN;
                        nfds++;
                    }
                } else {
                    int client_sock = fds[i].fd;

                    Task task = {
                        .function = &process_requests,
                        .arg = client_sock,
                    };

                    thread_queue_push(&tp->queue, task);

                    fds[i] = fds[nfds - 1];
                    nfds--;
                }
            }
        }
    }
}

static void process_requests(int client) {
    char buf[BUFFER_SIZE];
    char method[METHOD_SIZE];
    char path[URL_SIZE];

    int content_length = -1;

    int numbytes = recv(client, buf, sizeof(buf), 0);
    if (numbytes > 0) {
        buf[numbytes] = '\0';
        parse_arguments(buf, method, path, &content_length);

        if(content_length < 0 && strcmp(method, "POST") == 0){
            respond(client, 400, "Bad Request");
            return;
        }

        if (strcmp(method, "GET") && strcmp(method, "POST")) {
            respond(client, 501, "Method Not Implemented");
            return;
        }

        if (strcmp(method, "GET") == 0) {
            handle_get(client, path);
        } else if (strcmp(method, "POST") == 0) {
            handle_post(client, path, content_length, buf);
        }
    }
    close(client);
}

static void parse_arguments(char buf[], char* method, char* path,int* content_length) {
    char url[URL_SIZE];
    int i = 0, j = 0;

    while (!isspace((int)buf[j]) && i < METHOD_SIZE - 1) {
        method[i] = buf[j];
        i++;
        j++;
    }
    method[i] = '\0';
    while (isspace((int)buf[j]) && j < BUFFER_SIZE - 1)
        j++;

    i = 0;
    while (!isspace((int)buf[j]) && j < BUFFER_SIZE && i < URL_SIZE - 1) {
        url[i] = buf[j];
        i++;
        j++;
    }
    url[i] = '\0';

    // skipping query
    char* query = strstr(url, "?");
    if(query)
        *query = '\0';
    
    sprintf(path, "%s%s", ROOTFOLDER, url);

    if (path[strlen(path) - 1] == '/')
        strcat(path, "index.html");

    const char* cl_header = strcasestr(buf, "Content-Length:");
    if (cl_header) {
        cl_header += strlen("Content-Length:");

        while (*cl_header && isspace((unsigned char)*cl_header))
            cl_header++;

        *content_length = atoi(cl_header);
    }
}

static void respond(int client, int code, const char* reason) {
    char buf[BUFFER_SIZE];

    sprintf(buf, "HTTP/1.0 %d %s", code, reason);
    send(client, buf, strlen(buf), 0);

    strcpy(buf, SERVERSTRING);
    send(client, buf, strlen(buf), 0);

    strcpy(buf, "Content-Length:0\r\n");
    send(client, buf, strlen(buf), 0);

    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);

}

static void respond_file(int client, const char* content_type, FILE* resource, const char* body, int content_length) {
    char buf[BUFFER_SIZE];
    size_t bytes_read;

    sprintf(buf, "HTTP/1.0 200 OK");
    send(client, buf, strlen(buf), 0);

    strcpy(buf, SERVERSTRING);
    send(client, buf, strlen(buf), 0);

    if(body){
        sprintf(buf, "Content-Length: %d", content_length);
        send(client, buf, strlen(buf), 0);
    }

    sprintf(buf, "Content-Type: %s\r\n", content_type);
    send(client, buf, strlen(buf), 0);

    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);

    while ((bytes_read = fread(buf, 1, sizeof(buf) - 1, resource)) > 0) {
        buf[bytes_read] = '\0';
        if (body) {
            char* body_end = strstr(buf, "</body>");
            if (body_end) {
                *body_end = '\0';
                send(client, buf, strlen(buf), 0);
                send(client, body, strlen(body), 0);
                send(client, "</body>", 7, 0);
            } 
        }
        else{
            send(client, buf, bytes_read, 0);
        }
    }
}

static void handle_get(int client, char* path) {
    FILE* file = NULL;
    
    file = fopen(path, "r");

    if (file == NULL)
        respond(client, 404, "Not Found");
    else{
        respond_file(client, get_mime_type(path), file, NULL, 0);
        fclose(file);
    }
}

static void handle_post(int client, char* path, int content_length, char* buf) {
    FILE* file = NULL;

    char* body = strstr(buf, "\r\n\r\n");
    if (body) body += 4;
    
    file = fopen(path, "r");

    if (file == NULL)
        respond(client, 404, "Not Found");
    else {
        respond_file(client, get_mime_type(path), file, body, content_length);
        fclose(file);
    }
}

static const char* get_mime_type(const char* filename) {
    const char* ext = strchr(filename, '.');

    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";

    return "application/octet-stream";
}
