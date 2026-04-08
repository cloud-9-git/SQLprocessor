#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_RECORDS 25000
#define MAX_COLS 15
#define MAX_TABLES 10
#define MAX_UKS 5
#define MAX_SQL_LEN 4096

// ==========================================
// [1] 자료구조
// ==========================================
typedef enum { 
    STMT_INSERT, 
    STMT_SELECT, 
    STMT_DELETE, 
    STMT_UPDATE, 
    STMT_UNRECOGNIZED 
} StatementType;

typedef enum { 
    COL_NORMAL, 
    COL_PK, 
    COL_UK, 
    COL_NN 
} ColumnType;

typedef struct {
    StatementType type;
    char table_name[256];
    char row_data[1024];
    char set_col[50];
    char set_val[256];
    char where_col[50];
    char where_val[256];
} Statement;

typedef struct {
    char name[50];
    ColumnType type;
} ColumnInfo;

typedef struct {
    char table_name[256];
    FILE *file;
    ColumnInfo cols[MAX_COLS];
    int col_count;
    int pk_idx;
    int uk_indices[MAX_UKS];
    int uk_count;
    long pk_index[MAX_RECORDS];
    char records[MAX_RECORDS][1024]; 
    int record_count;
} TableCache;

TableCache open_tables[MAX_TABLES];
int open_table_count = 0;


// ==========================================
// [2] Lexer & Parser (어휘/구문 분석기)
// ==========================================
typedef enum {
    TOKEN_EOF, TOKEN_ILLEGAL, TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,
    TOKEN_SELECT, TOKEN_INSERT, TOKEN_UPDATE, TOKEN_DELETE, TOKEN_FROM,
    TOKEN_WHERE, TOKEN_SET, TOKEN_INTO, TOKEN_VALUES,
    TOKEN_STAR, TOKEN_COMMA, TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_EQ, TOKEN_SEMICOLON
} TokenType;

typedef struct {
    TokenType type;
    char text[256];
} Token;

typedef struct {
    const char *sql;
    int pos;
} Lexer;

void init_lexer(Lexer *l, const char *sql) {
    l->sql = sql;
    l->pos = 0;
}

