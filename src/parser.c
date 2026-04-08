#include "parser.h"

#include "common.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

typedef enum TokenType {
    TOKEN_EOF,
    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_NUMBER,
    TOKEN_COMMA,
    TOKEN_SEMICOLON,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_DOT,
    TOKEN_ASTERISK,
    TOKEN_EQUALS
} TokenType;

typedef struct Token {
    TokenType type;
    char *lexeme;
    size_t line;
    size_t column;
} Token;

typedef struct Parser {
    const char *input;
    size_t length;
    size_t position;
    size_t line;
    size_t column;
    Token current;
    char *error_buffer;
    size_t error_buffer_size;
    bool has_error;
} Parser;

static void free_token(Token *token) {
    free(token->lexeme);
    token->lexeme = NULL;
}

static void parser_set_error(Parser *parser, const char *format, ...) {
    va_list args;

    if (parser->has_error) {
        return;
    }

    parser->has_error = true;
    if (parser->error_buffer == NULL || parser->error_buffer_size == 0) {
        return;
    }

    va_start(args, format);
    vsnprintf(parser->error_buffer, parser->error_buffer_size, format, args);
    va_end(args);
}

static char parser_peek(const Parser *parser) {
    if (parser->position >= parser->length) {
        return '\0';
    }

    return parser->input[parser->position];
}

static char parser_peek_next(const Parser *parser) {
    if (parser->position + 1 >= parser->length) {
        return '\0';
    }

    return parser->input[parser->position + 1];
}

static void parser_advance(Parser *parser) {
    char current = parser_peek(parser);

    if (current == '\0') {
        return;
    }

    parser->position++;
    if (current == '\n') {
        parser->line++;
        parser->column = 1;
    } else {
        parser->column++;
    }
}

static void parser_skip_whitespace_and_comments(Parser *parser) {
    for (;;) {
        char current = parser_peek(parser);
        char next = parser_peek_next(parser);

        while (isspace((unsigned char)current)) {
            parser_advance(parser);
            current = parser_peek(parser);
            next = parser_peek_next(parser);
        }

        if (current == '-' && next == '-') {
            while (current != '\0' && current != '\n') {
                parser_advance(parser);
                current = parser_peek(parser);
            }
            continue;
        }

        break;
    }
}

static bool append_char(char **buffer, size_t *length, size_t *capacity, char value) {
    if (*length + 1 >= *capacity) {
        size_t new_capacity = (*capacity == 0) ? 16 : (*capacity * 2);
        char *resized = (char *)realloc(*buffer, new_capacity);

        if (resized == NULL) {
            return false;
        }

        *buffer = resized;
        *capacity = new_capacity;
    }

    (*buffer)[(*length)++] = value;
    (*buffer)[*length] = '\0';
    return true;
}

static bool parser_make_token(Parser *parser, TokenType type, const char *lexeme, size_t length, size_t line, size_t column) {
    parser->current.type = type;
    parser->current.line = line;
    parser->current.column = column;
    parser->current.lexeme = sp_strdup_n(lexeme, length);

    if (parser->current.lexeme == NULL) {
        parser_set_error(parser, "메모리 할당에 실패했습니다.");
        return false;
    }

    return true;
}

static bool parser_scan_string(Parser *parser, size_t token_line, size_t token_column) {
    char *buffer = NULL;
    size_t length = 0;
    size_t capacity = 0;

    parser_advance(parser);

    while (parser_peek(parser) != '\0') {
        char current = parser_peek(parser);

        if (current == '\'') {
            if (parser_peek_next(parser) == '\'') {
                if (!append_char(&buffer, &length, &capacity, '\'')) {
                    free(buffer);
                    parser_set_error(parser, "메모리 할당에 실패했습니다.");
                    return false;
                }
                parser_advance(parser);
                parser_advance(parser);
                continue;
            }

            parser_advance(parser);
            parser->current.type = TOKEN_STRING;
            parser->current.line = token_line;
            parser->current.column = token_column;
            parser->current.lexeme = (buffer == NULL) ? sp_strdup("") : buffer;
            if (parser->current.lexeme == NULL) {
                parser_set_error(parser, "메모리 할당에 실패했습니다.");
                return false;
            }
            return true;
        }

        if (!append_char(&buffer, &length, &capacity, current)) {
            free(buffer);
            parser_set_error(parser, "메모리 할당에 실패했습니다.");
            return false;
        }
        parser_advance(parser);
    }

    free(buffer);
    parser_set_error(parser, "%zu:%zu 문자열 리터럴이 닫히지 않았습니다.", token_line, token_column);
    return false;
}

