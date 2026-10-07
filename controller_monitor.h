#ifndef CONTROLLER_MONITOR_H
#define CONTROLLER_MONITOR_H

#include <arpa/inet.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "file_rules.h"

static struct {
    int fd;
    int active;
    unsigned short port;
    struct in_addr agent_ip;
    pthread_t thread;
    atomic_int stopping;
} controller_monitor;

static void *controller_monitor_worker(void *unused)
{
    (void)unused;
    struct pollfd descriptor = {
        .fd = controller_monitor.fd,
        .events = POLLIN
    };

    while (!atomic_load(&controller_monitor.stopping)) {
        int result = poll(&descriptor, 1, 200);
        if (result == -1 && errno == EINTR)
            continue;
        if (result == -1)
            break;
        if (result == 0)
            continue;
        if (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL))
            break;
        if (!(descriptor.revents & POLLIN))
            continue;

        char packet[512];
        struct sockaddr_in source;
        socklen_t source_length = sizeof(source);
        ssize_t length = recvfrom(
            controller_monitor.fd, packet, sizeof(packet) - 1,
            MSG_DONTWAIT, (struct sockaddr *)&source, &source_length);

        if (length <= 0 || source.sin_family != AF_INET ||
            source.sin_addr.s_addr != controller_monitor.agent_ip.s_addr)
            continue;

        packet[length] = '\0';

        /* Accept the expected numeric SYSINFO format only. */
        double cpu, memory;
        long uptime;
        int consumed = -1;
        int fields = sscanf(packet, "SYSINFO %lf %lf %ld SID:5083%n",
                            &cpu, &memory, &uptime, &consumed);
        if (fields != 3 || consumed < 0 ||
            length != (ssize_t)consumed + 1 ||
            packet[consumed] != '\n' ||
            !(cpu >= 0) || !(memory >= 0) || uptime < 0)
            continue;

        if (atomic_load(&controller_monitor.stopping))
            break;

        printf("\n[UDP] SYSINFO %.2f %.2f %ld SID:5083\nremoteops> ",
               cpu, memory, uptime);
        fflush(stdout);
    }

    return NULL;
}

static void controller_monitor_stop(void)
{
    if (!controller_monitor.active)
        return;

    atomic_store(&controller_monitor.stopping, 1);
    pthread_join(controller_monitor.thread, NULL);
    close(controller_monitor.fd);
    controller_monitor.active = 0;
}

/* Bind before sending START so the first datagram has a receiver. */
static int controller_monitor_start(
    unsigned short port, struct in_addr agent_ip)
{
    if (controller_monitor.active && controller_monitor.port == port)
        return 1;

    int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd == -1) {
        perror("UDP socket");
        return 0;
    }

    struct sockaddr_in local = {0};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&local, sizeof(local)) == -1) {
        perror("UDP bind");
        close(fd);
        return 0;
    }

    controller_monitor_stop();
    controller_monitor.fd = fd;
    controller_monitor.port = port;
    controller_monitor.agent_ip = agent_ip;
    atomic_store(&controller_monitor.stopping, 0);

    int result = pthread_create(
        &controller_monitor.thread, NULL, controller_monitor_worker, NULL);
    if (result != 0) {
        fprintf(stderr, "UDP receiver: %s\n", strerror(result));
        close(fd);
        return 0;
    }

    controller_monitor.active = 1;
    return 1;
}

#endif
