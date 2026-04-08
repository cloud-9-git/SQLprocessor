#include "sqlproc/parser.h"

#include "sqlproc/lexer.h"

#include <stdlib.h>
#include <string.h>

/* parser가 현재 바라보는 토큰 위치와 statement 번호를 추적합니다. */
typedef struct {
    TokenArray tokens;
    size_t current;
    size_t statement_index;
} ParserState;

/* AST 노드에 저장할 문자열을 새 메모리로 복사합니다. */
static char *dup_string(const char *text, SqlError *err) {
    size_t length;
    char *copy;

    length = strlen(text);
    copy = (char *)malloc(length + 1U);
    if (copy == NULL) {
        sql_error_set(err, 0, 0, 0, "out of memory");
        return NULL;
    }

    memcpy(copy, text, length + 1U);
    return copy;
}

/* 현재 읽고 있는 토큰을 반환합니다. */
static Token *peek(ParserState *parser) {
    return &parser->tokens.items[parser->current];
}

/* 직전에 소비한 토큰을 반환합니다. */
static Token *previous(ParserState *parser) {
    return &parser->tokens.items[parser->current - 1U];
}

/* 파일 끝 토큰에 도달했는지 확인합니다. */
static int is_at_end(ParserState *parser) {
    return peek(parser)->type == TOKEN_EOF;
}

/* 토큰 하나를 소비하고 이전 토큰을 돌려줍니다. */
static Token *advance(ParserState *parser) {
    if (!is_at_end(parser)) {
        parser->current++;
    }
    return previous(parser);
}

/* 현재 토큰이 원하는 타입이면 소비하고 성공을 반환합니다. */
static int match(ParserState *parser, TokenType type) {
    if (peek(parser)->type != type) {
        return 0;
    }
    advance(parser);
    return 1;
}

/* 현재 토큰이 기대한 타입인지 확인하고, 아니면 위치 정보가 포함된 오류를 만듭니다. */
static Token *expect(ParserState *parser, TokenType type, SqlError *err, const char *message) {
    Token *token = peek(parser);

    if (token->type != type) {
        sql_error_set(err, token->line, token->column, (int)parser->statement_index, "%s, found %s",
                      message, token_type_name(token->type));
        return NULL;
    }

    return advance(parser);
}

/* 새 AST 표현식 노드를 0으로 초기화해서 생성합니다. */
static AstExpr *new_expr(AstExprKind kind, SqlError *err) {
    AstExpr *expr = (AstExpr *)calloc(1U, sizeof(AstExpr));

    if (expr == NULL) {
        sql_error_set(err, 0, 0, 0, "out of memory");
        return NULL;
    }

    expr->kind = kind;
    return expr;
}

/* 표현식 트리를 재귀적으로 해제합니다. */
static void ast_expr_free(AstExpr *expr) {
    if (expr == NULL) {
        return;
    }

    switch (expr->kind) {
        case AST_EXPR_LITERAL:
            value_free(&expr->as.literal);
            break;
        case AST_EXPR_COLUMN_REF:
            free(expr->as.column_name);
            break;
        case AST_EXPR_BINARY:
            ast_expr_free(expr->as.binary.left);
            ast_expr_free(expr->as.binary.right);
            break;
        default:
            break;
    }

    free(expr);
}

/* 숫자, 문자열, 불리언 토큰을 내부 Value로 바꿉니다. */
static SqlStatus parse_literal(ParserState *parser, Value *out_value, SqlError *err) {
    Token *token = peek(parser);
    char *end_ptr = NULL;

    if (token->type == TOKEN_NUMBER) {
        long long number;
        advance(parser);
        number = strtoll(token->lexeme, &end_ptr, 10);
        if (end_ptr == NULL || *end_ptr != '\0') {
            sql_error_set(err, token->line, token->column, (int)parser->statement_index,
                          "invalid integer literal '%s'", token->lexeme);
            return SQL_STATUS_ERROR;
        }
        *out_value = value_make_int(number);
        return SQL_STATUS_OK;
    }

    if (token->type == TOKEN_STRING) {
        advance(parser);
        return value_make_text(out_value, token->lexeme, err);
    }

    if (token->type == TOKEN_TRUE) {
        advance(parser);
        *out_value = value_make_bool(1);
        return SQL_STATUS_OK;
    }

    if (token->type == TOKEN_FALSE) {
        advance(parser);
        *out_value = value_make_bool(0);
        return SQL_STATUS_OK;
    }

    sql_error_set(err, token->line, token->column, (int)parser->statement_index, "expected literal");
    return SQL_STATUS_ERROR;
}

