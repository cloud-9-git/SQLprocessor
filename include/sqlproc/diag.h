#ifndef SQLPROC_DIAG_H
#define SQLPROC_DIAG_H

#include <stdarg.h>

typedef enum {
    SQL_STATUS_OK = 0,
    SQL_STATUS_ERROR = 1,
    SQL_STATUS_OOM = 2,
    SQL_STATUS_IO = 3
} SqlStatus;

typedef struct {
    int line;
    int column;
    int statement_index;
    char message[256];
} SqlError;

void sql_error_init(SqlError *err);
void sql_error_set(SqlError *err, int line, int column, int statement_index, const char *fmt, ...);
void sql_error_vset(SqlError *err, int line, int column, int statement_index, const char *fmt, va_list args);
const char *sql_status_name(SqlStatus status);

#endif