static bool parser_scan_identifier(Parser *parser, size_t token_line, size_t token_column) {
    size_t start = parser->position;

    while (isalnum((unsigned char)parser_peek(parser)) || parser_peek(parser) == '_') {
        parser_advance(parser);
    }

    return parser_make_token(parser,
                             TOKEN_IDENTIFIER,
                             parser->input + start,
                             parser->position - start,
                             token_line,
                             token_column);
}

static bool parser_scan_number(Parser *parser, size_t token_line, size_t token_column) {
    size_t start = parser->position;

    if (parser_peek(parser) == '-') {
        parser_advance(parser);
    }

    while (isdigit((unsigned char)parser_peek(parser))) {
        parser_advance(parser);
    }

    if (parser_peek(parser) == '.') {
        parser_advance(parser);
        while (isdigit((unsigned char)parser_peek(parser))) {
            parser_advance(parser);
        }
    }

    return parser_make_token(parser,
                             TOKEN_NUMBER,
                             parser->input + start,
                             parser->position - start,
                             token_line,
                             token_column);
}

static bool parser_next_token(Parser *parser) {
    size_t token_line;
    size_t token_column;
    char current;

    free_token(&parser->current);
    parser_skip_whitespace_and_comments(parser);

    token_line = parser->line;
    token_column = parser->column;
    current = parser_peek(parser);

    if (current == '\0') {
        return parser_make_token(parser, TOKEN_EOF, "", 0, token_line, token_column);
    }

    if (isalpha((unsigned char)current) || current == '_') {
        return parser_scan_identifier(parser, token_line, token_column);
    }

    if (isdigit((unsigned char)current) || (current == '-' && isdigit((unsigned char)parser_peek_next(parser)))) {
        return parser_scan_number(parser, token_line, token_column);
    }

    if (current == '\'') {
        return parser_scan_string(parser, token_line, token_column);
    }

    parser_advance(parser);

    switch (current) {
        case ',':
            return parser_make_token(parser, TOKEN_COMMA, ",", 1, token_line, token_column);
        case ';':
            return parser_make_token(parser, TOKEN_SEMICOLON, ";", 1, token_line, token_column);
        case '(':
            return parser_make_token(parser, TOKEN_LPAREN, "(", 1, token_line, token_column);
        case ')':
            return parser_make_token(parser, TOKEN_RPAREN, ")", 1, token_line, token_column);
        case '.':
            return parser_make_token(parser, TOKEN_DOT, ".", 1, token_line, token_column);
        case '*':
            return parser_make_token(parser, TOKEN_ASTERISK, "*", 1, token_line, token_column);
        case '=':
            return parser_make_token(parser, TOKEN_EQUALS, "=", 1, token_line, token_column);
        default:
            parser_set_error(parser, "%zu:%zu 지원하지 않는 문자 '%c'를 발견했습니다.", token_line, token_column, current);
            return false;
    }
}

static bool token_is_keyword(const Token *token, const char *keyword) {
    return token->type == TOKEN_IDENTIFIER && sp_equals_ignore_case(token->lexeme, keyword);
}

static bool parser_expect_keyword(Parser *parser, const char *keyword) {
    if (!token_is_keyword(&parser->current, keyword)) {
        parser_set_error(parser,
                         "%zu:%zu '%s' 키워드를 기대했지만 '%s'를 발견했습니다.",
                         parser->current.line,
                         parser->current.column,
                         keyword,
                         parser->current.lexeme);
        return false;
    }

    return parser_next_token(parser);
}

static bool parser_expect_type(Parser *parser, TokenType type, const char *expected_name) {
    if (parser->current.type != type) {
        parser_set_error(parser,
                         "%zu:%zu %s를 기대했지만 '%s'를 발견했습니다.",
                         parser->current.line,
                         parser->current.column,
                         expected_name,
                         parser->current.lexeme);
        return false;
    }

    return true;
}