/* 표현식의 가장 작은 단위인 컬럼 참조 또는 리터럴을 읽습니다. */
static AstExpr *parse_primary_expr(ParserState *parser, SqlError *err) {
    Token *token = peek(parser);
    AstExpr *expr;
    SqlStatus status;

    if (token->type == TOKEN_IDENTIFIER) {
        expr = new_expr(AST_EXPR_COLUMN_REF, err);
        if (expr == NULL) {
            return NULL;
        }

        expr->as.column_name = dup_string(token->lexeme, err);
        if (expr->as.column_name == NULL) {
            free(expr);
            return NULL;
        }

        advance(parser);
        return expr;
    }

    expr = new_expr(AST_EXPR_LITERAL, err);
    if (expr == NULL) {
        return NULL;
    }

    status = parse_literal(parser, &expr->as.literal, err);
    if (status != SQL_STATUS_OK) {
        free(expr);
        return NULL;
    }

    return expr;
}

/* ⚠️ v1의 WHERE는 `column = literal` 한 가지 형태만 허용합니다.
 * 그래도 내부 표현은 트리로 유지해서 이후 AND/OR 확장이 가능하게 둡니다.
 */
static AstExpr *parse_where_expr(ParserState *parser, SqlError *err) {
    AstExpr *left;
    AstExpr *right;
    AstExpr *expr;

    left = parse_primary_expr(parser, err);
    if (left == NULL) {
        return NULL;
    }

    if (left->kind != AST_EXPR_COLUMN_REF) {
        ast_expr_free(left);
        sql_error_set(err, peek(parser)->line, peek(parser)->column, (int)parser->statement_index,
                      "WHERE left operand must be a column reference");
        return NULL;
    }

    if (expect(parser, TOKEN_EQ, err, "expected '=' in WHERE clause") == NULL) {
        ast_expr_free(left);
        return NULL;
    }

    right = parse_primary_expr(parser, err);
    if (right == NULL) {
        ast_expr_free(left);
        return NULL;
    }

    if (right->kind != AST_EXPR_LITERAL) {
        ast_expr_free(left);
        ast_expr_free(right);
        sql_error_set(err, peek(parser)->line, peek(parser)->column, (int)parser->statement_index,
                      "WHERE right operand must be a literal");
        return NULL;
    }

    expr = new_expr(AST_EXPR_BINARY, err);
    if (expr == NULL) {
        ast_expr_free(left);
        ast_expr_free(right);
        return NULL;
    }

    expr->as.binary.op = AST_BINARY_OP_EQ;
    expr->as.binary.left = left;
    expr->as.binary.right = right;
    return expr;
}

/* 동적 문자열 배열 끝에 새 문자열을 추가합니다. */
static SqlStatus append_string(char ***items, size_t *count, const char *text, SqlError *err) {
    char **grown = (char **)realloc(*items, sizeof(char *) * (*count + 1U));

    if (grown == NULL) {
        sql_error_set(err, 0, 0, 0, "out of memory");
        return SQL_STATUS_OOM;
    }

    *items = grown;
    (*items)[*count] = dup_string(text, err);
    if ((*items)[*count] == NULL) {
        return SQL_STATUS_OOM;
    }
    (*count)++;
    return SQL_STATUS_OK;
}

/* 동적 Value 배열 끝에 새 리터럴 값을 깊은 복사로 추가합니다. */
static SqlStatus append_value(Value **items, size_t *count, const Value *value, SqlError *err) {
    Value *grown = (Value *)realloc(*items, sizeof(Value) * (*count + 1U));
    SqlStatus status;

    if (grown == NULL) {
        sql_error_set(err, 0, 0, 0, "out of memory");
        return SQL_STATUS_OOM;
    }

    *items = grown;
    status = value_clone(value, &(*items)[*count], err);
    if (status != SQL_STATUS_OK) {
        return status;
    }
    (*count)++;
    return SQL_STATUS_OK;
}

/* `a, b, c` 형태의 식별자 목록을 읽습니다. */
static SqlStatus parse_identifier_list(ParserState *parser, char ***names, size_t *count, SqlError *err) {
    Token *token;
    SqlStatus status;

    do {
        token = expect(parser, TOKEN_IDENTIFIER, err, "expected identifier");
        if (token == NULL) {
            return SQL_STATUS_ERROR;
        }

        status = append_string(names, count, token->lexeme, err);
        if (status != SQL_STATUS_OK) {
            return status;
        }
    } while (match(parser, TOKEN_COMMA));

    return SQL_STATUS_OK;
}

/* `1, 'Alice', true` 형태의 리터럴 목록을 읽습니다. */
static SqlStatus parse_value_list(ParserState *parser, Value **values, size_t *count, SqlError *err) {
    SqlStatus status;

    do {
        Value literal;

        value_init(&literal);
        status = parse_literal(parser, &literal, err);
        if (status != SQL_STATUS_OK) {
            return status;
        }

        status = append_value(values, count, &literal, err);
        value_free(&literal);
        if (status != SQL_STATUS_OK) {
            return status;
        }
    } while (match(parser, TOKEN_COMMA));

    return SQL_STATUS_OK;
}

