#include "sqlproc/lexer.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* lexer가 현재 어느 위치를 읽고 있는지 추적하는 상태입니다. */
typedef struct {
    const char *input;
    size_t position;
    int line;
    int column;
} LexerState;

/* 원문 일부를 복사해 토큰 lexeme으로 저장합니다. */
static char *dup_slice(const char *start, size_t length, SqlError *err) {
    char *text = (char *)malloc(length + 1U);

    if (text == NULL) {
        sql_error_set(err, 0, 0, 0, "out of memory");
        return NULL;
    }

    memcpy(text, start, length);
    text[length] = '\0';
    return text;
}

/* 키워드 비교에 쓰는 대소문자 무시 문자열 비교 함수입니다. */
static int equals_ignore_case(const char *left, const char *right) {
    size_t index = 0U;

    while (left[index] != '\0' && right[index] != '\0') {
        if (tolower((unsigned char)left[index]) != tolower((unsigned char)right[index])) {
            return 0;
        }
        index++;
    }

    return left[index] == '\0' && right[index] == '\0';
}

/* 식별자 문자열이 예약어인지 일반 이름인지 판별합니다. */
static TokenType keyword_type(const char *lexeme) {
    if (equals_ignore_case(lexeme, "INSERT")) {
        return TOKEN_INSERT;
    }
    if (equals_ignore_case(lexeme, "INTO")) {
        return TOKEN_INTO;
    }
    if (equals_ignore_case(lexeme, "VALUES")) {
        return TOKEN_VALUES;
    }
    if (equals_ignore_case(lexeme, "SELECT")) {
        return TOKEN_SELECT;
    }
    if (equals_ignore_case(lexeme, "FROM")) {
        return TOKEN_FROM;
    }
    if (equals_ignore_case(lexeme, "WHERE")) {
        return TOKEN_WHERE;
    }
    if (equals_ignore_case(lexeme, "TRUE")) {
        return TOKEN_TRUE;
    }
    if (equals_ignore_case(lexeme, "FALSE")) {
        return TOKEN_FALSE;
    }
    return TOKEN_IDENTIFIER;
}

/* 문자 하나를 소비하면서 줄/열 위치도 함께 갱신합니다. */
static void lexer_advance(LexerState *state) {
    if (state->input[state->position] == '\n') {
        state->line++;
        state->column = 1;
    } else {
        state->column++;
    }
    state->position++;
}

/* 동적 토큰 배열 끝에 새 토큰 하나를 추가합니다. */
static SqlStatus push_token(TokenArray *tokens, TokenType type, char *lexeme, int line, int column, SqlError *err) {
    Token *items = (Token *)realloc(tokens->items, sizeof(Token) * (tokens->count + 1U));

    if (items == NULL) {
        free(lexeme);
        sql_error_set(err, line, column, 0, "out of memory");
        return SQL_STATUS_OOM;
    }

    tokens->items = items;
    tokens->items[tokens->count].type = type;
    tokens->items[tokens->count].lexeme = lexeme;
    tokens->items[tokens->count].line = line;
    tokens->items[tokens->count].column = column;
    tokens->count++;
    return SQL_STATUS_OK;
}

/* 식별자 또는 키워드를 읽어 토큰으로 만듭니다. */
static SqlStatus lex_identifier_or_keyword(LexerState *state, TokenArray *tokens, SqlError *err) {
    size_t start = state->position;
    int line = state->line;
    int column = state->column;
    char *lexeme;

    while (isalnum((unsigned char)state->input[state->position]) || state->input[state->position] == '_') {
        lexer_advance(state);
    }

    lexeme = dup_slice(state->input + start, state->position - start, err);
    if (lexeme == NULL) {
        return SQL_STATUS_OOM;
    }

    return push_token(tokens, keyword_type(lexeme), lexeme, line, column, err);
}

/* 정수 리터럴을 읽어 NUMBER 토큰으로 만듭니다. */
static SqlStatus lex_number(LexerState *state, TokenArray *tokens, SqlError *err) {
    size_t start = state->position;
    int line = state->line;
    int column = state->column;
    char *lexeme;

    if (state->input[state->position] == '-') {
        lexer_advance(state);
    }
    while (isdigit((unsigned char)state->input[state->position])) {
        lexer_advance(state);
    }

    lexeme = dup_slice(state->input + start, state->position - start, err);
    if (lexeme == NULL) {
        return SQL_STATUS_OOM;
    }

    return push_token(tokens, TOKEN_NUMBER, lexeme, line, column, err);
}

/* 따옴표 문자열을 읽고 지원하는 escape를 실제 문자로 복원합니다. */
static SqlStatus lex_string(LexerState *state, TokenArray *tokens, SqlError *err) {
    size_t capacity = 16U;
    size_t length = 0U;
    char *buffer = (char *)malloc(capacity);
    int line = state->line;
    int column = state->column;

    if (buffer == NULL) {
        sql_error_set(err, line, column, 0, "out of memory");
        return SQL_STATUS_OOM;
    }

    lexer_advance(state);
    while (state->input[state->position] != '\0' && state->input[state->position] != '\'') {
        char current = state->input[state->position];

        if (current == '\\') {
            lexer_advance(state);
            current = state->input[state->position];
            if (current == 'n') {
                current = '\n';
            } else if (current == 't') {
                current = '\t';
            } else if (current == '\\') {
                current = '\\';
            } else if (current == '\'') {
                current = '\'';
            } else {
                free(buffer);
                sql_error_set(err, state->line, state->column, 0, "unsupported string escape sequence");
                return SQL_STATUS_ERROR;
            }
        } else if (current == '\n') {
            free(buffer);
            sql_error_set(err, line, column, 0, "unterminated string literal");
            return SQL_STATUS_ERROR;
        }

        if (length + 2U > capacity) {
            char *grown;
            capacity *= 2U;
            grown = (char *)realloc(buffer, capacity);
            if (grown == NULL) {
                free(buffer);
                sql_error_set(err, line, column, 0, "out of memory");
                return SQL_STATUS_OOM;
            }
            buffer = grown;
        }

        buffer[length++] = current;
        lexer_advance(state);
    }

    if (state->input[state->position] != '\'') {
        free(buffer);
        sql_error_set(err, line, column, 0, "unterminated string literal");
        return SQL_STATUS_ERROR;
    }

    buffer[length] = '\0';
    lexer_advance(state);
    return push_token(tokens, TOKEN_STRING, buffer, line, column, err);
}

