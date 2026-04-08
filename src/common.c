#include "common.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *sp_strdup(const char *source) {
    size_t length = strlen(source);
    char *copy = (char *)malloc(length + 1);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, source, length + 1);
    return copy;
}

char *sp_strdup_n(const char *source, size_t length) {
    char *copy = (char *)malloc(length + 1);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, source, length);
    copy[length] = '\0';
    return copy;
}

bool sp_equals_ignore_case(const char *left, const char *right) {
    if (left == NULL || right == NULL) {
        return left == right;
    }

    while (*left != '\0' && *right != '\0') {
        if (tolower((unsigned char)*left) != tolower((unsigned char)*right)) {
            return false;
        }
        left++;
        right++;
    }

    return *left == '\0' && *right == '\0';
}

void sp_set_error(char *buffer, size_t buffer_size, const char *format, ...) {
    va_list args;

    if (buffer == NULL || buffer_size == 0) {
        return;
    }

    va_start(args, format);
    vsnprintf(buffer, buffer_size, format, args);
    va_end(args);
}
