#include <ctype.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"

// 파서에서 다음 토큰으로 한 칸 앞으로 이동합니다.
void advance_parser(Parser *p) {
    p->current_token = get_next_token(&p->lexer);
}

// 현재 토큰이 기대한 종류인지 검사하고, 맞으면 한 칸 진행합니다.
// 구문이 맞으면 1, 아니면 0을 반환합니다.
static int expect_token(Parser *p, TokenType type) {
    if (p->current_token.type == type) {
        advance_parser(p);
        return 1;
    }
    return 0;
}

// WHERE col = value 형태를 읽어 Statement에 조건 컬럼/값을 채웁니다.
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

// SELECT 문을 파싱합니다.
// 형태: SELECT * FROM table [WHERE col = value]
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

// INSERT 문을 파싱합니다.
// 형태: INSERT INTO table VALUES (...)
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

// UPDATE 문을 파싱합니다.
// 형태: UPDATE table SET col = value [WHERE col = value]
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

// DELETE 문을 파싱합니다.
// 형태: DELETE FROM table [WHERE col = value]
static int parse_delete(Parser *p, Statement *stmt) {
    stmt->type = STMT_DELETE;
    advance_parser(p);

    if (!expect_token(p, TOKEN_FROM)) return 0;

    if (p->current_token.type != TOKEN_IDENTIFIER) return 0;
    strncpy(stmt->table_name, p->current_token.text, sizeof(stmt->table_name) - 1);
    advance_parser(p);

    return parse_where_clause(p, stmt);
}

// SQL 한 줄을 전체 판별하고, 해당 파서로 분기 처리합니다.
// 성공 시 1, 실패 시 0을 반환합니다.
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
