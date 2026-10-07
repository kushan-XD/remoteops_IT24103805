#ifndef MONITOR_H
#define MONITOR_H

#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "file_rules.h"
#include "system_info.h"
#include "logging.h"

struct session_monitor {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    pthread_t thread;
    int active;
    int stopping;
    int udp_fd;
    int tcp_fd;
    struct sockaddr_in destination;
};

static inline void *monitor_worker(void *argument)
{
    struct session_monitor *monitor = argument;

    pthread_mutex_lock(&monitor->mutex);

    while (!monitor->stopping) {
        struct timespec deadline;
        if (clock_gettime(CLOCK_REALTIME, &deadline) == -1)
            break;
        deadline.tv_sec += 2;

        int result = 0;
        while (!monitor->stopping && result == 0)
            result = pthread_cond_timedwait(
                &monitor->condition, &monitor->mutex, &deadline);

        if (monitor->stopping)
            break;
        if (result != ETIMEDOUT)
            break;

        pthread_mutex_unlock(&monitor->mutex);

        char statistics[128];
        char packet[256];

        if (format_system_stats(statistics, sizeof(statistics)) == 0) {
            int length = snprintf(packet, sizeof(packet),
                                  "SYSINFO %s SID:5083\n", statistics);

            if (length > 0 && (size_t)length < sizeof(packet)) {
                ssize_t sent = sendto(
                    monitor->udp_fd, packet, (size_t)length,
                    MSG_DONTWAIT | MSG_NOSIGNAL,
                    (struct sockaddr *)&monitor->destination,
                    sizeof(monitor->destination));

                if (sent != length)
                    log_event(monitor->tcp_fd, "UDP_SEND_ERROR",
                              "monitor datagram not sent");
            }
        }

        pthread_mutex_lock(&monitor->mutex);
    }

    pthread_mutex_unlock(&monitor->mutex);
    return NULL;
}

static inline int monitor_init(struct session_monitor *monitor, int fd)
{
    memset(monitor, 0, sizeof(*monitor));
    monitor->tcp_fd = fd;
    monitor->udp_fd = -1;

    if (pthread_mutex_init(&monitor->mutex, NULL) != 0)
        return -1;
    if (pthread_cond_init(&monitor->condition, NULL) != 0) {
        pthread_mutex_destroy(&monitor->mutex);
        return -1;
    }
    return 0;
}

static inline void monitor_stop(struct session_monitor *monitor)
{
    if (!monitor->active)
        return;

    pthread_mutex_lock(&monitor->mutex);
    monitor->stopping = 1;
    pthread_cond_signal(&monitor->condition);
    pthread_mutex_unlock(&monitor->mutex);

    pthread_join(monitor->thread, NULL);
    close(monitor->udp_fd);
    monitor->udp_fd = -1;
    monitor->active = 0;

    log_event(monitor->tcp_fd, "MONITOR_STOP", "UDP worker joined");
}

static inline const char *monitor_command(
    struct session_monitor *monitor, const char *command)
{
    if (strcmp(command, "MONITOR STOP") == 0) {
        monitor_stop(monitor);
        return "OK MONITOR_STOPPED SID:5083\n";
    }

    const char *prefix = "MONITOR START ";
    uint64_t port;

    if (strncmp(command, prefix, strlen(prefix)) != 0 ||
        !file_size_parse(command + strlen(prefix), &port) ||
        port == 0 || port > 65535)
        return "ERR 006 BAD_REQUEST SID:5083\n";

    /*
     * Use the authenticated TCP client's address.
     * The client supplies only the destination UDP port.
     */
    struct sockaddr_in peer;
    socklen_t peer_length = sizeof(peer);
    if (getpeername(monitor->tcp_fd,
                    (struct sockaddr *)&peer, &peer_length) == -1 ||
        peer.sin_family != AF_INET)
        return "ERR 008 IO_ERROR SID:5083\n";

    int udp = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (udp == -1)
        return "ERR 008 IO_ERROR SID:5083\n";

    /* A repeated START replaces this session's previous monitor. */
    monitor_stop(monitor);
    monitor->destination = peer;
    monitor->destination.sin_port = htons((unsigned short)port);
    monitor->udp_fd = udp;
    monitor->stopping = 0;

    int result = pthread_create(
        &monitor->thread, NULL, monitor_worker, monitor);
    if (result != 0) {
        close(udp);
        monitor->udp_fd = -1;
        return "ERR 008 IO_ERROR SID:5083\n";
    }

    monitor->active = 1;
    log_event(monitor->tcp_fd, "MONITOR_START", command);
    return "OK MONITOR_STARTED SID:5083\n";
}

static inline void monitor_destroy(struct session_monitor *monitor)
{
    monitor_stop(monitor);
    pthread_cond_destroy(&monitor->condition);
    pthread_mutex_destroy(&monitor->mutex);
}

#endif
