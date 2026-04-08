#ifndef SQLPROC_SCHEMA_H
#define SQLPROC_SCHEMA_H

#include <stddef.h>

#include "sqlproc/diag.h"
#include "sqlproc/value.h"

/* 테이블의 한 컬럼 정의입니다. 이름과 타입만 관리합니다. */
typedef struct {
    char *name;
    DataType type;
} ColumnDef;

/* 한 테이블의 전체 스키마입니다.
 * storage, binder, planner, executor가 공통으로 사용합니다.
 */
typedef struct {
    char *table_name;
    size_t column_count;
    ColumnDef *columns;
} TableSchema;

/* 스키마 순서대로 정렬된 한 행(row)입니다. */
typedef struct {
    size_t value_count;
    Value *values;
} Row;

/* TableSchema를 빈 상태로 초기화합니다. */
void table_schema_init(TableSchema *schema);

/* TableSchema 내부의 동적 메모리를 모두 해제합니다. */
void table_schema_free(TableSchema *schema);

/* TableSchema를 깊은 복사합니다. */
SqlStatus table_schema_clone(const TableSchema *src, TableSchema *dest, SqlError *err);

/* 컬럼 이름을 찾아 schema 안의 인덱스를 반환합니다. 못 찾으면 -1입니다. */
int table_schema_find_column(const TableSchema *schema, const char *column_name);

/* Row를 빈 상태로 초기화합니다. */
void row_init(Row *row);

/* Row 내부 Value들과 배열 메모리를 모두 해제합니다. */
void row_free(Row *row);

/* Row를 깊은 복사합니다. */
SqlStatus row_clone(const Row *src, Row *dest, SqlError *err);

#endif
