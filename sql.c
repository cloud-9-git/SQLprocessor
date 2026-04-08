#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// =================================================================
// 1. 자료구조 정의
// =================================================================
typedef enum { STMT_INSERT, STMT_SELECT, STMT_UNRECOGNIZED } StatementType;

typedef struct {
    StatementType type;
    char table_name[256];
    char row_data[1024];
} Statement;

// [개선1] 파일 포인터 캐싱 (File Handle Caching)을 위한 구조체
typedef struct {
    char table_name[256];
    FILE *file;
} TableCache;

TableCache open_tables[100];
int open_table_count = 0;

// =================================================================
// 2. 유틸리티 및 DB 엔진 함수
// =================================================================

// 대문자 변환 함수 (대소문자 구분 없는 파싱을 위함)
void string_to_upper(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = toupper((unsigned char)str[i]);
    }
}

// [개선1, 2] 테이블 파일을 캐시에서 찾거나, 존재할 때만 새로 엽니다.
FILE* get_table_file(const char* table_name) {
    for (int i = 0; i < open_table_count; i++) {
        if (strcmp(open_tables[i].table_name, table_name) == 0) {
            return open_tables[i].file;
        }
    }

    // [개선3] 버퍼 오버플로우 방지
    char filename[300];
    snprintf(filename, sizeof(filename), "%s.csv", table_name);

    // "r+" : 읽기/쓰기 모드. 파일이 없으면 생성하지 않고 NULL 반환. (암묵적 CREATE TABLE 방지)
    FILE *file = fopen(filename, "r+");
    if (file == NULL) {
        printf("[오류] '%s' 테이블이 존재하지 않습니다. (파일 없음)\n", filename);
        return NULL;
    }

    // 캐시에 등록
    if (open_table_count < 100) {
        strcpy(open_tables[open_table_count].table_name, table_name);
        open_tables[open_table_count].file = file;
        open_table_count++;
    }

    return file;
}

// 프로그램 종료 시 열려있는 모든 캐시 파일을 닫습니다.
void close_all_tables() {
    for (int i = 0; i < open_table_count; i++) {
        fclose(open_tables[i].file);
    }
}

// =================================================================
// 3. 파서 (Parser) - [개선3, 4] 엄격한 검증 및 버퍼 오버플로우 방지
// =================================================================
int parse_statement(const char *sql, Statement *stmt) {
    char cmd1[20], cmd2[20], cmd3[20];
    char garbage[256];

    // 원본 문자열 보호를 위한 복사본 생성
    char sql_copy[4096];
    strncpy(sql_copy, sql, sizeof(sql_copy) - 1);
    sql_copy[sizeof(sql_copy) - 1] = '\0';

    if (sscanf(sql_copy, "%19s", cmd1) != 1) return 0;
    string_to_upper(cmd1);

    if (strcmp(cmd1, "INSERT") == 0) {
        // [개선3, 5] 길이 제한 설정, schema.table 파싱 가능토록 %255s 사용
        // [개선4] garbage 변수를 두어 WHERE 같은 지원하지 않는 찌꺼기 명령어를 감지
        int matched = sscanf(sql_copy, "%19s %19s %255s %19s ( %1023[^)] ) %255s",
                             cmd1, cmd2, stmt->table_name, cmd3, stmt->row_data, garbage);

        string_to_upper(cmd2);
        string_to_upper(cmd3);

        // 정확히 5개 요소가 매칭되어야 정상 문법 (garbage가 매칭되면 matched=6이 되어 실패)
        if (matched == 5 && strcmp(cmd2, "INTO") == 0 && strcmp(cmd3, "VALUES") == 0) {
            stmt->type = STMT_INSERT;
            return 1;
        }
    } 
    else if (strcmp(cmd1, "SELECT") == 0) {
        int matched = sscanf(sql_copy, "%19s %19s %19s %255s %255s",
                             cmd1, cmd2, cmd3, stmt->table_name, garbage);

        string_to_upper(cmd3);

        // 정확히 4개 요소가 매칭되어야 정상 (matched=5면 실패)
        if (matched == 4 && strcmp(cmd2, "*") == 0 && strcmp(cmd3, "FROM") == 0) {
            stmt->type = STMT_SELECT;
            return 1;
        }
    }

    return 0; // 문법 오류 또는 미지원 쿼리
}

