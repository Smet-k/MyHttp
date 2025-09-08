#include "server.h"

#include <ctype.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "request.h"
#include "tpool.h"
#define MAX_CLIENTS 50000
#define BUFFER_SIZE 1024
#define SERVERSTRING "Server: myHttp\r\n"
#define ROOTFOLDER "html"

static void startup(int* server_fd, struct sockaddr_in* server_addr, Config* cfg);
static void accept_requests(const int server_fd, const Config cfg);

static void process_requests(const int client);

static void respond(const int client, const int code, const char* reason);
static void respond_file(const int client, const http_request_t request, FILE* resource);
static const char* get_mime_type(const char* filename);
static void handle_request(const int client, const http_request_t request);

void run_server(Config cfg) {
    struct sockaddr_in server_addr;
    int server_sock;

    startup(&server_sock, &server_addr, &cfg);

    accept_requests(server_sock, cfg);

    close(server_sock);
}

static void startup(int* server_fd, struct sockaddr_in* server_addr, Config* cfg) {
    if (!server_fd || !server_addr || !cfg) {
        perror("Invalid argument to startup");
        exit(EXIT_FAILURE);
    }

    if ((*server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Server start failed");
        exit(EXIT_FAILURE);
    }

    const int opt = 1;
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

static void accept_requests(const int server_fd, const Config cfg) {
    ThreadPool* tp = threadpool_create(cfg.threads);
    struct sockaddr_in client_addr;
    socklen_t client_addr_length = sizeof(client_addr);

    struct pollfd fds[MAX_CLIENTS];
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    nfds_t nfds = 1;

    while (1) {
        const int ready = poll(fds, nfds, -1);
        if (ready < 0) {
            perror("poll failed");
            continue;
        }

        for (int i = 0; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == server_fd) {
                    const int client_sock = accept(server_fd,
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

static void process_requests(const int client) {
    char buf[BUFFER_SIZE];

    const int numbytes = recv(client, buf, sizeof(buf) - 1, 0);
    if (numbytes <= 0) {
        close(client);
        return;
    }

    buf[numbytes] = '\0';
    const http_request_t request = parse_request(buf);

    if (request.content_length < 0 && request.request_line.method == HTTP_POST) {
        respond(client, 400, "Bad Request");
        return;
    }

    switch (request.request_line.method) {
        case HTTP_GET:
        case HTTP_POST:
            handle_request(client, request);
            break;
        default:
            respond(client, 501, "Method Not Implemented");
            break;
    }
    close(client);
}

static void respond(const int client, const int code, const char* reason) {
    if(!reason) return;

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

static void respond_file(int client, http_request_t request, FILE* resource) {
    if(!resource) {
        fprintf(stderr, "Responding with NULL file\n");
        return;
    }

    char buf[BUFFER_SIZE];
    size_t bytes_read;

    sprintf(buf, "HTTP/1.0 200 OK");
    send(client, buf, strlen(buf), 0);

    strcpy(buf, SERVERSTRING);
    send(client, buf, strlen(buf), 0);

    if (request.body) {
        sprintf(buf, "Content-Length: %d", request.content_length);
        send(client, buf, strlen(buf), 0);
    }

    sprintf(buf, "Content-Type: %s\r\n", get_mime_type(request.request_line.path));
    send(client, buf, strlen(buf), 0);

    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);

    while ((bytes_read = fread(buf, 1, sizeof(buf) - 1, resource)) > 0) {
        buf[bytes_read] = '\0';
        if (request.body) {
            char* body_end = strstr(buf, "</body>");
            if (body_end) {
                *body_end = '\0';
                send(client, buf, strlen(buf), 0);
                send(client, request.body, strlen(request.body), 0);
                send(client, "</body>", 7, 0);
            }
        } else {
            send(client, buf, bytes_read, 0);
        }
    }
}

static void handle_request(const int client, const http_request_t request) {
    FILE* file = NULL;

    file = fopen(request.request_line.path, "r");

    if (file == NULL)
        respond(client, 404, "Not Found");
    else {
        respond_file(client, request, file);
        fclose(file);
    }
}

static const char* get_mime_type(const char* filename) {
    if(!filename) {
        fprintf(stderr, "NULL filename provided\n");
        return NULL;
    }

    const char* ext = strchr(filename, '.');

    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";

    return "application/octet-stream";
}
