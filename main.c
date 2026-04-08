#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "types.h"
#include "parser.h"
#include "executor.h"

/* SQL 파일을 한 줄씩 읽어 ';' 단위로 문장을 분해해 실행합니다. */
int main(int argc, char *argv[]) {
    char filename[256];

    if (argc >= 2) {
        strncpy(filename, argv[1], 255);
        filename[255] = '\0';
    } else {
        printf("실행할 SQL 파일 경로: ");
        if (scanf("%255s", filename) != 1) return 1;
    }

    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("[알림] '%s' 파일을 열 수 없습니다.\n", filename);
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
                    printf("[알림] 유효하지 않은 SQL 문장입니다. %s\n", s);
                }
            }
            idx = 0;
        } else if (idx < MAX_SQL_LEN - 1) {
            buf[idx++] = (char)ch;
        }
    }

    fclose(f);
    close_all_tables();

    return 0;
}

