#ifndef STREAM_IO_H
#define STREAM_IO_H

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>

/* Create a separate reader for every TCP connection. */
struct stream_reader {
    int fd;
    unsigned char buffer[4096];
    size_t next;
    size_t end;
};

/* Return available bytes, 0 for EOF, or -1 for an error. */
static inline ssize_t stream_read_some(
    struct stream_reader *reader, void *destination, size_t capacity)
{
    if (capacity == 0)
        return 0;

    if (reader->next == reader->end) {
        ssize_t received;

        do {
            received = recv(reader->fd, reader->buffer,
                            sizeof(reader->buffer), 0);
        } while (received == -1 && errno == EINTR);

        if (received <= 0)
            return received;

        reader->next = 0;
        reader->end = (size_t)received;
    }

    size_t available = reader->end - reader->next;
    size_t count = available < capacity ? available : capacity;

    memcpy(destination, reader->buffer + reader->next, count);
    reader->next += count;
    return (ssize_t)count;
}

/*
 * Read a newline-terminated command, removing its newline.
 * Return 1: complete line; 0: EOF before any bytes;
 *       -1: I/O error; -2: invalid, oversized or incomplete line.
 * After -2, the caller must close this connection.
 */
static inline int stream_read_line(
    struct stream_reader *reader, char *line, size_t capacity)
{
    size_t used = 0;

    if (capacity == 0)
        return -2;

    for (;;) {
        unsigned char byte;
        ssize_t received = stream_read_some(reader, &byte, 1);

        if (received == -1)
            return -1;

        if (received == 0)
            return used == 0 ? 0 : -2;

        if (byte == '\n') {
            line[used] = '\0';
            return 1;
        }

        if (byte == '\0' || used >= capacity - 1)
            return -2;

        line[used++] = (char)byte;
    }
}


/*
 * Read exactly length bytes through the connection's buffered reader.
 * Return 1 on success, 0 on premature EOF, or -1 on receive error.
 * On failure, the destination may contain a partial result.
 */
static inline int stream_read_exact(
    struct stream_reader *reader, void *destination, size_t length)
{
    unsigned char *bytes = destination;
    size_t received = 0;

    while (received < length) {
        ssize_t count = stream_read_some(
            reader, bytes + received, length - received);

        if (count == -1)
            return -1;
        if (count == 0)
            return 0;

        received += (size_t)count;
    }

    return 1;
}

/* Send every byte. Return 0 on success or -1 on failure. */
static inline int stream_send_all(
    int fd, const void *data, size_t length)
{
    const unsigned char *bytes = data;
    size_t sent = 0;

    while (sent < length) {
        ssize_t count = send(fd, bytes + sent,
                             length - sent, MSG_NOSIGNAL);

        if (count == -1 && errno == EINTR)
            continue;

        if (count <= 0) {
            if (count == 0)
                errno = EPIPE;
            return -1;
        }

        sent += (size_t)count;
    }

    return 0;
}

#endif