static bool parser_parse_identifier(Parser *parser, char **out_identifier) {
    char *identifier;

    if (!parser_expect_type(parser, TOKEN_IDENTIFIER, "식별자")) {
        return false;
    }

    identifier = sp_strdup(parser->current.lexeme);
    if (identifier == NULL) {
        parser_set_error(parser, "메모리 할당에 실패했습니다.");
        return false;
    }

    *out_identifier = identifier;
    return parser_next_token(parser);
}

static void free_qualified_name(QualifiedName *name) {
    free(name->schema);
    free(name->table);
    name->schema = NULL;
    name->table = NULL;
}

static bool parser_parse_qualified_name(Parser *parser, QualifiedName *out_name) {
    char *first = NULL;
    char *second = NULL;

    out_name->schema = NULL;
    out_name->table = NULL;

    if (!parser_parse_identifier(parser, &first)) {
        return false;
    }

    if (parser->current.type == TOKEN_DOT) {
        if (!parser_next_token(parser)) {
            free(first);
            return false;
        }

        if (!parser_parse_identifier(parser, &second)) {
            free(first);
            return false;
        }

        out_name->schema = first;
        out_name->table = second;
        return true;
    }

    out_name->schema = sp_strdup("default");
    if (out_name->schema == NULL) {
        free(first);
        parser_set_error(parser, "메모리 할당에 실패했습니다.");
        return false;
    }

    out_name->table = first;
    return true;
}

static bool append_string(char ***items, size_t *count, const char *value, Parser *parser) {
    char **resized = (char **)realloc(*items, sizeof(char *) * (*count + 1));

    if (resized == NULL) {
        parser_set_error(parser, "메모리 할당에 실패했습니다.");
        return false;
    }

    *items = resized;
    resized[*count] = sp_strdup(value);
    if (resized[*count] == NULL) {
        parser_set_error(parser, "메모리 할당에 실패했습니다.");
        return false;
    }

    (*count)++;
    return true;
}

static void free_string_list(char **items, size_t count) {
    size_t index;

    for (index = 0; index < count; index++) {
        free(items[index]);
    }
    free(items);
}

static bool parser_parse_identifier_list(Parser *parser, char ***out_items, size_t *out_count) {
    char **items = NULL;
    size_t count = 0;

    for (;;) {
        if (parser->current.type != TOKEN_IDENTIFIER) {
            free_string_list(items, count);
            parser_set_error(parser,
                             "%zu:%zu 컬럼 이름을 기대했지만 '%s'를 발견했습니다.",
                             parser->current.line,
                             parser->current.column,
                             parser->current.lexeme);
            return false;
        }

        if (!append_string(&items, &count, parser->current.lexeme, parser)) {
            free_string_list(items, count);
            return false;
        }

        if (!parser_next_token(parser)) {
            free_string_list(items, count);
            return false;
        }

        if (parser->current.type != TOKEN_COMMA) {
            break;
        }

        if (!parser_next_token(parser)) {
            free_string_list(items, count);
            return false;
        }
    }

    *out_items = items;
    *out_count = count;
    return true;
}

static bool token_is_value(const Token *token) {
    return token->type == TOKEN_STRING || token->type == TOKEN_NUMBER || token->type == TOKEN_IDENTIFIER;
}

static bool parser_parse_value_list(Parser *parser, char ***out_items, size_t *out_count) {
    char **items = NULL;
    size_t count = 0;

    for (;;) {
        if (!token_is_value(&parser->current)) {
            free_string_list(items, count);
            parser_set_error(parser,
                             "%zu:%zu 값 리터럴을 기대했지만 '%s'를 발견했습니다.",
                             parser->current.line,
                             parser->current.column,
                             parser->current.lexeme);
            return false;
        }

        if (!append_string(&items, &count, parser->current.lexeme, parser)) {
            free_string_list(items, count);
            return false;
        }

        if (!parser_next_token(parser)) {
            free_string_list(items, count);
            return false;
        }

        if (parser->current.type != TOKEN_COMMA) {
            break;
        }

        if (!parser_next_token(parser)) {
            free_string_list(items, count);
            return false;
        }
    }

    *out_items = items;
    *out_count = count;
    return true;
}

