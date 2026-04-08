#ifndef SQLPROC_STORAGE_H
#define SQLPROC_STORAGE_H

#include "sqlproc/schema.h"

typedef SqlStatus (*RowVisitor)(const Row *row, void *ctx, SqlError *err);

SqlStatus storage_append_row(const char *db_root, const TableSchema *schema, const Row *row, SqlError *err);
SqlStatus storage_scan_rows(const char *db_root, const TableSchema *schema, RowVisitor visit, void *ctx, SqlError *err);

#endif
