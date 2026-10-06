#ifndef EXEC_INFO_H
#define EXEC_INFO_H

#include <stdio.h>
#include <string.h>

/*
 * Return 0: success; 1: name not allowed; -1: execution/I/O error.
 * Client input is never interpolated into a shell command.
 */
static inline int format_exec_result(
    const char *name, char *output, size_t capacity)
{
    const char *fixed_command;

    if (strcmp(name, "DATE") == 0)
        fixed_command = "/usr/bin/date";
    else if (strcmp(name, "UPTIME") == 0)
        fixed_command = "/usr/bin/uptime";
    else if (strcmp(name, "DISKFREE") == 0)
        fixed_command = "/usr/bin/df -h";
    else if (strcmp(name, "HOSTNAME") == 0)
        fixed_command = "/usr/bin/uname -n";
    else if (strcmp(name, "WHOAMI") == 0)
        fixed_command = "/usr/bin/whoami";
    else
        return 1;

    const char marker[] = " [TRUNCATED]";
    if (capacity <= sizeof(marker))
        return -1;

    FILE *pipe = popen(fixed_command, "r");
    if (pipe == NULL)
        return -1;

    size_t used = 0;
    size_t limit = capacity - sizeof(marker);
    int truncated = 0;
    int byte;

    while ((byte = fgetc(pipe)) != EOF) {
        unsigned char character = (unsigned char)byte;

        /* Flatten newlines and other control bytes into spaces. */
        if (character < 32 || character > 126)
            character = ' ';

        if (used < limit)
            output[used++] = (char)character;
        else
            truncated = 1;
    }

    int failed = ferror(pipe);
    int status = pclose(pipe);

    if (failed || status != 0)
        return -1;

    while (used > 0 && output[used - 1] == ' ')
        used--;

    if (truncated) {
        memcpy(output + used, marker, sizeof(marker));
    } else {
        output[used] = '\0';
    }

    return 0;
}

#endif