static void free_insert_statement(InsertStatement *statement) {
    free_qualified_name(&statement->target);
    free_string_list(statement->columns, statement->column_count);
    free_string_list(statement->values, statement->value_count);
    statement->columns = NULL;
    statement->values = NULL;
    statement->column_count = 0;
    statement->value_count = 0;
}

static void free_select_statement(SelectStatement *statement) {
    free_qualified_name(&statement->source);
    free_string_list(statement->columns, statement->column_count);
    free(statement->where_clause.column);
    free(statement->where_clause.value);
    statement->columns = NULL;
    statement->column_count = 0;
    statement->where_clause.column = NULL;
    statement->where_clause.value = NULL;
}

static bool parser_parse_value(Parser *parser, char **out_value) {
    char *value;

    if (!token_is_value(&parser->current)) {
        parser_set_error(parser,
                         "%zu:%zu 값 리터럴을 기대했지만 '%s'를 발견했습니다.",
                         parser->current.line,
                         parser->current.column,
                         parser->current.lexeme);
        return false;
    }

    value = sp_strdup(parser->current.lexeme);
    if (value == NULL) {
        parser_set_error(parser, "메모리 할당에 실패했습니다.");
        return false;
    }

    *out_value = value;
    return parser_next_token(parser);
}

static bool parser_parse_where_clause(Parser *parser, SelectStatement *statement) {
    if (!parser_expect_keyword(parser, "WHERE")) {
        return false;
    }

    if (!parser_parse_identifier(parser, &statement->where_clause.column)) {
        return false;
    }

    if (!parser_expect_type(parser, TOKEN_EQUALS, "'='")) {
        return false;
    }

    if (!parser_next_token(parser)) {
        return false;
    }

    if (!parser_parse_value(parser, &statement->where_clause.value)) {
        return false;
    }

    statement->has_where_clause = true;
    return true;
}

static bool parser_consume_statement_end(Parser *parser) {
    if (parser->current.type == TOKEN_SEMICOLON) {
        return parser_next_token(parser);
    }

    if (parser->current.type == TOKEN_EOF) {
        return true;
    }

    parser_set_error(parser,
                     "%zu:%zu 문장 종료를 기대했지만 '%s'를 발견했습니다.",
                     parser->current.line,
                     parser->current.column,
                     parser->current.lexeme);
    return false;
}

static bool parser_parse_insert(Parser *parser, Statement *out_statement) {
    InsertStatement statement;

    memset(&statement, 0, sizeof(statement));

    if (!parser_expect_keyword(parser, "INSERT")) {
        return false;
    }

    if (!parser_expect_keyword(parser, "INTO")) {
        return false;
    }

    if (!parser_parse_qualified_name(parser, &statement.target)) {
        free_insert_statement(&statement);
        return false;
    }

    if (parser->current.type == TOKEN_LPAREN) {
        statement.has_column_list = true;

        if (!parser_next_token(parser)) {
            free_insert_statement(&statement);
            return false;
        }

        if (!parser_parse_identifier_list(parser, &statement.columns, &statement.column_count)) {
            free_insert_statement(&statement);
            return false;
        }

        if (!parser_expect_type(parser, TOKEN_RPAREN, "')'")) {
            free_insert_statement(&statement);
            return false;
        }

        if (!parser_next_token(parser)) {
            free_insert_statement(&statement);
            return false;
        }
    }

    if (!parser_expect_keyword(parser, "VALUES")) {
        free_insert_statement(&statement);
        return false;
    }

    if (!parser_expect_type(parser, TOKEN_LPAREN, "'('")) {
        free_insert_statement(&statement);
        return false;
    }

    if (!parser_next_token(parser)) {
        free_insert_statement(&statement);
        return false;
    }

    if (!parser_parse_value_list(parser, &statement.values, &statement.value_count)) {
        free_insert_statement(&statement);
        return false;
    }

    if (!parser_expect_type(parser, TOKEN_RPAREN, "')'")) {
        free_insert_statement(&statement);
        return false;
    }

    if (!parser_next_token(parser)) {
        free_insert_statement(&statement);
        return false;
    }

    if (statement.has_column_list && statement.column_count != statement.value_count) {
        free_insert_statement(&statement);
        parser_set_error(parser, "INSERT 컬럼 수와 값 수가 일치하지 않습니다.");
        return false;
    }

    out_statement->type = STATEMENT_INSERT;
    out_statement->as.insert_statement = statement;
    return parser_consume_statement_end(parser);
}

