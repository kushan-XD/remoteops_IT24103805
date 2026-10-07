#ifndef LOGGING_H
#define LOGGING_H

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define LOG_PATH "remoteops_IT24103805.log"

/* Included by the Agent only: one shared mutex for its workers. */
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline void log_event(int fd, const char *event, const char *detail)
{
    int saved_errno = errno;
    int error = pthread_mutex_lock(&log_mutex);

    if (error != 0) {
        fprintf(stderr, "log mutex: %s\n", strerror(error));
        errno = saved_errno;
        return;
    }

    time_t now = time(NULL);
    struct tm local;
    char timestamp[64] = "TIME_UNAVAILABLE";

    if (now != (time_t)-1 && localtime_r(&now, &local) != NULL) {
        if (strftime(timestamp, sizeof(timestamp),
                     "%Y-%m-%dT%H:%M:%S%z", &local) == 0)
            strcpy(timestamp, "TIME_UNAVAILABLE");
    }

    FILE *log = fopen(LOG_PATH, "a");
    if (log == NULL) {
        perror("open log");
    } else {
        int failed = fprintf(log, "%s fd=%d %s ",
                             timestamp, fd, event) < 0;

        /* Prevent control characters from creating fake log lines. */
        for (const unsigned char *p =
                 (const unsigned char *)detail; *p != '\0'; p++) {
            unsigned char character = *p;
            if (character < 32 || character > 126)
                character = '?';

            if (fputc(character, log) == EOF)
                failed = 1;
        }

        if (fputc('\n', log) == EOF)
            failed = 1;

        if (fclose(log) == EOF)
            failed = 1;

        if (failed)
            fprintf(stderr, "Could not write complete log entry.\n");
    }

    error = pthread_mutex_unlock(&log_mutex);
    if (error != 0)
        fprintf(stderr, "log mutex unlock: %s\n", strerror(error));

    errno = saved_errno;
}

#endif
