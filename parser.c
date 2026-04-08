#include <ctype.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"

/* ?뚯꽌?먯꽌 ?ㅼ쓬 ?좏겙?쇰줈 ??移??욎쑝濡??대룞?⑸땲?? */
void advance_parser(Parser *p) {
    p->current_token = get_next_token(&p->lexer);
}

/* ?꾩옱 ?좏겙??湲곕???醫낅쪟?몄? 寃?ы븯怨? 留욎쑝硫???移?吏꾪뻾?⑸땲?? */
/* 援щЦ??留욎쑝硫?1, ?꾨땲硫?0??諛섑솚?⑸땲?? */
static int expect_token(Parser *p, TokenType type) {
    if (p->current_token.type == type) {
        advance_parser(p);
        return 1;
    }
    return 0;
}

/* WHERE col = value ?뺥깭瑜??쎌뼱 Statement??議곌굔 而щ읆/媛믪쓣 梨꾩썎?덈떎. */
static int parse_where_clause(Parser *p, Statement *stmt) {
    if (p->current_token.type != TOKEN_WHERE) return 1;
    advance_parser(p);

    if (p->current_token.type != TOKEN_IDENTIFIER) return 0;
    strncpy(stmt->where_col, p->current_token.text, sizeof(stmt->where_col) - 1);
    advance_parser(p);

    if (!expect_token(p, TOKEN_EQ)) return 0;

    if (p->current_token.type == TOKEN_STRING ||
        p->current_token.type == TOKEN_NUMBER ||
        p->current_token.type == TOKEN_IDENTIFIER) {
        strncpy(stmt->where_val, p->current_token.text, sizeof(stmt->where_val) - 1);
        advance_parser(p);
    } else {
        return 0;
    }

    return 1;
}

/* SELECT 臾몄쓣 ?뚯떛?⑸땲?? */
/* ?뺥깭: SELECT * FROM table [WHERE col = value] */
static int parse_select(Parser *p, Statement *stmt) {
    stmt->type = STMT_SELECT;
    advance_parser(p);

    if (!expect_token(p, TOKEN_STAR)) return 0;
    if (!expect_token(p, TOKEN_FROM)) return 0;

    if (p->current_token.type != TOKEN_IDENTIFIER) return 0;
    strncpy(stmt->table_name, p->current_token.text, sizeof(stmt->table_name) - 1);
    advance_parser(p);

    return parse_where_clause(p, stmt);
}

/* INSERT 臾몄쓣 ?뚯떛?⑸땲?? */
/* ?뺥깭: INSERT INTO table VALUES (...) */
static int parse_insert(Parser *p, Statement *stmt) {
    stmt->type = STMT_INSERT;
    advance_parser(p);
    const char *open_paren;
    const char *close_paren;
    int len;

    if (!expect_token(p, TOKEN_INTO)) return 0;

    if (p->current_token.type != TOKEN_IDENTIFIER) return 0;
    strncpy(stmt->table_name, p->current_token.text, sizeof(stmt->table_name) - 1);
    advance_parser(p);

    if (!expect_token(p, TOKEN_VALUES)) return 0;
    if (p->current_token.type != TOKEN_LPAREN) return 0;

    open_paren = strchr(p->lexer.sql, '(');
    close_paren = strrchr(p->lexer.sql, ')');
    if (!open_paren || !close_paren || close_paren <= open_paren) return 0;

    len = (int)(close_paren - open_paren - 1);
    if (len >= (int)sizeof(stmt->row_data)) len = (int)sizeof(stmt->row_data) - 1;
    strncpy(stmt->row_data, open_paren + 1, len);
    stmt->row_data[len] = '\0';

    return 1;
}

/* UPDATE 臾몄쓣 ?뚯떛?⑸땲?? */
/* ?뺥깭: UPDATE table SET col = value [WHERE col = value] */
static int parse_update(Parser *p, Statement *stmt) {
    stmt->type = STMT_UPDATE;
    advance_parser(p);

    if (p->current_token.type != TOKEN_IDENTIFIER) return 0;
    strncpy(stmt->table_name, p->current_token.text, sizeof(stmt->table_name) - 1);
    advance_parser(p);

    if (!expect_token(p, TOKEN_SET)) return 0;

    if (p->current_token.type != TOKEN_IDENTIFIER) return 0;
    strncpy(stmt->set_col, p->current_token.text, sizeof(stmt->set_col) - 1);
    advance_parser(p);

    if (!expect_token(p, TOKEN_EQ)) return 0;

    if (p->current_token.type == TOKEN_STRING ||
        p->current_token.type == TOKEN_NUMBER ||
        p->current_token.type == TOKEN_IDENTIFIER) {
        strncpy(stmt->set_val, p->current_token.text, sizeof(stmt->set_val) - 1);
        advance_parser(p);
    } else return 0;

    return parse_where_clause(p, stmt);
}

/* DELETE 臾몄쓣 ?뚯떛?⑸땲?? */
/* ?뺥깭: DELETE FROM table [WHERE col = value] */
static int parse_delete(Parser *p, Statement *stmt) {
    stmt->type = STMT_DELETE;
    advance_parser(p);

    if (!expect_token(p, TOKEN_FROM)) return 0;

    if (p->current_token.type != TOKEN_IDENTIFIER) return 0;
    strncpy(stmt->table_name, p->current_token.text, sizeof(stmt->table_name) - 1);
    advance_parser(p);

    return parse_where_clause(p, stmt);
}

/* SQL ??以꾩쓣 ?꾩껜 ?먮퀎?섍퀬, ?대떦 ?뚯꽌濡?遺꾧린 泥섎━?⑸땲?? */
/* ?깃났 ??1, ?ㅽ뙣 ??0??諛섑솚?⑸땲?? */
int parse_statement(const char *sql, Statement *stmt) {
    memset(stmt, 0, sizeof(Statement));

    Parser p;
    init_lexer(&p.lexer, sql);
    advance_parser(&p);

    switch (p.current_token.type) {
        case TOKEN_SELECT: return parse_select(&p, stmt);
        case TOKEN_INSERT: return parse_insert(&p, stmt);
        case TOKEN_UPDATE: return parse_update(&p, stmt);
        case TOKEN_DELETE: return parse_delete(&p, stmt);
        default:
            stmt->type = STMT_UNRECOGNIZED;
            return 0;
    }
}
