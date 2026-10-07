#ifndef CONTROLLER_FILES_H
#define CONTROLLER_FILES_H

#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "file_rules.h"
#include "stream_io.h"


/* Local end-to-end transfer measurement; not a network-only benchmark. */
static inline void controller_transfer_report(
    const char *operation, uint64_t bytes,
    const struct timespec *started, int timing_valid)
{
    struct timespec finished;
    if (!timing_valid ||
        clock_gettime(CLOCK_MONOTONIC, &finished) == -1)
        return;

    double seconds = (double)(finished.tv_sec - started->tv_sec)
                   + (double)(finished.tv_nsec - started->tv_nsec) / 1e9;

    if (seconds <= 0.0)
        return;

    double mib = (double)bytes / (1024.0 * 1024.0);
    printf("%s: %llu bytes in %.6f s (%.2f MiB/s)\n",
           operation, (unsigned long long)bytes,
           seconds, mib / seconds);
}

/* Return 1 to continue, or 0 when the connection must close. */
static inline int controller_file_command(
    int fd, struct stream_reader *reader, const char *command)
{
    int upload = strncmp(command, "PUT ", 4) == 0;
    const char *name = command + 4;

    if (!file_name_valid(name)) {
        fprintf(stderr, "Use PUT filename or GET filename; no paths.\n");
        return 1;
    }

    struct timespec started;
    int timing_valid =
        clock_gettime(CLOCK_MONOTONIC, &started) == 0;

    char header[512];
    char response[512];

    if (upload) {
        int input = open(name, O_RDONLY | O_NOFOLLOW |
                               O_NONBLOCK | O_CLOEXEC);
        if (input == -1) {
            perror("open upload");
            return 1;
        }

        struct stat info;
        if (fstat(input, &info) == -1) {
            perror("inspect upload");
            close(input);
            return 1;
        }

        if (!S_ISREG(info.st_mode) || info.st_size < 0 ||
            (uint64_t)info.st_size > FILE_SIZE_LIMIT) {
            fprintf(stderr, "Upload must be a regular file at most 64 MiB.\n");
            close(input);
            return 1;
        }

        uint64_t remaining = (uint64_t)info.st_size;
        int length = snprintf(header, sizeof(header),
                              "PUT %s %llu\n", name,
                              (unsigned long long)remaining);
        if (length < 0 || (size_t)length >= sizeof(header) ||
            stream_send_all(fd, header, (size_t)length) == -1) {
            close(input);
            return 0;
        }

        unsigned char buffer[8192];
        while (remaining > 0) {
            size_t wanted = remaining > sizeof(buffer)
                          ? sizeof(buffer) : (size_t)remaining;
            ssize_t count;
            do {
                count = read(input, buffer, wanted);
            } while (count == -1 && errno == EINTR);

            if (count <= 0 ||
                stream_send_all(fd, buffer, (size_t)count) == -1) {
                fprintf(stderr, "Upload interrupted.\n");
                close(input);
                return 0;
            }
            remaining -= (uint64_t)count;
        }
        close(input);

        if (stream_read_line(reader, response, sizeof(response)) != 1)
            return 0;

        puts(response);
        snprintf(header, sizeof(header),
                 "OK FILE_RECEIVED %s SID:5083", name);

        /* Agent closes after rejecting PUT; do not reuse that socket. */
        int accepted = strcmp(response, header) == 0;
        if (accepted)
            controller_transfer_report(
                "Upload", (uint64_t)info.st_size, &started, timing_valid);
        return accepted;
    }

    int length = snprintf(header, sizeof(header), "GET %s\n", name);
    if (length < 0 || (size_t)length >= sizeof(header) ||
        stream_send_all(fd, header, (size_t)length) == -1)
        return 0;

    if (stream_read_line(reader, response, sizeof(response)) != 1)
        return 0;
    puts(response);

    if (strncmp(response, "ERR ", 4) == 0)
        return 1;

    char returned_name[FILE_NAME_LIMIT + 1];
    char size_text[32];
    int consumed = -1;
    uint64_t size;

    int fields = sscanf(response, "OK FILE_SEND %128s %31s SID:5083%n",
                        returned_name, size_text, &consumed);
    if (fields != 2 || consumed < 0 || response[consumed] != '\0' ||
        strcmp(returned_name, name) != 0 ||
        !file_size_parse(size_text, &size) || size > FILE_SIZE_LIMIT) {
        fprintf(stderr, "Invalid download header.\n");
        return 0;
    }

    /*
     * Keep incomplete downloads separate. link() publishes the finished
     * file only if its destination does not already exist.
     */
    char temporary[] = ".download-XXXXXX";
    int output = mkstemp(temporary);
    if (output == -1) {
        perror("create download");
        return 0;
    }
    if (fcntl(output, F_SETFD, FD_CLOEXEC) == -1) {
        close(output);
        unlink(temporary);
        return 0;
    }

    unsigned char buffer[8192];
    uint64_t remaining = size;
    int success = 1;

    while (remaining > 0) {
        size_t count = remaining > sizeof(buffer)
                     ? sizeof(buffer) : (size_t)remaining;
        if (stream_read_exact(reader, buffer, count) != 1) {
            success = 0;
            break;
        }

        size_t written = 0;
        while (written < count) {
            ssize_t result = write(output, buffer + written,
                                   count - written);
            if (result == -1 && errno == EINTR)
                continue;
            if (result <= 0) {
                success = 0;
                break;
            }
            written += (size_t)result;
        }
        if (!success)
            break;
        remaining -= count;
    }

    if (success && fsync(output) == -1)
        success = 0;
    if (close(output) == -1)
        success = 0;

    if (!success) {
        unlink(temporary);
        fprintf(stderr, "Download failed; partial file removed.\n");
        return 0;
    }

    char destination[256];
    snprintf(destination, sizeof(destination), "downloaded-%s", name);

    if (link(temporary, destination) == -1) {
        perror("save download (existing files are not overwritten)");
    } else {
        printf("Saved %llu bytes to %s\n",
               (unsigned long long)size, destination);
        controller_transfer_report(
            "Download", size, &started, timing_valid);
    }
    if (unlink(temporary) == -1)
        perror("remove download temporary file");

    return 1;
}

#endif