// =================================================================
// 4. 실행 엔진 (Execution)
// =================================================================
void execute_insert(Statement *stmt) {
    FILE *file = get_table_file(stmt->table_name);
    if (file == NULL) return; // 파일(테이블)이 없으면 중단

    // [개선1] 데이터를 추가하기 위해 파일 포인터를 맨 끝으로 이동
    fseek(file, 0, SEEK_END);
    fprintf(file, "%s\n", stmt->row_data);
    fflush(file); // 메모리 버퍼를 디스크에 강제 기록
    
    printf("[성공] '%s' 테이블에 데이터 추가 완료\n", stmt->table_name);
}

void execute_select(Statement *stmt) {
    FILE *file = get_table_file(stmt->table_name);
    if (file == NULL) return;

    // [개선1] 데이터를 처음부터 읽기 위해 파일 포인터를 맨 앞으로 이동
    fseek(file, 0, SEEK_SET);

    printf("\n=== [%s] 조회 결과 ===\n", stmt->table_name);
    char line[1024];
    while (fgets(line, sizeof(line), file) != NULL) {
        printf("%s", line);
    }
    printf("======================\n");

    // 다음 읽기/쓰기를 위해 EOF(End of File) 상태 플래그 초기화
    clearerr(file);
}

void execute_statement(Statement *stmt) {
    if (stmt->type == STMT_INSERT) {
        execute_insert(stmt);
    } else if (stmt->type == STMT_SELECT) {
        execute_select(stmt);
    }
}

// =================================================================
// 5. 메인 로직 - [개선6] 글자 단위 스트림 처리 (Multi-line, 따옴표 무시)
// =================================================================
int main(int argc, char *argv[]) {
    char filename[256];

    if (argc >= 2) {
        strncpy(filename, argv[1], sizeof(filename)-1);
    } else {
        printf("실행할 SQL 파일 이름을 입력하세요: ");
        scanf("%255s", filename);
    }

    FILE *sql_file = fopen(filename, "r");
    if (sql_file == NULL) {
        printf("[오류] '%s' 파일을 열 수 없습니다.\n", filename);
        return 1;
    }

    char sql_buffer[4096];
    int buf_idx = 0;
    int in_quotes = 0;
    int ch;

    // 파일에서 문자 하나씩(char by char) 읽어오기
    while ((ch = fgetc(sql_file)) != EOF) {
        // [개선3] 한 문장이 버퍼 크기를 넘어가면 오버플로우 방지를 위해 스킵
        if (buf_idx >= sizeof(sql_buffer) - 1) {
            printf("[오류] SQL 문장이 너무 깁니다. 해당 문장 파싱을 건너뜁니다.\n");
            while ((ch = fgetc(sql_file)) != EOF && ch != ';') { /* 세미콜론까지 건너뛰기 */ }
            buf_idx = 0;
            in_quotes = 0;
            continue;
        }

        if (ch == '\'') {
            in_quotes = !in_quotes; // 문자열 내부 상태 토글
        }

        // 따옴표 바깥에 있는 세미콜론(;)을 만났을 때만 문장의 끝으로 간주
        if (ch == ';' && !in_quotes) {
            sql_buffer[buf_idx] = '\0'; // 문자열 완성
            
            // 앞부분 공백(엔터, 탭, 띄어쓰기 등) 무시
            char *start = sql_buffer;
            while (isspace(*start)) start++;

            if (strlen(start) > 0) {
                Statement stmt;
                if (parse_statement(start, &stmt) == 1) {
                    execute_statement(&stmt);
                } else {
                    printf("\n[에러] 문법 오류 또는 미지원 쿼리: %s;\n", start);
                }
            }
            buf_idx = 0; // 다음 문장을 담기 위해 버퍼 초기화
        } else {
            sql_buffer[buf_idx++] = (char)ch;
        }
    }

    fclose(sql_file);
    close_all_tables(); // 프로그램 종료 시 열려있는 모든 테이블 안전하게 닫기
    return 0;
}