#ifndef PROCESS_INFO_H
#define PROCESS_INFO_H

#include <stdio.h>
#include <string.h>

/* Return a bounded snapshot of up to 50 PID:name entries. */
static inline int format_process_list(char *output, size_t capacity)
{
    if (capacity == 0)
        return -1;

    output[0] = '\0';

    FILE *processes = popen("ps -e -o pid=,comm=", "r");
    if (processes == NULL)
        return -1;

    char line[512];
    size_t used = 0;
    unsigned int entries = 0;
    int failed = 0;

    while (fgets(line, sizeof(line), processes) != NULL) {
        /* Continue reading even after reaching our output limit. */
        if (entries >= 50 || failed)
            continue;

        long pid;
        char name[64];

        if (sscanf(line, "%ld %63[^\n]", &pid, name) != 2)
            continue;

        /* Keep names safe for a single comma-separated text line. */
        for (size_t i = 0; name[i] != '\0'; i++) {
            unsigned char byte = (unsigned char)name[i];
            if (byte <= 32 || byte >= 127 || byte == ',' || byte == ':')
                name[i] = '_';
        }

        int length = snprintf(
            output + used, capacity - used,
            "%s%ld:%s", entries == 0 ? "" : ",", pid, name);

        if (length < 0 || (size_t)length >= capacity - used) {
            failed = 1;
            continue;
        }

        used += (size_t)length;
        entries++;
    }

    if (ferror(processes))
        failed = 1;

    int status = pclose(processes);

    if (failed || status != 0 || entries == 0)
        return -1;

    return 0;
}

#endif