Token get_next_token(Lexer *l) {
    Token t = { .type = TOKEN_ILLEGAL, .text = "" };

    // 공백 건너뛰기
    while (l->sql[l->pos] && isspace((unsigned char)l->sql[l->pos])) {
        l->pos++;
    }

    if (l->sql[l->pos] == '\0') {
        t.type = TOKEN_EOF;
        return t;
    }

    char c = l->sql[l->pos];
    int start_pos = l->pos;

    // 심볼(기호) 토큰화
    if (strchr("*,=();", c)) {
        switch(c) {
            case '*': t.type = TOKEN_STAR; break;
            case ',': t.type = TOKEN_COMMA; break;
            case '=': t.type = TOKEN_EQ; break;
            case '(': t.type = TOKEN_LPAREN; break;
            case ')': t.type = TOKEN_RPAREN; break;
            case ';': t.type = TOKEN_SEMICOLON; break;
        }
        t.text[0] = c; 
        t.text[1] = '\0';
        l->pos++;
    } 
    // 문자열 리터럴 토큰화 ('value')
    else if (c == '\'') {
        l->pos++; 
        start_pos = l->pos;
        while (l->sql[l->pos] && l->sql[l->pos] != '\'') {
            l->pos++;
        }
        int len = l->pos - start_pos;
        if (len >= sizeof(t.text)) len = sizeof(t.text) - 1;
        strncpy(t.text, l->sql + start_pos, len);
        t.text[len] = '\0';
        
        if (l->sql[l->pos] == '\'') l->pos++; 
        t.type = TOKEN_STRING;
    }
    // [해결 1] 식별자 또는 키워드 토큰화 (하이픈, 마침표 허용)
    else if (isalpha((unsigned char)c) || c == '_') {
        // 기존 isalnum 뿐만 아니라 '_', '-', '.' 도 한 덩어리로 묶습니다.
        while (l->sql[l->pos] && (isalnum((unsigned char)l->sql[l->pos]) || strchr("_-.", l->sql[l->pos]))) {
            l->pos++;
        }
        int len = l->pos - start_pos;
        if (len >= sizeof(t.text)) len = sizeof(t.text) - 1;
        strncpy(t.text, l->sql + start_pos, len);
        t.text[len] = '\0';
        
        char upper_text[256];
        for(int i = 0; t.text[i]; i++) {
            upper_text[i] = toupper((unsigned char)t.text[i]);
        }
        upper_text[strlen(t.text)] = '\0';

        if (strcmp(upper_text, "SELECT") == 0) t.type = TOKEN_SELECT;
        else if (strcmp(upper_text, "INSERT") == 0) t.type = TOKEN_INSERT;
        else if (strcmp(upper_text, "UPDATE") == 0) t.type = TOKEN_UPDATE;
        else if (strcmp(upper_text, "DELETE") == 0) t.type = TOKEN_DELETE;
        else if (strcmp(upper_text, "FROM") == 0) t.type = TOKEN_FROM;
        else if (strcmp(upper_text, "WHERE") == 0) t.type = TOKEN_WHERE;
        else if (strcmp(upper_text, "SET") == 0) t.type = TOKEN_SET;
        else if (strcmp(upper_text, "INTO") == 0) t.type = TOKEN_INTO;
        else if (strcmp(upper_text, "VALUES") == 0) t.type = TOKEN_VALUES;
        else t.type = TOKEN_IDENTIFIER;
    }
    // [해결 1] 숫자 기반 토큰화 (전화번호 010-3000 등 허용)
    else if (isdigit((unsigned char)c)) {
        while (l->sql[l->pos] && (isalnum((unsigned char)l->sql[l->pos]) || strchr("_-.", l->sql[l->pos]))) {
            l->pos++;
        }
        int len = l->pos - start_pos;
        if (len >= sizeof(t.text)) len = sizeof(t.text) - 1;
        strncpy(t.text, l->sql + start_pos, len);
        t.text[len] = '\0';
        t.type = TOKEN_NUMBER;
    }
    return t;
}

typedef struct {
    Lexer lexer;
    Token current_token;
} Parser;

void advance_parser(Parser *p) {
    p->current_token = get_next_token(&p->lexer);
}

int expect_token(Parser *p, TokenType type) {
    if (p->current_token.type == type) {
        advance_parser(p);
        return 1;
    }
    return 0;
}

int parse_where_clause(Parser *p, Statement *stmt) {
    if (p->current_token.type != TOKEN_WHERE) return 1; 
    advance_parser(p); 

    if (p->current_token.type != TOKEN_IDENTIFIER) return 0;
    strncpy(stmt->where_col, p->current_token.text, sizeof(stmt->where_col)-1);
    advance_parser(p);

    if (!expect_token(p, TOKEN_EQ)) return 0;

    // [해결 2] 식별자(TOKEN_IDENTIFIER)도 값으로 허용 (따옴표 없는 문자열)
    if (p->current_token.type == TOKEN_STRING || 
        p->current_token.type == TOKEN_NUMBER || 
        p->current_token.type == TOKEN_IDENTIFIER) {
        strncpy(stmt->where_val, p->current_token.text, sizeof(stmt->where_val)-1);
        advance_parser(p);
    } else return 0;
    
    return 1;
}

int parse_select(Parser *p, Statement *stmt) {
    stmt->type = STMT_SELECT;
    advance_parser(p); 
    
    if (!expect_token(p, TOKEN_STAR)) return 0;
    if (!expect_token(p, TOKEN_FROM)) return 0;

    if (p->current_token.type != TOKEN_IDENTIFIER) return 0;
    strncpy(stmt->table_name, p->current_token.text, sizeof(stmt->table_name)-1);
    advance_parser(p);

    return parse_where_clause(p, stmt);
}

