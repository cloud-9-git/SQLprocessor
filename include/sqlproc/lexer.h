#ifndef SQLPROC_LEXER_H
#define SQLPROC_LEXER_H

#include <stddef.h>

#include "sqlproc/diag.h"

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

typedef struct {
    TokenType type;
    char *lexeme;
    int line;
    int column;
} Token;

typedef struct {
    size_t count;
    Token *items;
} TokenArray;

SqlStatus lexer_tokenize(const char *sql, TokenArray *out_tokens, SqlError *err);
void token_array_free(TokenArray *tokens);
const char *token_type_name(TokenType type);

#endif
