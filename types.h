#ifndef TYPES_H
#define TYPES_H

#include <stdio.h>

#define MAX_RECORDS 25000
#define MAX_COLS 15
#define MAX_TABLES 10
#define MAX_UKS 5
#define MAX_SQL_LEN 4096

/* sample */
typedef enum {
    STMT_INSERT,
    STMT_SELECT,
    STMT_DELETE,
    STMT_UPDATE,
    STMT_UNRECOGNIZED
} StatementType;

/* 而щ읆???쒖빟 議곌굔???섑??대뒗 ??낆엯?덈떎. (?쇰컲 / PK / UK / NN) */
typedef enum {
    COL_NORMAL,
    COL_PK,
    COL_UK,
    COL_NN
} ColumnType;

/* ?뚯꽌媛 ??媛쒖쓽 SQL 臾몄옣???댁꽍??寃곌낵瑜??댁뒿?덈떎. */
/* ?? SELECT??????뚯씠釉붾챸, WHERE 議곌굔, UPDATE??set ?뺣낫 ?깆쓣 ?ш린 ?섎굹濡??꾨떖?⑸땲?? */
typedef struct {
    /* 臾몄옣 醫낅쪟: SELECT/INSERT/UPDATE/DELETE. */
    StatementType type;
    /* ????뚯씠釉??대쫫. */
    char table_name[256];
    /* INSERT VALUES(...) ?대????먮낯 臾몄옄?? */
    char row_data[1024];
    /* UPDATE ... SET ?먯꽌 諛붽? 而щ읆紐? */
    char set_col[50];
    /* UPDATE ... SET 媛? */
    char set_val[256];
    /* WHERE ?덉쓽 鍮꾧탳 ???而щ읆. */
    char where_col[50];
    /* WHERE ??鍮꾧탳 媛? */
    char where_val[256];
} Statement;

typedef struct {
    /* 而щ읆 ?대쫫. */
    char name[50];
    /* 而щ읆 ?쒖빟 ???PK, UK, NN ??. */
    ColumnType type;
} ColumnInfo;

/* ?뚯씠釉?罹먯떆?낅땲?? */
/* ?뚯씪?먯꽌 ?쎌? ?ㅻ뜑/?덉퐫?쒕? 硫붾え由ъ뿉 ?ｌ뼱 諛섎났 ?ъ슜??SQL 泥섎━瑜?鍮좊Ⅴ寃??⑸땲?? */
typedef struct {
    /* ?뚯씠釉??뚯씪紐??뺤옣???쒖쇅). */
    char table_name[256];
    /* ?대젮 ?덈뒗 CSV ?뚯씪 ?ъ씤?? */
    FILE *file;
    /* ?ㅻ뜑?먯꽌 ?뚯떛??而щ읆 ?뺣낫. */
    ColumnInfo cols[MAX_COLS];
    /* ?ㅻ뜑???ㅼ젣 而щ읆 ?? */
    int col_count;
    /* PK 而щ읆 ?몃뜳???놁쑝硫?-1). */
    int pk_idx;
    /* UK 而щ읆 ?몃뜳??紐⑸줉. */
    int uk_indices[MAX_UKS];
    /* UK 而щ읆 媛쒖닔. */
    int uk_count;
    /* PK ?뺣젹/以묐났 寃?ъ슜 ?몃뜳??諛곗뿴. */
    long pk_index[MAX_RECORDS];
    /* CSV ?덉퐫??臾몄옄??罹먯떆. */
    char records[MAX_RECORDS][1024];
    /* ?꾩옱 罹먯떆??濡쒕뱶???덉퐫???? */
    int record_count;
    /* LRU 援먯껜??理쒓렐 ?묎렐 ?쒕쾲. */
    unsigned long long last_used_seq;
} TableCache;

/* ?댄쐶 遺꾩꽍湲곌? 留뚮뱺 ?좏겙 醫낅쪟?낅땲?? */
/* SQL ?뚯떛 ????湲???⑥뼱瑜?臾몃쾿 ?⑥쐞濡??섎닏?덈떎. */
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

/* ?좏겙 ?섎굹瑜??쒗쁽?섎뒗 援ъ“泥댁엯?덈떎. */
/* type(醫낅쪟)怨?text(?ㅼ젣 臾몄옄?????④퍡 蹂닿??⑸땲?? */
typedef struct {
    /* ?좏겙 醫낅쪟. */
    TokenType type;
    /* ?좏겙???ㅼ젣 ?띿뒪?? */
    char text[256];
} Token;

/* ?꾩옱 SQL 臾몄옄?댁뿉???대뵒源뚯? ?쎌뿀?붿? 愿由ы빀?덈떎. */
typedef struct {
    /* ?뚯떛??SQL 臾몄옄?? */
    const char *sql;
    /* ?꾩옱 ?쎈뒗 ?꾩튂. */
    int pos;
} Lexer;

/* ?뚯꽌 ?숈옉 ?곹깭?낅땲?? */
/* Lexer ?곹깭 + ?꾩옱 ?좏겙??臾띠뼱 ??臾몄옣 ?뚯떛???ъ슜?⑸땲?? */
typedef struct {
    /* ?꾩옱 ?ъ슜 以묒씤 lexer ?곹깭. */
    Lexer lexer;
    /* ?꾩옱 ?꾩튂?먯꽌 ?쎌? ?좏겙. */
    Token current_token;
} Parser;

extern TableCache open_tables[MAX_TABLES];
extern int open_table_count;

#endif
