#ifndef SQLPROC_RENDERER_H
#define SQLPROC_RENDERER_H

#include <stdio.h>

#include "sqlproc/executor.h"

SqlStatus renderer_print_results(FILE *stream, const ExecutionOutput *output, SqlError *err);
void renderer_print_error(FILE *stream, const SqlError *err);
void renderer_print_trace(FILE *stream, const PlanScript *plan);
void renderer_print_check_ok(FILE *stream, size_t statement_count);

#endif
