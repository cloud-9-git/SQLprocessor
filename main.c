#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "types.h"
#include "parser.h"
#include "executor.h"

/* SQL ?뚯씪????以꾩뵫 ?쎌뼱 ';' ?⑥쐞濡?SQL 臾몄옣??遺꾨━???ㅽ뻾?⑸땲?? */
int main(int argc, char *argv[]) {
    char filename[256];

    if (argc >= 2) {
        strncpy(filename, argv[1], 255);
        filename[255] = '\0';
    } else {
        printf("SQL ?뚯씪 寃쎈줈: ");
        if (scanf("%255s", filename) != 1) return 1;
    }

    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("[?먮윭] '%s' ?뚯씪???????놁뒿?덈떎.\n", filename);
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
                    printf("[?덈궡] ?좏슚?섏? ?딆? SQL 臾몄엯?덈떎. %s\n", s);
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