int parse_insert(Parser *p, Statement *stmt) {
    stmt->type = STMT_INSERT;
    advance_parser(p); 
    const char *open_paren;
    const char *close_paren;
    int len;

    if (!expect_token(p, TOKEN_INTO)) return 0;
    
    if (p->current_token.type != TOKEN_IDENTIFIER) return 0;
    strncpy(stmt->table_name, p->current_token.text, sizeof(stmt->table_name)-1);
    advance_parser(p);

    if (!expect_token(p, TOKEN_VALUES)) return 0;
    if (p->current_token.type != TOKEN_LPAREN) return 0;

    open_paren = strchr(p->lexer.sql, '(');
    close_paren = strrchr(p->lexer.sql, ')');
    if (!open_paren || !close_paren || close_paren <= open_paren) return 0;

    len = (int)(close_paren - open_paren - 1);
    if (len >= sizeof(stmt->row_data)) len = sizeof(stmt->row_data) - 1;
    strncpy(stmt->row_data, open_paren + 1, len);
    stmt->row_data[len] = '\0';

    return 1;
}

int parse_update(Parser *p, Statement *stmt) {
    stmt->type = STMT_UPDATE;
    advance_parser(p); 
    
    if (p->current_token.type != TOKEN_IDENTIFIER) return 0;
    strncpy(stmt->table_name, p->current_token.text, sizeof(stmt->table_name)-1);
    advance_parser(p);

    if (!expect_token(p, TOKEN_SET)) return 0;
    
    if (p->current_token.type != TOKEN_IDENTIFIER) return 0;
    strncpy(stmt->set_col, p->current_token.text, sizeof(stmt->set_col)-1);
    advance_parser(p);

    if (!expect_token(p, TOKEN_EQ)) return 0;

    // [해결 2] 식별자(TOKEN_IDENTIFIER)도 값으로 허용
    if (p->current_token.type == TOKEN_STRING || 
        p->current_token.type == TOKEN_NUMBER || 
        p->current_token.type == TOKEN_IDENTIFIER) {
        strncpy(stmt->set_val, p->current_token.text, sizeof(stmt->set_val)-1);
        advance_parser(p);
    } else return 0;
    
    return parse_where_clause(p, stmt);
}

int parse_delete(Parser *p, Statement *stmt) {
    stmt->type = STMT_DELETE;
    advance_parser(p); 

    if (!expect_token(p, TOKEN_FROM)) return 0;

    if (p->current_token.type != TOKEN_IDENTIFIER) return 0;
    strncpy(stmt->table_name, p->current_token.text, sizeof(stmt->table_name)-1);
    advance_parser(p);

    return parse_where_clause(p, stmt);
}

int parse_statement(const char *sql, Statement *stmt) {
    memset(stmt, 0, sizeof(Statement));
    
    Parser p;
    init_lexer(&p.lexer, sql);
    advance_parser(&p); 

    switch(p.current_token.type) {
        case TOKEN_SELECT: return parse_select(&p, stmt);
        case TOKEN_INSERT: return parse_insert(&p, stmt);
        case TOKEN_UPDATE: return parse_update(&p, stmt);
        case TOKEN_DELETE: return parse_delete(&p, stmt);
        default:
            stmt->type = STMT_UNRECOGNIZED;
            return 0;
    }
}


// ==========================================
// [3] 유틸리티 함수
// ==========================================
int compare_long(const void *a, const void *b) { 
    long val_a = *(long*)a;
    long val_b = *(long*)b;
    return (val_a > val_b) - (val_a < val_b); 
}

int find_in_pk_index(TableCache *tc, long val) {
    if (tc->record_count == 0 || tc->pk_idx == -1) return -1;
    long *found = bsearch(&val, tc->pk_index, tc->record_count, sizeof(long), compare_long);
    return found ? (int)(found - tc->pk_index) : -1;
}