/* INSERT 문장을 AST 구조체로 해석합니다. */
static SqlStatus parse_insert(ParserState *parser, Statement *statement, SqlError *err) {
    Token *table_token;
    SqlStatus status;

    if (expect(parser, TOKEN_INSERT, err, "expected INSERT") == NULL) {
        return SQL_STATUS_ERROR;
    }
    if (expect(parser, TOKEN_INTO, err, "expected INTO after INSERT") == NULL) {
        return SQL_STATUS_ERROR;
    }

    table_token = expect(parser, TOKEN_IDENTIFIER, err, "expected table name");
    if (table_token == NULL) {
        return SQL_STATUS_ERROR;
    }

    statement->kind = STATEMENT_INSERT;
    statement->as.insert_stmt.table_name = dup_string(table_token->lexeme, err);
    if (statement->as.insert_stmt.table_name == NULL) {
        return SQL_STATUS_OOM;
    }

    /* 컬럼 목록은 사용자가 입력한 순서를 그대로 AST에 담아둡니다.
     * 실제 schema 순서 정렬은 binder 단계에서 수행합니다.
     */
    if (match(parser, TOKEN_LPAREN)) {
        status = parse_identifier_list(parser,
                                       &statement->as.insert_stmt.column_names,
                                       &statement->as.insert_stmt.column_count,
                                       err);
        if (status != SQL_STATUS_OK) {
            return status;
        }

        if (expect(parser, TOKEN_RPAREN, err, "expected ')' after column list") == NULL) {
            return SQL_STATUS_ERROR;
        }
    }

    if (expect(parser, TOKEN_VALUES, err, "expected VALUES after INSERT target") == NULL) {
        return SQL_STATUS_ERROR;
    }
    if (expect(parser, TOKEN_LPAREN, err, "expected '(' before VALUES list") == NULL) {
        return SQL_STATUS_ERROR;
    }

    status = parse_value_list(parser, &statement->as.insert_stmt.values, &statement->as.insert_stmt.value_count, err);
    if (status != SQL_STATUS_OK) {
        return status;
    }

    if (expect(parser, TOKEN_RPAREN, err, "expected ')' after VALUES list") == NULL) {
        return SQL_STATUS_ERROR;
    }

    return SQL_STATUS_OK;
}

/* SELECT 문장을 AST 구조체로 해석합니다. */
static SqlStatus parse_select(ParserState *parser, Statement *statement, SqlError *err) {
    Token *table_token;
    SqlStatus status = SQL_STATUS_OK;

    if (expect(parser, TOKEN_SELECT, err, "expected SELECT") == NULL) {
        return SQL_STATUS_ERROR;
    }

    statement->kind = STATEMENT_SELECT;
    /* SELECT는 우선 문법 형태만 AST에 기록하고,
     * 실제 컬럼 존재 여부 검사는 binder에 넘깁니다.
     */
    if (match(parser, TOKEN_STAR)) {
        statement->as.select_stmt.select_all = 1;
    } else {
        status = parse_identifier_list(parser,
                                       &statement->as.select_stmt.column_names,
                                       &statement->as.select_stmt.column_count,
                                       err);
        if (status != SQL_STATUS_OK) {
            return status;
        }
    }

    if (expect(parser, TOKEN_FROM, err, "expected FROM after SELECT list") == NULL) {
        return SQL_STATUS_ERROR;
    }

    table_token = expect(parser, TOKEN_IDENTIFIER, err, "expected table name after FROM");
    if (table_token == NULL) {
        return SQL_STATUS_ERROR;
    }

    statement->as.select_stmt.table_name = dup_string(table_token->lexeme, err);
    if (statement->as.select_stmt.table_name == NULL) {
        return SQL_STATUS_OOM;
    }

    if (match(parser, TOKEN_WHERE)) {
        statement->as.select_stmt.where_clause = parse_where_expr(parser, err);
        if (statement->as.select_stmt.where_clause == NULL) {
            return SQL_STATUS_ERROR;
        }
    }

    return SQL_STATUS_OK;
}

/* Statement 메모리를 해제하기 전에 안전한 기본값으로 초기화합니다. */
static void statement_init(Statement *statement) {
    statement->kind = STATEMENT_INSERT;
    statement->line = 0;
    statement->column = 0;
    statement->as.insert_stmt.table_name = NULL;
    statement->as.insert_stmt.column_count = 0U;
    statement->as.insert_stmt.column_names = NULL;
    statement->as.insert_stmt.value_count = 0U;
    statement->as.insert_stmt.values = NULL;
}

