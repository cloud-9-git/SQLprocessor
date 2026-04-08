#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "executor.h"
TableCache open_tables[MAX_TABLES];
int open_table_count = 0;
static unsigned long long g_table_access_seq = 0;
static void insert_pk_sorted(TableCache *tc, long val, const char* row_str);
/* qsort/bsearch?먯꽌 鍮꾧탳??long 媛?鍮꾧탳 ?⑥닔?낅땲?? */
static int compare_long(const void *a, const void *b) {
    long val_a = *(long *)a;
    long val_b = *(long *)b;
    return (val_a > val_b) - (val_a < val_b);
}
/* 罹먯떆??PK ?몃뜳??諛곗뿴?먯꽌 媛?議댁옱 ?щ?瑜??대텇 ?먯깋?쇰줈 李얠뒿?덈떎. */
static int find_in_pk_index(TableCache *tc, long val) {
    if (tc->record_count == 0 || tc->pk_idx == -1) return -1;
    long *found = bsearch(&val, tc->pk_index, tc->record_count, sizeof(long), compare_long);
    return found ? (int)(found - tc->pk_index) : -1;
}
/* ?낅젰 臾몄옄???묐걹 怨듬갚怨?媛먯떬 ?곗샂??')瑜??쒓굅??鍮꾧탳/??μ뿉 ?곌린 醫뗪쾶 ?뺣━?⑸땲?? */
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
/* 臾몄옄??鍮꾧탳 ?꾩뿉 怨듬갚/?곗샂?쒕? ?뺣━??媛숈? 媛믪씤吏 ?뺤씤?⑸땲?? */
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
/* CSV ??以꾩쓣 而대쭏 湲곗??쇰줈 ?꾨뱶 ?⑥쐞 諛곗뿴濡?遺꾪빐?⑸땲?? */
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
/* 而щ읆 ?대쫫?쇰줈 ?꾩옱 ?뚯씠釉붿뿉??而щ읆 ?몃뜳?ㅻ? 李얠뒿?덈떎. */
static int get_col_idx(TableCache *tc, const char *col_name) {
    if (!col_name || strlen(col_name) == 0) return -1;
    int i;
    for (i = 0; i < tc->col_count; i++) {
        if (strcmp(tc->cols[i].name, col_name) == 0) return i;
    }
    return -1;
}
/* 罹먯떆???꾩옱 ?곹깭(?ㅻ뜑 + 紐⑤뱺 ?덉퐫??瑜?CSV ?뚯씪濡??ㅼ떆 ?곷땲?? */
void rewrite_file(TableCache *tc) {
    int i;
    if (tc->file) fclose(tc->file);
    char filename[300];
    snprintf(filename, sizeof(filename), "%s.csv", tc->table_name);
    tc->file = fopen(filename, "w+");
    if (!tc->file) return;
    for (i = 0; i < tc->col_count; i++) {
        fprintf(tc->file, "%s", tc->cols[i].name);
        if (tc->cols[i].type == COL_PK) {
            fprintf(tc->file, "(PK)");
        } else if (tc->cols[i].type == COL_UK) {
            fprintf(tc->file, "(UK)");
        } else if (tc->cols[i].type == COL_NN) {
            fprintf(tc->file, "(NN)");
        }
        fprintf(tc->file, "%s", (i == tc->col_count - 1 ? "\n" : ","));
    }
    for (i = 0; i < tc->record_count; i++) {
        fprintf(tc->file, "%s\n", tc->records[i]);
    }
    fflush(tc->file);
}
/* LRU ?뺤콉?먯꽌 援먯껜 ??곸씠 ?????덈룄濡??ъ슜 ?쒓컖??媛깆떊?⑸땲?? */
static void touch_table(TableCache *tc) {
    tc->last_used_seq = ++g_table_access_seq;
}
/* TableCache ?щ’???덈줈 ?ъ슜?섎뒗 ?곹깭濡?珥덇린?뷀빀?덈떎. */
static void reset_table_cache(TableCache *tc) {
    memset(tc, 0, sizeof(TableCache));
    tc->file = NULL;
    tc->pk_idx = -1;
}
/* ?꾩옱 ?대┛ ?뚯씠釉?以?媛???ㅻ옯?숈븞 ?ъ슜?섏? ?딆? ?щ’??李얠뒿?덈떎. */
static int find_lru_table_index(void) {
    int i;
    int lru_index = 0;
    unsigned long long oldest_seq = open_tables[0].last_used_seq;
    for (i = 1; i < open_table_count; i++) {
        if (open_tables[i].last_used_seq < oldest_seq) {
            oldest_seq = open_tables[i].last_used_seq;
            lru_index = i;
        }
    }
    return lru_index;
}
/* ???뚯씪?????곹깭???몃뱾濡?TableCache ?щ’??梨꾩썎?덈떎. */
static int load_table_contents(TableCache *tc, const char *name, FILE *f) {
    reset_table_cache(tc);
    strncpy(tc->table_name, name, 255);
    tc->table_name[255] = '\0';
    tc->file = f;
    tc->record_count = 0;
    tc->pk_idx = -1;
    tc->col_count = 0;
    tc->uk_count = 0;
    tc->last_used_seq = ++g_table_access_seq;
    char header[1024];
    if (fgets(header, sizeof(header), f)) {
        char *token = strtok(header, ",\n\r");
        int idx = 0;
        while (token && idx < MAX_COLS) {
            char *paren = strchr(token, '(');
            if (paren) {
                int len = (int)(paren - token);
                if (len >= (int)sizeof(tc->cols[idx].name)) len = (int)sizeof(tc->cols[idx].name) - 1;
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
    return 1;
}
/* PK ?뺣젹 洹쒖튃???좎??섎룄濡????덉퐫?쒕? ?곸젅???꾩튂???쎌엯?⑸땲?? */
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
/* ?뚯씠釉??대쫫?쇰줈 罹먯떆瑜?李얘퀬, ?놁쑝硫??뚯씪???댁뼱 罹먯떆瑜?援ъ꽦?⑸땲?? */
/* 罹먯떆媛 媛??李쇱쑝硫?LRU(Least Recently Used) ?쒖쑝濡?媛???ㅻ옒???щ’??援먯껜??濡쒕뱶?⑸땲?? */
TableCache* get_table(const char* name) {
    int i;
    for (i = 0; i < open_table_count; i++) {
        if (strcmp(open_tables[i].table_name, name) == 0) {
            touch_table(&open_tables[i]);
            return &open_tables[i];
        }
    }
    char filename[300];
    snprintf(filename, sizeof(filename), "%s.csv", name);
    FILE *f = fopen(filename, "r+");
    if (!f) {
        printf("[?먮윭] '%s.csv' ?뚯씪???????놁뒿?덈떎.\n", name);
        return NULL;
    }
    TableCache *tc = NULL;
    if (open_table_count < MAX_TABLES) {
        tc = &open_tables[open_table_count++];
    } else {
        int evict_idx = find_lru_table_index();
        tc = &open_tables[evict_idx];
        printf("[INFO] Evict table `%s` and load `%s` with LRU cache.\n", tc->table_name, name);
        if (tc->file) fclose(tc->file);
    }
    if (!load_table_contents(tc, name, f)) {
        if (open_table_count < MAX_TABLES) open_table_count--;
        fclose(f);
        return NULL;
    }
    return tc;
}
/* INSERT ?ㅽ뻾 ?듭떖 ?⑥닔: */
/* ?됱쓣 ?뚯떛???쒖빟(NN/PK/UK)??寃?ы븯怨?罹먯떆??諛섏쁺 ???뚯씪????ν빀?덈떎. */
void execute_insert(Statement *stmt) {
    TableCache *tc = get_table(stmt->table_name);
    if (!tc) return;
    char buffer[1024];
    char *vals[MAX_COLS] = {0};
    parse_csv_row(stmt->row_data, vals, buffer);
    int val_count = 0;
    while (vals[val_count] && val_count < MAX_COLS) val_count++;
    long new_id = 0;
    int i;
    int r;
    for (i = 0; i < tc->col_count; i++) {
        char *val = (i < val_count && vals[i]) ? vals[i] : "";
        char normalized_val[256];
        strncpy(normalized_val, val, sizeof(normalized_val) - 1);
        normalized_val[sizeof(normalized_val) - 1] = '\0';
        trim_and_unquote(normalized_val);
        if (tc->cols[i].type == COL_NN && strlen(normalized_val) == 0) {
            printf("[?ㅻ쪟] INSERT ?ㅽ뙣: '%s' (NN ?쒖빟).\n", tc->cols[i].name);
            return;
        }
        if (i == tc->pk_idx && strlen(normalized_val) > 0) {
            new_id = atol(normalized_val);
            if (find_in_pk_index(tc, new_id) != -1) {
                printf("[?ㅻ쪟] PK 以묐났: %ld\n", new_id);
                return;
            }
        }
        if (tc->cols[i].type == COL_UK && strlen(normalized_val) > 0) {
            for (r = 0; r < tc->record_count; r++) {
                char row_buf[1024];
                char *f[MAX_COLS] = {0};
                parse_csv_row(tc->records[r], f, row_buf);
                if (compare_value(f[i], normalized_val)) {
                    printf("[?ㅻ쪟] INSERT ?ㅽ뙣: '%s' 以묐났 (UK ?쒖빟).\n", normalized_val);
                    return;
                }
            }
        }
    }
    char new_line[1024] = "";
    size_t offset = 0;
    for (i = 0; i < tc->col_count; i++) {
        const char *v = (i < val_count && vals[i]) ? vals[i] : ((tc->cols[i].type == COL_NN) ? "" : "NULL");
        char normalized_storage_val[1024];
        char formatted_val[1024];
        strncpy(normalized_storage_val, v, sizeof(normalized_storage_val) - 1);
        normalized_storage_val[sizeof(normalized_storage_val) - 1] = '\0';
        trim_and_unquote(normalized_storage_val);
        if (strchr(normalized_storage_val, ',')) {
            snprintf(formatted_val, sizeof(formatted_val), "'%.*s'", (int)(sizeof(formatted_val) - 3), normalized_storage_val);
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
    printf("[?꾨즺] ?곗씠??1嫄??쎌엯??n");
}
/* SELECT ?ㅽ뻾: 議곌굔???놁쑝硫??꾩껜 異쒕젰, ?덉쑝硫?where ?꾪꽣 ?곸슜 異쒕젰?낅땲?? */
void execute_select(Statement *stmt) {
    TableCache *tc = get_table(stmt->table_name);
    if (!tc) return;
    int where_idx = get_col_idx(tc, stmt->where_col);
    printf("\n--- [%s] 議고쉶 寃곌낵 ---\n", tc->table_name);
    int i;
    for (i = 0; i < tc->record_count; i++) {
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
/* UPDATE ?ㅽ뻾: */
/* where 議곌굔?쇰줈 ????됱쓣 李얠븘 set 媛믪쑝濡?媛깆떊?섍퀬 ?뚯씪??媛깆떊?⑸땲?? */
void execute_update(Statement *stmt) {
    TableCache *tc = get_table(stmt->table_name);
    if (!tc) return;
    int where_idx = get_col_idx(tc, stmt->where_col);
    int set_idx = get_col_idx(tc, stmt->set_col);
    if (where_idx == -1 || set_idx == -1) {
        printf("[?ㅻ쪟] ???而щ읆??李얠쓣 ???놁뒿?덈떎.\n");
        return;
    }
    if (set_idx == tc->pk_idx) {
        printf("[?ㅻ쪟] PK(湲곕낯????UPDATE濡?蹂寃쏀븷 ???놁뒿?덈떎. 醫낅즺?⑸땲??\n");
        return;
    }
    trim_and_unquote(stmt->set_val);
    if (tc->cols[set_idx].type == COL_NN && strlen(stmt->set_val) == 0) {
        printf("[?ㅻ쪟] UPDATE ?ㅽ뙣: '%s' (NN ?쒖빟).\n", tc->cols[set_idx].name);
        return;
    }
    int match_flags[MAX_RECORDS] = {0};
    int target_count = 0;
    int i;
    int r;
    int j;
    for (i = 0; i < tc->record_count; i++) {
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
            printf("[?ㅻ쪟] UPDATE ?ㅽ뙣: ?ㅼ닔??????됱뿉 UK 以묐났 媛?μ꽦 ?덉뒿?덈떎.\n");
            return;
        }
        for (r = 0; r < tc->record_count; r++) {
            if (match_flags[r]) continue;
            char row_buf[1024];
            char *f[MAX_COLS] = {0};
            parse_csv_row(tc->records[r], f, row_buf);
            if (compare_value(f[set_idx], stmt->set_val)) {
                printf("[?ㅻ쪟] UPDATE ?ㅽ뙣: '%s' 以묐났 (UK ?쒖빟).\n", stmt->set_val);
                return;
            }
        }
    }
    int count = 0;
    for (i = 0; i < tc->record_count; i++) {
        if (match_flags[i]) {
            char row_buf[1024];
            char *fields[MAX_COLS] = {0};
            parse_csv_row(tc->records[i], fields, row_buf);
            char new_row[1024] = "";
            size_t offset = 0;
            for (j = 0; j < tc->col_count; j++) {
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
        printf("[?꾨즺] %d嫄댁쓽 ?됱씠 媛깆떊??n", count);
    } else {
        printf("[?덈궡] ????됱씠 ?놁뒿?덈떎.\n");
    }
}
/* DELETE ?ㅽ뻾: */
/* where 議곌굔??留욌뒗 ?됱쓣 紐⑤몢 ?쒓굅?섍퀬 ?⑥? ?됱쑝濡?罹먯떆瑜??뺤텞??????ν빀?덈떎. */
void execute_delete(Statement *stmt) {
    TableCache *tc = get_table(stmt->table_name);
    if (!tc) return;
    int where_idx = get_col_idx(tc, stmt->where_col);
    if (where_idx == -1) {
        printf("[?ㅻ쪟] 議곌굔 而щ읆??李얠쓣 ???놁뒿?덈떎.\n");
        return;
    }
    int count = 0;
    int i;
    int j;
    for (i = 0; i < tc->record_count; i++) {
        char row_buf[1024];
        char *fields[MAX_COLS] = {0};
        parse_csv_row(tc->records[i], fields, row_buf);
        if (compare_value(fields[where_idx], stmt->where_val)) {
            for (j = i; j < tc->record_count - 1; j++) {
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
        printf("[?꾨즺] %d嫄댁씠 ??젣??n", count);
    } else {
        printf("[?덈궡] ??젣 ??곸씠 ?놁뒿?덈떎.\n");
    }
}
/* 醫낅즺 ?쒖젏 ?뺣━: */
/* 吏湲덇퉴吏 ???뚯씠釉??뚯씪 ?ъ씤?곕? 紐⑤몢 ?レ뒿?덈떎. */
void close_all_tables(void) {
    int i;
    for (i = 0; i < open_table_count; i++) {
        if (open_tables[i].file) fclose(open_tables[i].file);
    }
}