void trim_and_unquote(char *str) {
    if (!str) return;
    
    char *start = str;
    while (isspace((unsigned char)*start)) start++;
    
    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    
    if (start[0] == '\'' && end > start && *end == '\'') {
        start++;
        *end = '\0';
    }
    
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

int compare_value(const char *field, const char *search_val) {
    char f_buf[256]; 
    char v_buf[256]; 
    
    strncpy(f_buf, field ? field : "", 255); 
    f_buf[255] = '\0';
    
    strncpy(v_buf, search_val ? search_val : "", 255); 
    v_buf[255] = '\0';
    
    trim_and_unquote(f_buf);
    trim_and_unquote(v_buf);
    
    return strcmp(f_buf, v_buf) == 0;
}

void parse_csv_row(const char *row, char *fields[MAX_COLS], char *buffer) {
    strncpy(buffer, row, 1023); 
    buffer[1023] = '\0';
    
    int i = 0;
    int in_quotes = 0; 
    char *p = buffer; 
    
    fields[i++] = p;
    
    while (*p && i < MAX_COLS) {
        if (*p == '\'') in_quotes = !in_quotes;
        
        if (*p == ',' && !in_quotes) { 
            *p = '\0'; 
            fields[i++] = p + 1; 
        }
        p++;
    }
}

int get_col_idx(TableCache *tc, const char *col_name) {
    if (!col_name || strlen(col_name) == 0) return -1;
    for (int i = 0; i < tc->col_count; i++) {
        if (strcmp(tc->cols[i].name, col_name) == 0) return i;
    }
    return -1;
}

void rewrite_file(TableCache *tc) {
    if (tc->file) fclose(tc->file);
    
    char filename[300]; 
    snprintf(filename, sizeof(filename), "%s.csv", tc->table_name);
    
    tc->file = fopen(filename, "w+");
    if (!tc->file) return;
    
    // [해결 3] 헤더 쓸 때 컬럼 제약조건(PK, UK, NN)을 원래대로 보존
    for (int i = 0; i < tc->col_count; i++) {
        fprintf(tc->file, "%s", tc->cols[i].name);
        
        // 구조체의 컬럼 타입을 보고 문자열 복구
        if (tc->cols[i].type == COL_PK) {
            fprintf(tc->file, "(PK)");
        } else if (tc->cols[i].type == COL_UK) {
            fprintf(tc->file, "(UK)");
        } else if (tc->cols[i].type == COL_NN) {
            fprintf(tc->file, "(NN)");
        }
        
        fprintf(tc->file, "%s", (i == tc->col_count - 1 ? "\n" : ","));
    }
    
    // 데이터 행 쓰기
    for (int i = 0; i < tc->record_count; i++) {
        fprintf(tc->file, "%s\n", tc->records[i]);
    }
    
    fflush(tc->file);
}

void insert_pk_sorted(TableCache *tc, long val, const char* row_str) {
    if (tc->pk_idx == -1) {
        strncpy(tc->records[tc->record_count], row_str, 1023); 
        tc->records[tc->record_count][1023] = '\0';
        return;
    }
    
    int i = tc->record_count - 1;
    while (i >= 0 && tc->pk_index[i] > val) {
        tc->pk_index[i + 1] = tc->pk_index[i];
        strncpy(tc->records[i + 1], tc->records[i], 1023); 
        tc->records[i + 1][1023] = '\0';
        i--;
    }
    
    tc->pk_index[i + 1] = val;
    strncpy(tc->records[i + 1], row_str, 1023); 
    tc->records[i + 1][1023] = '\0';
}


// ==========================================
// [4] 실행 엔진
// ==========================================
TableCache* get_table(const char* name) {
    for (int i = 0; i < open_table_count; i++) {
        if (strcmp(open_tables[i].table_name, name) == 0) return &open_tables[i];
    }
    if (open_table_count >= MAX_TABLES) return NULL;

    char filename[300]; 
    snprintf(filename, sizeof(filename), "%s.csv", name);
    
    FILE *f = fopen(filename, "r+");
    if (!f) { 
        printf("[오류] '%s.csv' 테이블 파일이 없습니다.\n", name); 
        return NULL; 
    }
    
    TableCache *tc = &open_tables[open_table_count++];
    strncpy(tc->table_name, name, 255); 
    tc->table_name[255] = '\0';
    tc->file = f; 
    tc->record_count = 0; 
    tc->pk_idx = -1; 
    tc->col_count = 0; 
    tc->uk_count = 0;
    
    char header[1024];
    if (fgets(header, sizeof(header), f)) {
        char *token = strtok(header, ",\n\r"); 
        int idx = 0;
        
        while (token && idx < MAX_COLS) {
            char *paren = strchr(token, '(');
            if (paren) {
                int len = paren - token;
                if (len >= sizeof(tc->cols[idx].name)) len = sizeof(tc->cols[idx].name) - 1;
                strncpy(tc->cols[idx].name, token, len); 
                tc->cols[idx].name[len] = '\0';
                
                if (strstr(paren, "(PK)")) { 
                    tc->cols[idx].type = COL_PK; 
                    tc->pk_idx = idx; 
                } else if (strstr(paren, "(UK)")) { 
                    tc->cols[idx].type = COL_UK; 
                    if (tc->uk_count < MAX_UKS) tc->uk_indices[tc->uk_count++] = idx; 
                } else if (strstr(paren, "(NN)")) {
                    tc->cols[idx].type = COL_NN;
                } else {
                    tc->cols[idx].type = COL_NORMAL;
                }
            } else {
                strncpy(tc->cols[idx].name, token, sizeof(tc->cols[idx].name) - 1); 
                tc->cols[idx].name[sizeof(tc->cols[idx].name) - 1] = '\0';
                tc->cols[idx].type = COL_NORMAL;
            }
            token = strtok(NULL, ",\n\r"); 
            idx++;
        }
        tc->col_count = idx;
    }
    
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        char buffer[1024]; 
        char *fields[MAX_COLS] = {0}; 
        parse_csv_row(line, fields, buffer);
        
        long cp = (tc->pk_idx != -1 && fields[tc->pk_idx]) ? atol(fields[tc->pk_idx]) : 0;
        char *nl = strpbrk(line, "\r\n"); 
        if (nl) *nl = '\0';
        
        insert_pk_sorted(tc, cp, line); 
        tc->record_count++;
    }
    return tc;
}

