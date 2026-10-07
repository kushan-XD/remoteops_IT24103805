#ifndef FILE_TRANSFER_H
#define FILE_TRANSFER_H

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "file_rules.h"
#include "stream_io.h"
#include "logging.h"

/* Return 1 to continue this session, or 0 to close it. */
static inline int file_reply(int fd, const char *message, int keep_open)
{
    if (stream_send_all(fd, message, strlen(message)) == -1) {
        log_event(fd, "SEND_ERROR", "file response failed");
        return 0;
    }
    return keep_open;
}

/* Open each directory without following symbolic links. */
static inline int file_storage_open(void)
{
    if (mkdir("agentfiles", 0700) == -1 && errno != EEXIST)
        return -1;

    int root = open("agentfiles",
                    O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (root == -1)
        return -1;

    if (mkdirat(root, "IT24103805", 0700) == -1 && errno != EEXIST) {
        close(root);
        return -1;
    }

    int directory = openat(root, "IT24103805",
                           O_RDONLY | O_DIRECTORY |
                           O_NOFOLLOW | O_CLOEXEC);
    close(root);
    return directory;
}

/* O_EXCL prevents reusing or following an existing temporary file. */
static inline int file_temp_open(int directory, char *name, size_t capacity)
{
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    static unsigned long sequence = 0;

    for (unsigned int attempt = 0; attempt < 100; ++attempt) {
        if (pthread_mutex_lock(&mutex) != 0)
            return -1;
        unsigned long number = ++sequence;
        pthread_mutex_unlock(&mutex);

        int length = snprintf(name, capacity, ".upload-%ld-%lu",
                              (long)getpid(), number);
        if (length < 0 || (size_t)length >= capacity)
            return -1;

        int output = openat(directory, name,
                            O_WRONLY | O_CREAT | O_EXCL |
                            O_NOFOLLOW | O_CLOEXEC, 0600);
        if (output != -1 || errno != EEXIST)
            return output;
    }

    return -1;
}

static inline int file_write_all(int fd, const void *data, size_t length)
{
    const unsigned char *bytes = data;
    size_t written = 0;

    while (written < length) {
        ssize_t count = write(fd, bytes + written, length - written);
        if (count == -1 && errno == EINTR)
            continue;
        if (count <= 0)
            return -1;
        written += (size_t)count;
    }

    return 0;
}

static inline int handle_put(
    int fd, struct stream_reader *reader, const char *command)
{
    char name[FILE_NAME_LIMIT + 1];
    uint64_t size;

    if (strncmp(command, "PUT ", 4) != 0)
        return file_reply(fd, "ERR 006 BAD_REQUEST SID:5083\n", 0);

    const char *arguments = command + 4;
    const char *separator = strchr(arguments, ' ');
    if (separator == NULL)
        return file_reply(fd, "ERR 006 BAD_REQUEST SID:5083\n", 0);

    size_t name_length = (size_t)(separator - arguments);
    if (name_length == 0 || name_length > FILE_NAME_LIMIT)
        return file_reply(fd, "ERR 006 BAD_REQUEST SID:5083\n", 0);

    memcpy(name, arguments, name_length);
    name[name_length] = '\0';

    if (!file_name_valid(name) ||
        !file_size_parse(separator + 1, &size))
        return file_reply(fd, "ERR 006 BAD_REQUEST SID:5083\n", 0);

    /*
     * Rejected uploads close the session: unread body bytes must never
     * be interpreted as commands.
     */
    if (size > FILE_SIZE_LIMIT)
        return file_reply(fd, "ERR 004 FILE_TOO_LARGE SID:5083\n", 0);

    int directory = file_storage_open();
    if (directory == -1)
        return file_reply(fd, "ERR 008 IO_ERROR SID:5083\n", 0);

    struct stat existing;
    int inspected = fstatat(directory, name, &existing,
                            AT_SYMLINK_NOFOLLOW);
    if ((inspected == 0 && !S_ISREG(existing.st_mode)) ||
        (inspected == -1 && errno != ENOENT)) {
        close(directory);
        return file_reply(fd, "ERR 008 IO_ERROR SID:5083\n", 0);
    }

    char temporary[80];
    int output = file_temp_open(directory, temporary, sizeof(temporary));
    if (output == -1) {
        close(directory);
        return file_reply(fd, "ERR 008 IO_ERROR SID:5083\n", 0);
    }

    /*
     * Bound an idle receive during an upload. Restore the previous
     * setting before returning to the normal command loop.
     */
    struct timeval previous;
    socklen_t previous_length = sizeof(previous);
    struct timeval timeout = {.tv_sec = 15, .tv_usec = 0};
    int configured = 0;
    int success = 0;

    if (getsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
                   &previous, &previous_length) == -1 ||
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
                   &timeout, sizeof(timeout)) == -1)
        goto cleanup;

    configured = 1;
    unsigned char buffer[8192];
    uint64_t remaining = size;

    while (remaining > 0) {
        size_t count = remaining > sizeof(buffer)
                     ? sizeof(buffer) : (size_t)remaining;
        int result = stream_read_exact(reader, buffer, count);

        if (result != 1) {
            log_event(fd, result == 0 ? "EOF" : "RECEIVE_ERROR",
                      "incomplete PUT body; upload discarded");
            goto cleanup;
        }

        if (file_write_all(output, buffer, count) == -1)
            goto cleanup;

        remaining -= count;
    }

    if (fsync(output) == -1)
        goto cleanup;

    success = 1;

