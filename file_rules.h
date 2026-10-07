#ifndef FILE_RULES_H
#define FILE_RULES_H

#include <stddef.h>
#include <stdint.h>

#define FILE_SIZE_LIMIT (UINT64_C(64) * 1024 * 1024)
#define FILE_NAME_LIMIT 128

/*
 * Accept a single filename, not a path.
 * The first character must be a letter or digit.
 * Remaining characters may also include '.', '_' and '-'.
 */
static inline int file_name_valid(const char *name)
{
    size_t length = 0;

    for (const unsigned char *p = (const unsigned char *)name;
         *p != '\0'; ++p) {
        int alphanumeric =
            (*p >= 'a' && *p <= 'z') ||
            (*p >= 'A' && *p <= 'Z') ||
            (*p >= '0' && *p <= '9');

        if (length == 0 && !alphanumeric)
            return 0;

        if (!alphanumeric && *p != '.' && *p != '_' && *p != '-')
            return 0;

        if (++length > FILE_NAME_LIMIT)
            return 0;
    }

    return length != 0;
}

/*
 * Accept decimal digits only. Reject signs, spaces and overflow.
 * Parsing is separate from enforcing FILE_SIZE_LIMIT so an oversized
 * upload can receive the protocol's FILE_TOO_LARGE response.
 */
static inline int file_size_parse(const char *text, uint64_t *size)
{
    uint64_t value = 0;

    if (*text == '\0')
        return 0;

    for (const unsigned char *p = (const unsigned char *)text;
         *p != '\0'; ++p) {
        if (*p < '0' || *p > '9')
            return 0;

        unsigned int digit = (unsigned int)(*p - '0');

        if (value > (UINT64_MAX - digit) / 10)
            return 0;

        value = value * 10 + digit;
    }

    *size = value;
    return 1;
}

#endif
