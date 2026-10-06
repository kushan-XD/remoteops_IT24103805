#include <arpa/inet.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

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

    if (close(fd) == -1) {
        perror("close");
        return 1;
    }

    return 0;
}
