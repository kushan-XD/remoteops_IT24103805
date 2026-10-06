#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

int main(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        return 1;
    }

    puts("TCP socket created successfully.");

    if (close(fd) == -1) {
        perror("close");
        return 1;
    }

    puts("Socket closed successfully.");
    return 0;
}
