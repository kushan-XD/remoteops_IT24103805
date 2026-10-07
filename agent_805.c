#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#include <pthread.h>
#include <stdlib.h>
#include "auth_session.h"

#define AGENT_PORT 9410

static void *client_worker(void *argument)
{
    int client_fd = *(int *)argument;
    free(argument);

    log_event(client_fd, "CONNECT", "worker started");
    serve_session(client_fd);
    log_event(client_fd, "DISCONNECT", "session ended");

    if (close(client_fd) == -1)
        perror("close client");

    return NULL;
}

int main(void)
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("socket");
        return 1;
    }

    int reuse = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
                   &reuse, sizeof(reuse)) == -1) {
        perror("setsockopt");
        close(listen_fd);
        return 1;
    }

    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons(AGENT_PORT);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_fd, (struct sockaddr *)&address,
             sizeof(address)) == -1) {
        perror("bind");
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, 10) == -1) {
        perror("listen");
        close(listen_fd);
        return 1;
    }

    printf("Agent IT24103805 listening on TCP port %d\n", AGENT_PORT);

    pthread_attr_t attributes;
    int error = pthread_attr_init(&attributes);
    if (error != 0) {
        fprintf(stderr, "pthread_attr_init: %s\n", strerror(error));
        close(listen_fd);
        return 1;
    }

    error = pthread_attr_setdetachstate(
        &attributes, PTHREAD_CREATE_DETACHED);
    if (error != 0) {
        fprintf(stderr, "pthread_attr_setdetachstate: %s\n",
                strerror(error));
        pthread_attr_destroy(&attributes);
        close(listen_fd);
        return 1;
    }

    for (;;) {
        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd == -1) {
            if (errno == EINTR)
                continue;
            perror("accept");
            pthread_attr_destroy(&attributes);
            close(listen_fd);
            return 1;
        }

        int *argument = malloc(sizeof(*argument));
        if (argument == NULL) {
            perror("malloc");
            close(client_fd);
            continue;
        }

        *argument = client_fd;

        pthread_t worker;
        error = pthread_create(
            &worker, &attributes, client_worker, argument);

        if (error != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(error));
            free(argument);
            close(client_fd);
            continue;
        }

        puts("Client assigned to an independent worker.");
        /* The worker now owns argument and client_fd. */
    }
}