/* Statement 안에 들어 있는 INSERT/SELECT 전용 메모리를 해제합니다. */
static void statement_free(Statement *statement) {
    size_t index;

    if (statement == NULL) {
        return;
    }

    if (statement->kind == STATEMENT_INSERT) {
        free(statement->as.insert_stmt.table_name);
        for (index = 0U; index < statement->as.insert_stmt.column_count; index++) {
            free(statement->as.insert_stmt.column_names[index]);
        }
        free(statement->as.insert_stmt.column_names);
        for (index = 0U; index < statement->as.insert_stmt.value_count; index++) {
            value_free(&statement->as.insert_stmt.values[index]);
        }
        free(statement->as.insert_stmt.values);
    } else if (statement->kind == STATEMENT_SELECT) {
        free(statement->as.select_stmt.table_name);
        for (index = 0U; index < statement->as.select_stmt.column_count; index++) {
            free(statement->as.select_stmt.column_names[index]);
        }
        free(statement->as.select_stmt.column_names);
        ast_expr_free(statement->as.select_stmt.where_clause);
    }
}

/* AstScript 컨테이너를 비어 있는 상태로 초기화합니다. */
void ast_script_init(AstScript *script) {
    if (script == NULL) {
        return;
    }

    script->statement_count = 0U;
    script->statements = NULL;
}

/* AstScript 전체를 순회하며 statement와 표현식을 해제합니다. */
void ast_script_free(AstScript *script) {
    size_t index;

    if (script == NULL) {
        return;
    }

    for (index = 0U; index < script->statement_count; index++) {
        statement_free(&script->statements[index]);
    }
    free(script->statements);
    script->statements = NULL;
    script->statement_count = 0U;
}

/* 파싱이 끝난 statement를 AstScript 뒤에 추가합니다. */
static SqlStatus append_statement(AstScript *script, const Statement *statement, SqlError *err) {
    Statement *grown = (Statement *)realloc(script->statements, sizeof(Statement) * (script->statement_count + 1U));

    if (grown == NULL) {
        sql_error_set(err, statement->line, statement->column, 0, "out of memory");
        return SQL_STATUS_OOM;
    }

    script->statements = grown;
    script->statements[script->statement_count] = *statement;
    script->statement_count++;
    return SQL_STATUS_OK;
}

/* 🧭 parser의 실제 진입점입니다.
 * lexer 결과를 읽어 세미콜론으로 구분된 여러 statement를 AST로 바꿉니다.
 */
SqlStatus parser_parse_script(const char *sql, AstScript *out_script, SqlError *err) {
    ParserState parser;
    SqlStatus status;

    if (sql == NULL || out_script == NULL) {
        sql_error_set(err, 0, 0, 0, "parser_parse_script received null pointer");
        return SQL_STATUS_ERROR;
    }

    ast_script_init(out_script);
    parser.current = 0U;
    parser.statement_index = 0U;

    status = lexer_tokenize(sql, &parser.tokens, err);
    if (status != SQL_STATUS_OK) {
        return status;
    }

    /* ⭐ 하나의 SQL 파일에 여러 문장이 들어올 수 있으므로 세미콜론 단위로 반복 파싱합니다. */
    while (!is_at_end(&parser)) {
        Statement statement;
        Token *start;

        while (match(&parser, TOKEN_SEMICOLON)) {
        }
        if (is_at_end(&parser)) {
            break;
        }

        start = peek(&parser);
        parser.statement_index = out_script->statement_count + 1U;
        statement_init(&statement);
        statement.line = start->line;
        statement.column = start->column;

        if (start->type == TOKEN_INSERT) {
            status = parse_insert(&parser, &statement, err);
        } else if (start->type == TOKEN_SELECT) {
            status = parse_select(&parser, &statement, err);
        } else {
            sql_error_set(err, start->line, start->column, (int)parser.statement_index,
                          "expected INSERT or SELECT, found %s", token_type_name(start->type));
            status = SQL_STATUS_ERROR;
        }

        if (status != SQL_STATUS_OK) {
            statement_free(&statement);
            ast_script_free(out_script);
            token_array_free(&parser.tokens);
            return status;
        }

        if (expect(&parser, TOKEN_SEMICOLON, err, "expected ';' after statement") == NULL) {
            statement_free(&statement);
            ast_script_free(out_script);
            token_array_free(&parser.tokens);
            return SQL_STATUS_ERROR;
        }

        status = append_statement(out_script, &statement, err);
        if (status != SQL_STATUS_OK) {
            statement_free(&statement);
            ast_script_free(out_script);
            token_array_free(&parser.tokens);
            return status;
        }
    }

    token_array_free(&parser.tokens);
    return SQL_STATUS_OK;
}
