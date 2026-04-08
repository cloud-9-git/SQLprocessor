#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <stdbool.h>
#include <stddef.h>

#include "parser.h"

bool execute_statements(const StatementList *statements,
                        const char *data_root,
                        char *error_buffer,
                        size_t error_buffer_size);

#endif
