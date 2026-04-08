#ifndef TYPES_H
#define TYPES_H

#include <stdio.h>

#define MAX_RECORDS 25000
#define MAX_COLS 15
#define MAX_TABLES 10
#define MAX_UKS 5
#define MAX_SQL_LEN 4096

// SQL 문장이 어떤 종류인지 구분하는 타입입니다.
typedef enum {
    STMT_INSERT,
    STMT_SELECT,
    STMT_DELETE,
    STMT_UPDATE,
    STMT_UNRECOGNIZED
} StatementType;

// 컬럼의 제약 조건을 나타내는 타입입니다. (일반 / PK / UK / NN)
typedef enum {
    COL_NORMAL,
    COL_PK,
    COL_UK,
    COL_NN
} ColumnType;

// 파서가 한 개의 SQL 문장을 해석한 결과를 담습니다.
// 예: SELECT의 대상 테이블명, WHERE 조건, UPDATE의 set 정보 등을 여기 하나로 전달합니다.
typedef struct {
    // 문장 종류: SELECT/INSERT/UPDATE/DELETE.
    StatementType type;
    // 대상 테이블 이름.
    char table_name[256];
    // INSERT VALUES(...) 내부의 원본 문자열.
    char row_data[1024];
    // UPDATE ... SET 에서 바꿀 컬럼명.
    char set_col[50];
    // UPDATE ... SET 값.
    char set_val[256];
    // WHERE 절의 비교 대상 컬럼.
    char where_col[50];
    // WHERE 절 비교 값.
    char where_val[256];
} Statement;

typedef struct {
    // 컬럼 이름.
    char name[50];
    // 컬럼 제약 타입(PK, UK, NN 등).
    ColumnType type;
} ColumnInfo;

// 테이블 캐시입니다.
// 파일에서 읽은 헤더/레코드를 메모리에 넣어 반복 사용해 SQL 처리를 빠르게 합니다.
typedef struct {
    // 테이블 파일명(확장자 제외).
    char table_name[256];
    // 열려 있는 CSV 파일 포인터.
    FILE *file;
    // 헤더에서 파싱한 컬럼 정보.
    ColumnInfo cols[MAX_COLS];
    // 헤더의 실제 컬럼 수.
    int col_count;
    // PK 컬럼 인덱스(없으면 -1).
    int pk_idx;
    // UK 컬럼 인덱스 목록.
    int uk_indices[MAX_UKS];
    // UK 컬럼 개수.
    int uk_count;
    // PK 정렬/중복 검사용 인덱스 배열.
    long pk_index[MAX_RECORDS];
    // CSV 레코드 문자열 캐시.
    char records[MAX_RECORDS][1024];
    // 현재 캐시에 로드된 레코드 수.
    int record_count;
} TableCache;

// 어휘 분석기가 만든 토큰 종류입니다.
// SQL 파싱 시 한 글자/단어를 문법 단위로 나눕니다.
typedef enum {
    TOKEN_EOF,
    TOKEN_ILLEGAL,
    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_NUMBER,
    TOKEN_SELECT,
    TOKEN_INSERT,
    TOKEN_UPDATE,
    TOKEN_DELETE,
    TOKEN_FROM,
    TOKEN_WHERE,
    TOKEN_SET,
    TOKEN_INTO,
    TOKEN_VALUES,
    TOKEN_STAR,
    TOKEN_COMMA,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_EQ,
    TOKEN_SEMICOLON
} TokenType;

// 토큰 하나를 표현하는 구조체입니다.
// type(종류)과 text(실제 문자열)을 함께 보관합니다.
typedef struct {
    // 토큰 종류.
    TokenType type;
    // 토큰의 실제 텍스트.
    char text[256];
} Token;

// 현재 SQL 문자열에서 어디까지 읽었는지 관리합니다.
typedef struct {
    // 파싱할 SQL 문자열.
    const char *sql;
    // 현재 읽는 위치.
    int pos;
} Lexer;

// 파서 동작 상태입니다.
// Lexer 상태 + 현재 토큰을 묶어 한 문장 파싱에 사용합니다.
typedef struct {
    // 현재 사용 중인 lexer 상태.
    Lexer lexer;
    // 현재 위치에서 읽은 토큰.
    Token current_token;
} Parser;

extern TableCache open_tables[MAX_TABLES];
extern int open_table_count;

#endif
