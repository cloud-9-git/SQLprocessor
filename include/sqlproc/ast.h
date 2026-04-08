#ifndef SQLPROC_AST_H
#define SQLPROC_AST_H

#include <stddef.h>

#include "sqlproc/value.h"

typedef enum {
    AST_EXPR_LITERAL = 0,
    AST_EXPR_COLUMN_REF = 1,
    AST_EXPR_BINARY = 2
} AstExprKind;

typedef enum {
    AST_BINARY_OP_EQ = 0,
    AST_BINARY_OP_AND = 1,
    AST_BINARY_OP_OR = 2
} AstBinaryOp;

typedef struct AstExpr AstExpr;

struct AstExpr {
    AstExprKind kind;
    union {
        Value literal;
        char *column_name;
        struct {
            AstBinaryOp op;
            AstExpr *left;
            AstExpr *right;
        } binary;
    } as;
};

typedef struct {
    char *table_name;
    size_t column_count;
    char **column_names;
    size_t value_count;
    Value *values;
} InsertStmt;

typedef struct {
    char *table_name;
    int select_all;
    size_t column_count;
    char **column_names;
    AstExpr *where_clause;
} SelectStmt;

typedef enum {
    STATEMENT_INSERT = 0,
    STATEMENT_SELECT = 1
} StatementKind;

typedef struct {
    StatementKind kind;
    int line;
    int column;
    union {
        InsertStmt insert_stmt;
        SelectStmt select_stmt;
    } as;
} Statement;

typedef struct {
    size_t statement_count;
    Statement *statements;
} AstScript;

void ast_script_init(AstScript *script);
void ast_script_free(AstScript *script);

#endif
