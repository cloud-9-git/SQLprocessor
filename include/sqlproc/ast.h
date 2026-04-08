#ifndef SQLPROC_AST_H
#define SQLPROC_AST_H

#include <stddef.h>

#include "sqlproc/value.h"

/* AST 표현식 노드의 종류입니다. */
typedef enum {
    AST_EXPR_LITERAL = 0,
    AST_EXPR_COLUMN_REF = 1,
    AST_EXPR_BINARY = 2
} AstExprKind;

/* 현재 AST에서 예약해 둔 이항 연산자 종류입니다.
 * v1은 EQ만 실제 실행하지만, AND/OR 확장을 위해 자리를 남겨둡니다.
 */
typedef enum {
    AST_BINARY_OP_EQ = 0,
    AST_BINARY_OP_AND = 1,
    AST_BINARY_OP_OR = 2
} AstBinaryOp;

typedef struct AstExpr AstExpr;

/* SQL 표현식을 트리 구조로 저장합니다.
 * 예: `active = true` 는 Binary(ColumnRef, Literal) 형태가 됩니다.
 */
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

/* INSERT 문장의 원본 의미를 보관합니다.
 * 컬럼 목록은 사용자가 입력한 순서를 그대로 유지합니다.
 */
typedef struct {
    char *table_name;
    size_t column_count;
    char **column_names;
    size_t value_count;
    Value *values;
} InsertStmt;

/* SELECT 문장의 원본 의미를 보관합니다.
 * select_all이 1이면 `SELECT *`, 아니면 column_names를 사용합니다.
 */
typedef struct {
    char *table_name;
    int select_all;
    size_t column_count;
    char **column_names;
    AstExpr *where_clause;
} SelectStmt;

/* AST 레벨에서 구분하는 statement 종류입니다. */
typedef enum {
    STATEMENT_INSERT = 0,
    STATEMENT_SELECT = 1
} StatementKind;

/* SQL 파일 안의 한 statement입니다.
 * line/column은 오류 메시지를 위해 시작 위치를 저장합니다.
 */
typedef struct {
    StatementKind kind;
    int line;
    int column;
    union {
        InsertStmt insert_stmt;
        SelectStmt select_stmt;
    } as;
} Statement;

/* SQL 파일 전체를 AST로 표현한 결과입니다. */
typedef struct {
    size_t statement_count;
    Statement *statements;
} AstScript;

/* AstScript를 빈 상태로 초기화합니다. */
void ast_script_init(AstScript *script);

/* AstScript 안의 모든 statement와 표현식을 해제합니다. */
void ast_script_free(AstScript *script);

#endif
