#ifndef SQLPROC_LEXER_H
#define SQLPROC_LEXER_H

#include <stddef.h>

#include "sqlproc/diag.h"

/* lexer가 만들어내는 토큰 종류입니다. */
typedef enum {
    TOKEN_EOF = 0,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_INSERT,
    TOKEN_INTO,
    TOKEN_VALUES,
    TOKEN_SELECT,
    TOKEN_FROM,
    TOKEN_WHERE,
    TOKEN_COMMA,
    TOKEN_STAR,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_SEMICOLON,
    TOKEN_EQ
} TokenType;

/* SQL 원문에서 잘라낸 한 개의 토큰입니다.
 * lexeme은 토큰의 실제 문자열, line/column은 시작 위치입니다.
 */
typedef struct {
    TokenType type;
    char *lexeme;
    int line;
    int column;
} Token;

/* 토큰 배열 전체를 담는 동적 컨테이너입니다. */
typedef struct {
    size_t count;
    Token *items;
} TokenArray;

/* SQL 문자열 전체를 토큰 배열로 변환합니다. */
SqlStatus lexer_tokenize(const char *sql, TokenArray *out_tokens, SqlError *err);

/* TokenArray 안의 lexeme과 배열 메모리를 해제합니다. */
void token_array_free(TokenArray *tokens);

/* TokenType을 사람이 읽기 쉬운 이름으로 바꿉니다. */
const char *token_type_name(TokenType type);

#endif
