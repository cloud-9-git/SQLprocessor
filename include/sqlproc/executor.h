#ifndef SQLPROC_EXECUTOR_H
#define SQLPROC_EXECUTOR_H

#include <stdio.h>

#include "sqlproc/plan.h"

typedef struct {
    size_t column_count;
    char **column_names;
    size_t row_count;
    Row *rows;
} ResultSet;

typedef struct {
    size_t result_count;
    ResultSet *results;
} ExecutionOutput;

typedef struct {
    const char *db_root;
    int trace;
    FILE *trace_stream;
} ExecutionContext;

void execution_output_init(ExecutionOutput *output);
void execution_output_free(ExecutionOutput *output);

SqlStatus executor_run_script(ExecutionContext *ctx, const PlanScript *plan, ExecutionOutput *out_output, SqlError *err);

#endif
