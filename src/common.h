#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stddef.h>

char *sp_strdup(const char *source);
char *sp_strdup_n(const char *source, size_t length);
bool sp_equals_ignore_case(const char *left, const char *right);
void sp_set_error(char *buffer, size_t buffer_size, const char *format, ...);

#endif
