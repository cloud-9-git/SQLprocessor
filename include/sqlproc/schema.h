#ifndef SQLPROC_SCHEMA_H
#define SQLPROC_SCHEMA_H

#include <stddef.h>

#include "sqlproc/diag.h"
#include "sqlproc/value.h"

typedef struct {
    char *name;
    DataType type;
} ColumnDef;

typedef struct {
    char *table_name;
    size_t column_count;
    ColumnDef *columns;
} TableSchema;

typedef struct {
    size_t value_count;
    Value *values;
} Row;

void table_schema_init(TableSchema *schema);
void table_schema_free(TableSchema *schema);
SqlStatus table_schema_clone(const TableSchema *src, TableSchema *dest, SqlError *err);
int table_schema_find_column(const TableSchema *schema, const char *column_name);

void row_init(Row *row);
void row_free(Row *row);
SqlStatus row_clone(const Row *src, Row *dest, SqlError *err);

#endif
