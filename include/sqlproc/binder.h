#ifndef SQLPROC_BINDER_H
#define SQLPROC_BINDER_H

#include <stddef.h>

#include "sqlproc/ast.h"
#include "sqlproc/catalog.h"

/* WHERE 절을 실행하기 쉬운 형태로 바꾼 단일 조건입니다.
 * 컬럼 이름 대신 schema index를 들고 있습니다.
 */
typedef struct {
    size_t column_index;
    Value value;
} BoundPredicate;

/* INSERT를 실행 직전 형태로 정규화한 결과입니다.
 * row는 항상 schema 순서와 동일합니다.
 */
typedef struct {
    TableSchema schema;
    Row row;
} BoundInsertStmt;

/* SELECT를 실행 직전 형태로 정규화한 결과입니다.
 * projection과 filter 모두 문자열 대신 index 기반으로 바뀝니다.
 */
typedef struct {
    TableSchema schema;
    size_t projection_count;
    size_t *projection_indices;
    int has_filter;
    BoundPredicate filter;
} BoundSelectStmt;

/* binder가 구분하는 statement 종류입니다. */
typedef enum {
    BOUND_STATEMENT_INSERT = 0,
    BOUND_STATEMENT_SELECT = 1
} BoundStatementKind;

/* AST에 스키마 해석 결과를 덧붙인 한 개의 statement입니다. */
typedef struct {
    BoundStatementKind kind;
    int line;
    int column;
    union {
        BoundInsertStmt insert_stmt;
        BoundSelectStmt select_stmt;
    } as;
} BoundStatement;

/* 바인딩이 끝난 statement 목록입니다. */
typedef struct {
    size_t statement_count;
    BoundStatement *statements;
} BoundScript;

/* BoundScript를 빈 상태로 초기화합니다. */
void bound_script_init(BoundScript *script);

/* BoundScript 안의 schema, row, projection 정보를 해제합니다. */
void bound_script_free(BoundScript *script);

/* 🧭 AST를 읽어 실제 스키마 기준으로 검증하고 BoundScript를 만듭니다. */
SqlStatus binder_bind_script(const Catalog *catalog, const AstScript *ast, BoundScript *out_script, SqlError *err);

#endif
