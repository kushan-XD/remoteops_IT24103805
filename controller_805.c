#include <arpa/inet.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#include "stream_io.h"
#include "controller_files.h"
#include "controller_monitor.h"

#define AGENT_PORT 9410

static int controller_run(int argc, char *argv[])
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

        if (strncmp(command, "PUT ", 4) == 0 ||
            strncmp(command, "GET ", 4) == 0) {
            command[length - 1] = '\0';
            if (!controller_file_command(fd, &reader, command)) {
                fprintf(stderr, "File transfer ended the connection.\n");
                close(fd);
                return 1;
            }
            continue;
        }

        int starting_monitor =
            strncmp(command, "MONITOR START ", 14) == 0;

        if (starting_monitor) {
            char port_text[8192];
            size_t port_length = length - 14 - 1;
            memcpy(port_text, command + 14, port_length);
            port_text[port_length] = '\0';

            uint64_t port;
            if (!file_size_parse(port_text, &port) ||
                port == 0 || port > 65535) {
                fprintf(stderr, "UDP port must be 1 through 65535.\n");
                continue;
            }

            if (!controller_monitor_start(
                    (unsigned short)port, agent.sin_addr))
                continue;
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

        if (strcmp(response, "OK MONITOR_STOPPED SID:5083") == 0 ||
            strcmp(command, "AUTH\n") == 0 ||
            strncmp(command, "AUTH ", 5) == 0 ||
            (starting_monitor &&
             strcmp(response, "OK MONITOR_STARTED SID:5083") != 0))
            controller_monitor_stop();


        if (strcmp(response, "OK BYE SID:5083") == 0)
            break;
    }

    if (close(fd) == -1) {
        perror("close");
        return 1;
    }

    return 0;
}

/* Join the UDP receiver on every normal Controller exit path. */
int main(int argc, char *argv[])
{
    int result = controller_run(argc, argv);
    controller_monitor_stop();
    return result;
}