void execute_insert(Statement *stmt) {
    TableCache *tc = get_table(stmt->table_name); 
    if (!tc) return;
    
    char buffer[1024]; 
    char *vals[MAX_COLS] = {0};
    parse_csv_row(stmt->row_data, vals, buffer);
    
    int val_count = 0;
    while(vals[val_count] && val_count < MAX_COLS) val_count++;

    long new_id = 0;
    
    for (int i = 0; i < tc->col_count; i++) {
        char *val = (i < val_count && vals[i]) ? vals[i] : "";
        char normalized_val[256];
        strncpy(normalized_val, val, sizeof(normalized_val) - 1);
        normalized_val[sizeof(normalized_val) - 1] = '\0';
        trim_and_unquote(normalized_val); 
        
        if (tc->cols[i].type == COL_NN && strlen(normalized_val) == 0) {
            printf("[실패] INSERT 거부: '%s' (NN 제약).\n", tc->cols[i].name); 
            return;
        }
        
        if (i == tc->pk_idx && strlen(normalized_val) > 0) {
            new_id = atol(normalized_val);
            if (find_in_pk_index(tc, new_id) != -1) { 
                printf("[실패] PK 중복: %ld\n", new_id); 
                return; 
            }
        }
        
        if (tc->cols[i].type == COL_UK && strlen(normalized_val) > 0) {
            for (int r = 0; r < tc->record_count; r++) {
                char row_buf[1024]; 
                char *f[MAX_COLS] = {0}; 
                parse_csv_row(tc->records[r], f, row_buf);
                
                if (compare_value(f[i], normalized_val)) { 
                    printf("[실패] INSERT 거부: '%s' 중복 (UK 제약).\n", normalized_val); 
                    return; 
                }
            }
        }
    }

    char new_line[1024] = ""; 
    size_t offset = 0;
    
    for (int i = 0; i < tc->col_count; i++) {
        const char *v = (i < val_count && vals[i]) ? vals[i] : ((tc->cols[i].type == COL_NN) ? "" : "NULL");
        char normalized_storage_val[1024];
        char formatted_val[1024];

        strncpy(normalized_storage_val, v, sizeof(normalized_storage_val) - 1);
        normalized_storage_val[sizeof(normalized_storage_val) - 1] = '\0';
        trim_and_unquote(normalized_storage_val);

        if (strchr(normalized_storage_val, ',')) {
            snprintf(formatted_val, sizeof(formatted_val), "'%s'", normalized_storage_val);
            v = formatted_val;
        } else {
            v = normalized_storage_val;
        }

        int w = snprintf(new_line + offset, sizeof(new_line) - offset, "%s%s", v, (i < tc->col_count - 1) ? "," : "");
        if (w < 0 || (size_t)w >= sizeof(new_line) - offset) break; 
        
        offset += w;
    }
    
    insert_pk_sorted(tc, new_id, new_line); 
    tc->record_count++; 
    rewrite_file(tc); 
    printf("[성공] 데이터 추가 완료\n");
}