static bool parser_parse_select(Parser *parser, Statement *out_statement) {
    SelectStatement statement;

    memset(&statement, 0, sizeof(statement));

    if (!parser_expect_keyword(parser, "SELECT")) {
        return false;
    }

    if (parser->current.type == TOKEN_ASTERISK) {
        statement.select_all = true;
        if (!parser_next_token(parser)) {
            free_select_statement(&statement);
            return false;
        }
    } else {
        if (!parser_parse_identifier_list(parser, &statement.columns, &statement.column_count)) {
            free_select_statement(&statement);
            return false;
        }
    }

    if (!parser_expect_keyword(parser, "FROM")) {
        free_select_statement(&statement);
        return false;
    }

    if (!parser_parse_qualified_name(parser, &statement.source)) {
        free_select_statement(&statement);
        return false;
    }

    if (token_is_keyword(&parser->current, "WHERE")) {
        if (!parser_parse_where_clause(parser, &statement)) {
            free_select_statement(&statement);
            return false;
        }
    }

    out_statement->type = STATEMENT_SELECT;
    out_statement->as.select_statement = statement;
    return parser_consume_statement_end(parser);
}

static bool append_statement(StatementList *list, const Statement *statement, char *error_buffer, size_t error_buffer_size) {
    Statement *resized = (Statement *)realloc(list->items, sizeof(Statement) * (list->count + 1));

    if (resized == NULL) {
        sp_set_error(error_buffer, error_buffer_size, "메모리 할당에 실패했습니다.");
        return false;
    }

    resized[list->count] = *statement;
    list->items = resized;
    list->count++;
    return true;
}

bool parse_sql_script(const char *sql,
                      StatementList *out_list,
                      char *error_buffer,
                      size_t error_buffer_size) {
    Parser parser;
    StatementList list;

    memset(&parser, 0, sizeof(parser));
    memset(&list, 0, sizeof(list));

    parser.input = sql;
    parser.length = strlen(sql);
    parser.line = 1;
    parser.column = 1;
    parser.error_buffer = error_buffer;
    parser.error_buffer_size = error_buffer_size;

    if (error_buffer != NULL && error_buffer_size > 0) {
        error_buffer[0] = '\0';
    }

    if (!parser_next_token(&parser)) {
        free_statement_list(&list);
        free_token(&parser.current);
        return false;
    }

    while (parser.current.type != TOKEN_EOF) {
        Statement statement;
        memset(&statement, 0, sizeof(statement));

        if (token_is_keyword(&parser.current, "INSERT")) {
            if (!parser_parse_insert(&parser, &statement)) {
                free_statement_list(&list);
                free_token(&parser.current);
                return false;
            }
        } else if (token_is_keyword(&parser.current, "SELECT")) {
            if (!parser_parse_select(&parser, &statement)) {
                free_statement_list(&list);
                free_token(&parser.current);
                return false;
            }
        } else {
            parser_set_error(&parser,
                             "%zu:%zu INSERT 또는 SELECT 문을 기대했지만 '%s'를 발견했습니다.",
                             parser.current.line,
                             parser.current.column,
                             parser.current.lexeme);
            free_statement_list(&list);
            free_token(&parser.current);
            return false;
        }

        if (!append_statement(&list, &statement, error_buffer, error_buffer_size)) {
            if (statement.type == STATEMENT_INSERT) {
                free_insert_statement(&statement.as.insert_statement);
            } else if (statement.type == STATEMENT_SELECT) {
                free_select_statement(&statement.as.select_statement);
            }
            free_statement_list(&list);
            free_token(&parser.current);
            return false;
        }
    }

    free_token(&parser.current);
    *out_list = list;
    return true;
}

void free_statement_list(StatementList *list) {
    size_t index;

    if (list == NULL) {
        return;
    }

    for (index = 0; index < list->count; index++) {
        Statement *statement = &list->items[index];
        if (statement->type == STATEMENT_INSERT) {
            free_insert_statement(&statement->as.insert_statement);
        } else if (statement->type == STATEMENT_SELECT) {
            free_select_statement(&statement->as.select_statement);
        }
    }

    free(list->items);
    list->items = NULL;
    list->count = 0;
}
