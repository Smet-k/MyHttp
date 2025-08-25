#include "server.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>

#include "tpool.h"

#define MAXCLIENTS 1024

static void handle_requests(int client);

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

    struct pollfd fds[MAXCLIENTS];
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
                        .function = &handle_requests,
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

static void handle_requests(int client) {
    printf("handling request of client %d\n", client);
    // sleep(1);
    close(client);
}