void execute_select(Statement *stmt) {
    TableCache *tc = get_table(stmt->table_name); 
    if (!tc) return;
    
    int where_idx = get_col_idx(tc, stmt->where_col);
    printf("\n--- [%s] 조회 결과 ---\n", tc->table_name);
    
    for (int i = 0; i < tc->record_count; i++) {
        if (where_idx != -1) {
            char row_buf[1024]; 
            char *fields[MAX_COLS] = {0}; 
            parse_csv_row(tc->records[i], fields, row_buf);
            
            if (compare_value(fields[where_idx], stmt->where_val)) {
                printf("%s\n", tc->records[i]);
            }
        } else {
            printf("%s\n", tc->records[i]);
        }
    }
}

void execute_update(Statement *stmt) {
    TableCache *tc = get_table(stmt->table_name); 
    if (!tc) return;
    
    int where_idx = get_col_idx(tc, stmt->where_col);
    int set_idx = get_col_idx(tc, stmt->set_col);
    
    if (where_idx == -1 || set_idx == -1) { 
        printf("[실패] 컬럼을 찾을 수 없습니다.\n"); 
        return; 
    }
    
    if (set_idx == tc->pk_idx) {
        printf("[실패] 기본키(PK)는 UPDATE로 수정할 수 없습니다. 삭제 후 재삽입하세요.\n"); 
        return;
    }

    trim_and_unquote(stmt->set_val);
    
    if (tc->cols[set_idx].type == COL_NN && strlen(stmt->set_val) == 0) {
        printf("[실패] UPDATE 거부: '%s' (NN 제약).\n", tc->cols[set_idx].name); 
        return;
    }

    int match_flags[MAX_RECORDS] = {0};
    int target_count = 0;
    
    for (int i = 0; i < tc->record_count; i++) {
        char row_buf[1024]; 
        char *f[MAX_COLS] = {0}; 
        parse_csv_row(tc->records[i], f, row_buf);
        
        if (compare_value(f[where_idx], stmt->where_val)) { 
            match_flags[i] = 1; 
            target_count++; 
        }
    }

    if (tc->cols[set_idx].type == COL_UK) {
        if (target_count > 1) { 
            printf("[실패] UPDATE 거부: 다중 행에 동일한 UK 설정 불가.\n"); 
            return; 
        }
        for (int r = 0; r < tc->record_count; r++) {
            if (match_flags[r]) continue; 
            
            char row_buf[1024]; 
            char *f[MAX_COLS] = {0}; 
            parse_csv_row(tc->records[r], f, row_buf);
            
            if (compare_value(f[set_idx], stmt->set_val)) { 
                printf("[실패] UPDATE 거부: '%s' 중복 (UK 제약).\n", stmt->set_val); 
                return; 
            }
        }
    }

    int count = 0;
    for (int i = 0; i < tc->record_count; i++) {
        if (match_flags[i]) {
            char row_buf[1024]; 
            char *fields[MAX_COLS] = {0}; 
            parse_csv_row(tc->records[i], fields, row_buf);
            
            char new_row[1024] = ""; 
            size_t offset = 0;
            
            for (int j = 0; j < tc->col_count; j++) {
                const char *val = (j == set_idx) ? stmt->set_val : (fields[j] ? fields[j] : "");
                int w = snprintf(new_row + offset, sizeof(new_row) - offset, "%s%s", val, (j < tc->col_count - 1) ? "," : "");
                if (w < 0 || (size_t)w >= sizeof(new_row) - offset) break; 
                offset += w;
            }
            
            strncpy(tc->records[i], new_row, 1023); 
            tc->records[i][1023] = '\0'; 
            count++;
        }
    }
    
    if (count > 0) { 
        rewrite_file(tc); 
        printf("[성공] %d개의 행이 수정되었습니다.\n", count); 
    } else {
        printf("[알림] 수정할 대상이 없습니다.\n");
    }
}

