#include "sqlproc/diag.h"

#include <stdio.h>
#include <string.h>

void sql_error_init(SqlError *err) {
    if (err == NULL) {
        return;
    }

    err->line = 0;
    err->column = 0;
    err->statement_index = 0;
    err->message[0] = '\0';
}

void sql_error_vset(SqlError *err, int line, int column, int statement_index, const char *fmt, va_list args) {
    if (err == NULL) {
        return;
    }

    err->line = line;
    err->column = column;
    err->statement_index = statement_index;
    vsnprintf(err->message, sizeof(err->message), fmt, args);
    err->message[sizeof(err->message) - 1] = '\0';
}

void sql_error_set(SqlError *err, int line, int column, int statement_index, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    sql_error_vset(err, line, column, statement_index, fmt, args);
    va_end(args);
}

const char *sql_status_name(SqlStatus status) {
    switch (status) {
        case SQL_STATUS_OK:
            return "OK";
        case SQL_STATUS_ERROR:
            return "ERROR";
        case SQL_STATUS_OOM:
            return "OOM";
        case SQL_STATUS_IO:
            return "IO";
        default:
            return "UNKNOWN";
    }
}