/* 🧭 SQL 원문을 처음부터 끝까지 훑어 TokenArray로 변환합니다. */
SqlStatus lexer_tokenize(const char *sql, TokenArray *out_tokens, SqlError *err) {
    LexerState state;
    SqlStatus status;

    if (sql == NULL || out_tokens == NULL) {
        sql_error_set(err, 0, 0, 0, "lexer_tokenize received null pointer");
        return SQL_STATUS_ERROR;
    }

    out_tokens->count = 0U;
    out_tokens->items = NULL;
    state.input = sql;
    state.position = 0U;
    state.line = 1;
    state.column = 1;

    while (state.input[state.position] != '\0') {
        char current = state.input[state.position];
        int line = state.line;
        int column = state.column;
        char *lexeme;

        if (isspace((unsigned char)current)) {
            lexer_advance(&state);
            continue;
        }

        if (isalpha((unsigned char)current) || current == '_') {
            status = lex_identifier_or_keyword(&state, out_tokens, err);
            if (status != SQL_STATUS_OK) {
                token_array_free(out_tokens);
                return status;
            }
            continue;
        }

        if (isdigit((unsigned char)current) || (current == '-' && isdigit((unsigned char)state.input[state.position + 1U]))) {
            status = lex_number(&state, out_tokens, err);
            if (status != SQL_STATUS_OK) {
                token_array_free(out_tokens);
                return status;
            }
            continue;
        }

        if (current == '\'') {
            status = lex_string(&state, out_tokens, err);
            if (status != SQL_STATUS_OK) {
                token_array_free(out_tokens);
                return status;
            }
            continue;
        }

        lexeme = dup_slice(&state.input[state.position], 1U, err);
        if (lexeme == NULL) {
            token_array_free(out_tokens);
            return SQL_STATUS_OOM;
        }

        switch (current) {
            case ',':
                status = push_token(out_tokens, TOKEN_COMMA, lexeme, line, column, err);
                lexer_advance(&state);
                break;
            case '*':
                status = push_token(out_tokens, TOKEN_STAR, lexeme, line, column, err);
                lexer_advance(&state);
                break;
            case '(':
                status = push_token(out_tokens, TOKEN_LPAREN, lexeme, line, column, err);
                lexer_advance(&state);
                break;
            case ')':
                status = push_token(out_tokens, TOKEN_RPAREN, lexeme, line, column, err);
                lexer_advance(&state);
                break;
            case ';':
                status = push_token(out_tokens, TOKEN_SEMICOLON, lexeme, line, column, err);
                lexer_advance(&state);
                break;
            case '=':
                status = push_token(out_tokens, TOKEN_EQ, lexeme, line, column, err);
                lexer_advance(&state);
                break;
            default:
                free(lexeme);
                token_array_free(out_tokens);
                sql_error_set(err, line, column, 0, "unexpected character '%c'", current);
                return SQL_STATUS_ERROR;
        }

        if (status != SQL_STATUS_OK) {
            token_array_free(out_tokens);
            return status;
        }
    }

    status = push_token(out_tokens, TOKEN_EOF, dup_slice("", 0U, err), state.line, state.column, err);
    if (status != SQL_STATUS_OK) {
        token_array_free(out_tokens);
        return status;
    }

    return SQL_STATUS_OK;
}

/* 토큰 배열 안에 저장된 lexeme 메모리를 모두 정리합니다. */
void token_array_free(TokenArray *tokens) {
    size_t index;

    if (tokens == NULL) {
        return;
    }

    for (index = 0U; index < tokens->count; index++) {
        free(tokens->items[index].lexeme);
        tokens->items[index].lexeme = NULL;
    }

    free(tokens->items);
    tokens->items = NULL;
    tokens->count = 0U;
}

/* trace나 오류 메시지에서 토큰 타입 이름을 보여주기 위한 함수입니다. */
const char *token_type_name(TokenType type) {
    switch (type) {
        case TOKEN_EOF:
            return "EOF";
        case TOKEN_IDENTIFIER:
            return "IDENTIFIER";
        case TOKEN_NUMBER:
            return "NUMBER";
        case TOKEN_STRING:
            return "STRING";
        case TOKEN_TRUE:
            return "TRUE";
        case TOKEN_FALSE:
            return "FALSE";
        case TOKEN_INSERT:
            return "INSERT";
        case TOKEN_INTO:
            return "INTO";
        case TOKEN_VALUES:
            return "VALUES";
        case TOKEN_SELECT:
            return "SELECT";
        case TOKEN_FROM:
            return "FROM";
        case TOKEN_WHERE:
            return "WHERE";
        case TOKEN_COMMA:
            return "COMMA";
        case TOKEN_STAR:
            return "STAR";
        case TOKEN_LPAREN:
            return "LPAREN";
        case TOKEN_RPAREN:
            return "RPAREN";
        case TOKEN_SEMICOLON:
            return "SEMICOLON";
        case TOKEN_EQ:
            return "EQ";
        default:
            return "UNKNOWN";
    }
}
