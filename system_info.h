#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

#include <stddef.h>
#include <stdio.h>
#include <sys/sysinfo.h>

/* Format real Linux statistics without a protocol prefix or SID. */
static inline int format_system_stats(char *output, size_t capacity)
{
    struct sysinfo information;

    if (sysinfo(&information) == -1)
        return -1;

    double load = (double)information.loads[0] / 65536.0;

    double memory_mib =
        ((double)information.totalram - (double)information.freeram)
        * (double)information.mem_unit / (1024.0 * 1024.0);

    int length = snprintf(output, capacity, "%.2f %.2f %ld",
                          load, memory_mib, information.uptime);

    if (length < 0 || (size_t)length >= capacity)
        return -1;

    return 0;
}

#endif
