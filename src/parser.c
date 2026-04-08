#include "sqlproc/parser.h"

#include "sqlproc/lexer.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    TokenArray tokens;
    size_t current;
    size_t statement_index;
} ParserState;

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

static Token *peek(ParserState *parser) {
    return &parser->tokens.items[parser->current];
}

static Token *previous(ParserState *parser) {
    return &parser->tokens.items[parser->current - 1U];
}

static int is_at_end(ParserState *parser) {
    return peek(parser)->type == TOKEN_EOF;
}

static Token *advance(ParserState *parser) {
    if (!is_at_end(parser)) {
        parser->current++;
    }
    return previous(parser);
}

static int match(ParserState *parser, TokenType type) {
    if (peek(parser)->type != type) {
        return 0;
    }
    advance(parser);
    return 1;
}

static Token *expect(ParserState *parser, TokenType type, SqlError *err, const char *message) {
    Token *token = peek(parser);

    if (token->type != type) {
        sql_error_set(err, token->line, token->column, (int)parser->statement_index, "%s, found %s",
                      message, token_type_name(token->type));
        return NULL;
    }

    return advance(parser);
}

static AstExpr *new_expr(AstExprKind kind, SqlError *err) {
    AstExpr *expr = (AstExpr *)calloc(1U, sizeof(AstExpr));

    if (expr == NULL) {
        sql_error_set(err, 0, 0, 0, "out of memory");
        return NULL;
    }

    expr->kind = kind;
    return expr;
}

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

static SqlStatus parse_select(ParserState *parser, Statement *statement, SqlError *err) {
    Token *table_token;
    SqlStatus status = SQL_STATUS_OK;

    if (expect(parser, TOKEN_SELECT, err, "expected SELECT") == NULL) {
        return SQL_STATUS_ERROR;
    }

    statement->kind = STATEMENT_SELECT;
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

void ast_script_init(AstScript *script) {
    if (script == NULL) {
        return;
    }

    script->statement_count = 0U;
    script->statements = NULL;
}

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