void execute_delete(Statement *stmt) {
    TableCache *tc = get_table(stmt->table_name); 
    if (!tc) return;
    
    int where_idx = get_col_idx(tc, stmt->where_col);
    if (where_idx == -1) { 
        printf("[실패] 조건 컬럼을 찾을 수 없습니다.\n"); 
        return; 
    }
    
    int count = 0;
    for (int i = 0; i < tc->record_count; i++) {
        char row_buf[1024]; 
        char *fields[MAX_COLS] = {0}; 
        parse_csv_row(tc->records[i], fields, row_buf);
        
        if (compare_value(fields[where_idx], stmt->where_val)) {
            for (int j = i; j < tc->record_count - 1; j++) {
                tc->pk_index[j] = tc->pk_index[j + 1]; 
                strncpy(tc->records[j], tc->records[j + 1], 1023); 
                tc->records[j][1023] = '\0';
            }
            tc->record_count--; 
            count++; 
            i--; 
        }
    }
    
    if (count > 0) { 
        rewrite_file(tc); 
        printf("[성공] %d개의 행 삭제 완료\n", count); 
    } else {
        printf("[알림] 삭제할 대상이 없습니다.\n");
    }
}


// ==========================================
// [5] 메인 함수
// ==========================================
int main(int argc, char *argv[]) {
    char filename[256];
    
    if (argc >= 2) { 
        strncpy(filename, argv[1], 255); 
        filename[255] = '\0'; 
    } else { 
        printf("SQL 파일명: "); 
        if (scanf("%255s", filename) != 1) return 1; 
    }
    
    FILE *f = fopen(filename, "r");
    if (!f) { 
        printf("[오류] '%s' 파일을 찾을 수 없습니다.\n", filename); 
        return 1; 
    }
    
    char buf[MAX_SQL_LEN]; 
    int idx = 0;
    int q = 0; 
    int ch;
    
    while ((ch = fgetc(f)) != EOF) {
        if (ch == '-' && !q) {
            int n = fgetc(f);
            if (n == '-') { 
                while ((ch = fgetc(f)) != EOF && ch != '\n'); 
                continue; 
            }
            ungetc(n, f);
        }
        
        if (ch == '\'') q = !q;
        
        if (ch == ';' && !q) {
            buf[idx] = '\0'; 
            char *s = buf; 
            
            while (isspace((unsigned char)*s)) s++;
            
            if (strlen(s) > 0) {
                Statement stmt;
                if (parse_statement(s, &stmt)) {
                    if (stmt.type == STMT_INSERT) execute_insert(&stmt);
                    else if (stmt.type == STMT_SELECT) execute_select(&stmt);
                    else if (stmt.type == STMT_DELETE) execute_delete(&stmt);
                    else if (stmt.type == STMT_UPDATE) execute_update(&stmt);
                } else {
                    printf("[알림] 인식할 수 없거나 문법이 틀린 쿼리입니다: %s\n", s);
                }
            }
            idx = 0; 
        } else if (idx < MAX_SQL_LEN - 1) { 
            buf[idx++] = (char)ch; 
        }
    }
    
    fclose(f);
    
    for (int i = 0; i < open_table_count; i++) {
        if (open_tables[i].file) fclose(open_tables[i].file);
    }
    
    return 0;
}
