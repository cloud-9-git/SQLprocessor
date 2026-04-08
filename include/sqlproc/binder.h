#ifndef SQLPROC_BINDER_H
#define SQLPROC_BINDER_H

#include <stddef.h>

#include "sqlproc/ast.h"
#include "sqlproc/catalog.h"

typedef struct {
    size_t column_index;
    Value value;
} BoundPredicate;

typedef struct {
    TableSchema schema;
    Row row;
} BoundInsertStmt;

typedef struct {
    TableSchema schema;
    size_t projection_count;
    size_t *projection_indices;
    int has_filter;
    BoundPredicate filter;
} BoundSelectStmt;

typedef enum {
    BOUND_STATEMENT_INSERT = 0,
    BOUND_STATEMENT_SELECT = 1
} BoundStatementKind;

typedef struct {
    BoundStatementKind kind;
    int line;
    int column;
    union {
        BoundInsertStmt insert_stmt;
        BoundSelectStmt select_stmt;
    } as;
} BoundStatement;

typedef struct {
    size_t statement_count;
    BoundStatement *statements;
} BoundScript;

void bound_script_init(BoundScript *script);
void bound_script_free(BoundScript *script);

SqlStatus binder_bind_script(const Catalog *catalog, const AstScript *ast, BoundScript *out_script, SqlError *err);

#endif
