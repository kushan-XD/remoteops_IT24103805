#include <arpa/inet.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#include "stream_io.h"

#define AGENT_PORT 9410

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <Agent IPv4 address>\n", argv[0]);
        return 1;
    }

    struct sockaddr_in agent = {0};
    agent.sin_family = AF_INET;
    agent.sin_port = htons(AGENT_PORT);

    if (inet_pton(AF_INET, argv[1], &agent.sin_addr) != 1) {
        fprintf(stderr, "Invalid IPv4 address: %s\n", argv[1]);
        return 1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        return 1;
    }

    if (connect(fd, (struct sockaddr *)&agent, sizeof(agent)) == -1) {
        perror("connect");
        close(fd);
        return 1;
    }

    printf("Connected to Agent at %s:%d\n", argv[1], AGENT_PORT);

    struct stream_reader reader = {.fd = fd};
    char command[8192];
    char response[8192];

    for (;;) {
        printf("remoteops> ");
        fflush(stdout);

        if (fgets(command, sizeof(command), stdin) == NULL)
            break;

        size_t length = strlen(command);
        if (length == 0 || command[length - 1] != '\n') {
            fprintf(stderr, "Input must fit on one complete line.\n");
            close(fd);
            return 1;
        }

        if (stream_send_all(fd, command, length) == -1) {
            perror("send command");
            close(fd);
            return 1;
        }

        int result = stream_read_line(&reader, response, sizeof(response));
        if (result != 1) {
            if (result == -1)
                perror("receive response");
            else if (result == 0)
                fprintf(stderr, "Agent closed before replying.\n");
            else
                fprintf(stderr, "Invalid or incomplete response.\n");

            close(fd);
            return 1;
        }

        puts(response);

        if (strcmp(response, "OK BYE SID:5083") == 0)
            break;
    }

    if (close(fd) == -1) {
        perror("close");
        return 1;
    }

    return 0;
}
