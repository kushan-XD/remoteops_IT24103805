#ifndef AUTH_SESSION_H
#define AUTH_SESSION_H

#include <stdio.h>
#include <string.h>
#include "stream_io.h"
#include "system_info.h"
#include "process_info.h"

#define AUTH_TOKEN "OPS-3805"
#define SID_TAG " SID:5083\n"

static inline void serve_session(int fd)
{
    struct stream_reader reader = {.fd = fd};
    int authenticated = 0;
    char command[8192];

    for (;;) {
        int result = stream_read_line(&reader, command, sizeof(command));

        if (result == 0)
            return;

        if (result == -1) {
            perror("receive");
            return;
        }

        if (result == -2) {
            const char *error = "ERR 006 BAD_REQUEST" SID_TAG;
            if (stream_send_all(fd, error, strlen(error)) == -1)
                perror("send error response");
            return;
        }

        char process_list[4096];
        char process_response[8192];
        char statistics[128];
        char system_response[256];
        const char *response;
        int quit = 0;

        if (strcmp(command, "AUTH " AUTH_TOKEN) == 0) {
            authenticated = 1;
            response = "OK AUTHENTICATED" SID_TAG;
        } else if (strcmp(command, "AUTH") == 0 ||
                   strncmp(command, "AUTH ", 5) == 0) {
            authenticated = 0;
            response = "ERR 001 AUTH_FAILED" SID_TAG;
        } else if (!authenticated) {
            response = "ERR 003 AUTH_REQUIRED" SID_TAG;
        } else if (strcmp(command, "LISTPROC") == 0) {
            if (format_process_list(process_list, sizeof(process_list)) == -1) {
                response = "ERR 008 IO_ERROR" SID_TAG;
            } else {
                int length = snprintf(
                    process_response, sizeof(process_response),
                    "OK PROCS %s" SID_TAG, process_list);

                if (length < 0 ||
                    (size_t)length >= sizeof(process_response))
                    response = "ERR 008 IO_ERROR" SID_TAG;
                else
                    response = process_response;
            }
        } else if (strcmp(command, "SYSINFO") == 0) {
            if (format_system_stats(statistics, sizeof(statistics)) == -1) {
                response = "ERR 008 IO_ERROR" SID_TAG;
            } else {
                int length = snprintf(
                    system_response, sizeof(system_response),
                    "OK SYSINFO %s" SID_TAG, statistics);

                if (length < 0 ||
                    (size_t)length >= sizeof(system_response))
                    response = "ERR 008 IO_ERROR" SID_TAG;
                else
                    response = system_response;
            }
        } else if (strcmp(command, "QUIT") == 0) {
            response = "OK BYE" SID_TAG;
            quit = 1;
        } else {
            response = "ERR 012 UNKNOWN_COMMAND" SID_TAG;
        }

        if (stream_send_all(fd, response, strlen(response)) == -1) {
            perror("send response");
            return;
        }

        if (quit)
            return;
    }
}

#endif
