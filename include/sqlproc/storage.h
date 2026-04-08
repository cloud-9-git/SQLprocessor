#ifndef SQLPROC_STORAGE_H
#define SQLPROC_STORAGE_H

#include "sqlproc/schema.h"

/* storage가 한 row를 읽을 때마다 호출하는 콜백 타입입니다. */
typedef SqlStatus (*RowVisitor)(const Row *row, void *ctx, SqlError *err);

/* schema 순서의 row를 `<db-root>/data/<table>.rows` 끝에 추가합니다. */
SqlStatus storage_append_row(const char *db_root, const TableSchema *schema, const Row *row, SqlError *err);

/* row 파일 전체를 순차 탐색하면서 각 row를 RowVisitor에 전달합니다. */
SqlStatus storage_scan_rows(const char *db_root, const TableSchema *schema, RowVisitor visit, void *ctx, SqlError *err);

#endif