cleanup:
    if (close(output) == -1)
        success = 0;

    if (configured &&
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
                   &previous, previous_length) == -1)
        success = 0;

    /*
     * Rename publishes a complete file atomically. It replaces an
     * existing destination entry without following a destination link.
     * For simultaneous uploads, the last successful rename wins.
     */
    if (success &&
        renameat(directory, temporary, directory, name) == -1)
        success = 0;

    if (!success)
        unlinkat(directory, temporary, 0);

    close(directory);

    if (!success) {
        log_event(fd, "PUT_FAILED", name);
        return file_reply(fd, "ERR 008 IO_ERROR SID:5083\n", 0);
    }

    char detail[256];
    snprintf(detail, sizeof(detail), "%s bytes=%llu",
             name, (unsigned long long)size);
    log_event(fd, "PUT_COMPLETE", detail);

    char response[256];
    snprintf(response, sizeof(response),
             "OK FILE_RECEIVED %s SID:5083\n", name);
    return file_reply(fd, response, 1);
}


/* Send a size header followed by exactly that many raw file bytes. */
static inline int handle_get(int fd, const char *command)
{
    if (strncmp(command, "GET ", 4) != 0 ||
        !file_name_valid(command + 4))
        return file_reply(fd, "ERR 006 BAD_REQUEST SID:5083\n", 1);

    const char *name = command + 4;
    int directory = file_storage_open();
    if (directory == -1)
        return file_reply(fd, "ERR 008 IO_ERROR SID:5083\n", 1);

    /*
     * O_NONBLOCK prevents a FIFO from blocking open.
     * fstat then permits only regular files.
     */
    int input = openat(directory, name,
                       O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    int open_error = errno;
    close(directory);

    if (input == -1) {
        if (open_error == ENOENT || open_error == ELOOP)
            return file_reply(fd, "ERR 005 FILE_NOT_FOUND SID:5083\n", 1);
        return file_reply(fd, "ERR 008 IO_ERROR SID:5083\n", 1);
    }

    struct stat info;
    if (fstat(input, &info) == -1) {
        close(input);
        return file_reply(fd, "ERR 008 IO_ERROR SID:5083\n", 1);
    }

    if (!S_ISREG(info.st_mode) || info.st_size < 0) {
        close(input);
        return file_reply(fd, "ERR 005 FILE_NOT_FOUND SID:5083\n", 1);
    }

    uint64_t size = (uint64_t)info.st_size;
    if (size > FILE_SIZE_LIMIT) {
        close(input);
        return file_reply(fd, "ERR 004 FILE_TOO_LARGE SID:5083\n", 1);
    }

    struct timeval previous;
    socklen_t previous_length = sizeof(previous);
    struct timeval timeout = {.tv_sec = 15, .tv_usec = 0};

    if (getsockopt(fd, SOL_SOCKET, SO_SNDTIMEO,
                   &previous, &previous_length) == -1 ||
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO,
                   &timeout, sizeof(timeout)) == -1) {
        close(input);
        return file_reply(fd, "ERR 008 IO_ERROR SID:5083\n", 0);
    }

    char header[256];
    int length = snprintf(header, sizeof(header),
                          "OK FILE_SEND %s %llu SID:5083\n",
                          name, (unsigned long long)size);

    int success = 0;
    if (length < 0 || (size_t)length >= sizeof(header) ||
        stream_send_all(fd, header, (size_t)length) == -1)
        goto get_cleanup;

    unsigned char buffer[8192];
    uint64_t remaining = size;

    while (remaining > 0) {
        size_t wanted = remaining > sizeof(buffer)
                      ? sizeof(buffer) : (size_t)remaining;
        ssize_t count;

        do {
            count = read(input, buffer, wanted);
        } while (count == -1 && errno == EINTR);

        if (count <= 0 ||
            stream_send_all(fd, buffer, (size_t)count) == -1)
            goto get_cleanup;

        remaining -= (uint64_t)count;
    }

    success = 1;

get_cleanup:
    close(input);
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO,
                   &previous, previous_length) == -1)
        success = 0;

    if (!success) {
        /*
         * After a file header, an error line would become file bytes.
         * Close instead, allowing the receiver to detect truncation.
         */
        log_event(fd, "GET_FAILED", name);
        return 0;
    }

    char detail[256];
    snprintf(detail, sizeof(detail), "%s bytes=%llu",
             name, (unsigned long long)size);
    log_event(fd, "GET_COMPLETE", detail);
    return 1;
}

#endif
