#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#include "auth_session.h"

#define AGENT_PORT 9410

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

    for (;;) {
        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd == -1) {
            if (errno == EINTR)
                continue;
            perror("accept");
            close(listen_fd);
            return 1;
        }

        puts("Client connected; starting command session.");
        serve_session(client_fd);
        if (close(client_fd) == -1)
            perror("close client");
    }
}
