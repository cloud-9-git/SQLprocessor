#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_COLUMNS 32
#define MAX_NAME 64
#define MAX_VALUE 256
#define MAX_LINE 2048

typedef enum StatementType {
    STMT_INSERT,
    STMT_SELECT,
    STMT_INVALID
} StatementType;

typedef struct Statement {
    StatementType type;
    char schema[MAX_NAME];
    char table[MAX_NAME];
    bool select_all;
    bool has_column_list;
    int column_count;
    char columns[MAX_COLUMNS][MAX_NAME];
    int value_count;
    char values[MAX_COLUMNS][MAX_VALUE];
} Statement;

static char *skip_spaces(char *text) {
    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }
    return text;
}

static void trim(char *text) {
    size_t length = strlen(text);
    size_t start = 0;

    while (text[start] != '\0' && isspace((unsigned char)text[start])) {
        start++;
    }

    while (length > start && isspace((unsigned char)text[length - 1])) {
        text[--length] = '\0';
    }

    if (start > 0) {
        memmove(text, text + start, length - start + 1);
    }
}

static int ascii_ncasecmp(const char *left, const char *right, size_t count) {
    size_t index;

    for (index = 0; index < count; index++) {
        unsigned char l = (unsigned char)left[index];
        unsigned char r = (unsigned char)right[index];
        int diff = tolower(l) - tolower(r);

        if (diff != 0 || l == '\0' || r == '\0') {
            return diff;
        }
    }

    return 0;
}

static bool equals_ignore_case(const char *left, const char *right) {
    return ascii_ncasecmp(left, right, strlen(left) > strlen(right) ? strlen(left) : strlen(right)) == 0 &&
           strlen(left) == strlen(right);
}

static bool read_keyword(char **cursor, const char *keyword) {
    size_t length = strlen(keyword);
    char *text = skip_spaces(*cursor);

    if (ascii_ncasecmp(text, keyword, length) != 0) {
        return false;
    }

    if (text[length] != '\0' && !isspace((unsigned char)text[length])) {
        return false;
    }

    *cursor = text + length;
    return true;
}

static bool split_table_name(const char *input, char *schema, char *table, char *error) {
    const char *dot = strchr(input, '.');

    if (dot == NULL) {
        if (snprintf(schema, MAX_NAME, "default") >= MAX_NAME ||
            snprintf(table, MAX_NAME, "%s", input) >= MAX_NAME) {
            snprintf(error, MAX_LINE, "테이블 이름이 너무 깁니다.");
            return false;
        }
        return true;
    }

    if ((size_t)(dot - input) >= MAX_NAME || strlen(dot + 1) >= MAX_NAME) {
        snprintf(error, MAX_LINE, "schema 또는 table 이름이 너무 깁니다.");
        return false;
    }

    memcpy(schema, input, (size_t)(dot - input));
    schema[dot - input] = '\0';
    snprintf(table, MAX_NAME, "%s", dot + 1);
    return true;
}

static bool read_table_name(char **cursor, char *schema, char *table, char *error) {
    char token[MAX_NAME * 2];
    int length = 0;
    char *text = skip_spaces(*cursor);

    while (*text != '\0' && !isspace((unsigned char)*text) && *text != '(') {
        if (length >= (int)sizeof(token) - 1) {
            snprintf(error, MAX_LINE, "테이블 이름이 너무 깁니다.");
            return false;
        }
        token[length++] = *text++;
    }
    token[length] = '\0';

    if (length == 0) {
        snprintf(error, MAX_LINE, "테이블 이름이 없습니다.");
        return false;
    }

    if (!split_table_name(token, schema, table, error)) {
        return false;
    }

    *cursor = text;
    return true;
}

static bool add_list_item(char *items, size_t item_size, int *count, int max_count, char *token, char *error) {
    char *slot;

    trim(token);
    if (token[0] == '\0') {
        snprintf(error, MAX_LINE, "비어 있는 항목이 있습니다.");
        return false;
    }

    if (*count >= max_count) {
        snprintf(error, MAX_LINE, "지원 컬럼 수를 초과했습니다.");
        return false;
    }

    slot = items + ((size_t)(*count) * item_size);
    if (snprintf(slot, item_size, "%s", token) >= (int)item_size) {
        snprintf(error, MAX_LINE, "항목 길이가 너무 깁니다.");
        return false;
    }

    (*count)++;
    return true;
}

static bool parse_list(const char *text, char *items, size_t item_size, int *count, int max_count, char *error) {
    char token[MAX_VALUE];
    size_t token_length = 0;
    bool in_string = false;
    size_t index;

    *count = 0;
    token[0] = '\0';

    for (index = 0;; index++) {
        char current = text[index];

        if (in_string) {
            if (current == '\'') {
                if (text[index + 1] == '\'') {
                    if (token_length + 1 >= sizeof(token)) {
                        snprintf(error, MAX_LINE, "문자열 값이 너무 깁니다.");
                        return false;
                    }
                    token[token_length++] = '\'';
                    token[token_length] = '\0';
                    index++;
                    continue;
                }
                in_string = false;
                continue;
            }

            if (current == '\0') {
                snprintf(error, MAX_LINE, "문자열 리터럴이 닫히지 않았습니다.");
                return false;
            }

            if (token_length + 1 >= sizeof(token)) {
                snprintf(error, MAX_LINE, "문자열 값이 너무 깁니다.");
                return false;
            }

            token[token_length++] = current;
            token[token_length] = '\0';
            continue;
        }

        if (current == '\'') {
            in_string = true;
            continue;
        }

        if (current == ',' || current == '\0') {
            token[token_length] = '\0';
            if (!add_list_item(items, item_size, count, max_count, token, error)) {
                return false;
            }
            token_length = 0;
            token[0] = '\0';
            if (current == '\0') {
                break;
            }
            continue;
        }

        if (token_length + 1 >= sizeof(token)) {
            snprintf(error, MAX_LINE, "항목 길이가 너무 깁니다.");
            return false;
        }

        token[token_length++] = current;
        token[token_length] = '\0';
    }

    return *count > 0;
}

static char *find_closing_paren(char *text) {
    bool in_string = false;

    while (*text != '\0') {
        if (*text == '\'') {
            if (in_string && text[1] == '\'') {
                text += 2;
                continue;
            }
            in_string = !in_string;
        } else if (*text == ')' && !in_string) {
            return text;
        }
        text++;
    }

    return NULL;
}

static char *find_keyword_outside_quotes(char *text, const char *keyword) {
    size_t keyword_length = strlen(keyword);
    bool in_string = false;
    size_t index;

    for (index = 0; text[index] != '\0'; index++) {
        if (text[index] == '\'') {
            if (in_string && text[index + 1] == '\'') {
                index++;
                continue;
            }
            in_string = !in_string;
            continue;
        }

        if (!in_string &&
            ascii_ncasecmp(text + index, keyword, keyword_length) == 0 &&
            (index == 0 || isspace((unsigned char)text[index - 1])) &&
            (text[index + keyword_length] == '\0' || isspace((unsigned char)text[index + keyword_length]))) {
            return text + index;
        }
    }

    return NULL;
}

static bool parse_insert(char *sql, Statement *statement, char *error) {
    char *cursor = sql;
    char *end;

    if (!read_keyword(&cursor, "INSERT") || !read_keyword(&cursor, "INTO")) {
        snprintf(error, MAX_LINE, "INSERT 문법이 올바르지 않습니다.");
        return false;
    }

    if (!read_table_name(&cursor, statement->schema, statement->table, error)) {
        return false;
    }

    cursor = skip_spaces(cursor);
    if (*cursor == '(') {
        end = strchr(cursor, ')');
        if (end == NULL) {
            snprintf(error, MAX_LINE, "컬럼 목록의 ')'를 찾지 못했습니다.");
            return false;
        }

        *end = '\0';
        if (!parse_list(cursor + 1,
                        (char *)statement->columns,
                        sizeof(statement->columns[0]),
                        &statement->column_count,
                        MAX_COLUMNS,
                        error)) {
            return false;
        }

        statement->has_column_list = true;
        cursor = end + 1;
    }

    if (!read_keyword(&cursor, "VALUES")) {
        snprintf(error, MAX_LINE, "VALUES 키워드가 필요합니다.");
        return false;
    }

    cursor = skip_spaces(cursor);
    if (*cursor != '(') {
        snprintf(error, MAX_LINE, "VALUES 뒤에 '('가 필요합니다.");
        return false;
    }

    end = find_closing_paren(cursor + 1);
    if (end == NULL) {
        snprintf(error, MAX_LINE, "값 목록의 ')'를 찾지 못했습니다.");
        return false;
    }

    *end = '\0';
    if (!parse_list(cursor + 1,
                    (char *)statement->values,
                    sizeof(statement->values[0]),
                    &statement->value_count,
                    MAX_COLUMNS,
                    error)) {
        return false;
    }

    if (statement->has_column_list && statement->column_count != statement->value_count) {
        snprintf(error, MAX_LINE, "컬럼 수와 값 수가 다릅니다.");
        return false;
    }

    cursor = skip_spaces(end + 1);
    if (*cursor != '\0') {
        snprintf(error, MAX_LINE, "INSERT 문장 끝에 불필요한 내용이 있습니다.");
        return false;
    }

    statement->type = STMT_INSERT;
    return true;
}

static bool parse_select(char *sql, Statement *statement, char *error) {
    char *cursor = sql;
    char *from;

    if (!read_keyword(&cursor, "SELECT")) {
        snprintf(error, MAX_LINE, "SELECT 문법이 올바르지 않습니다.");
        return false;
    }

    from = find_keyword_outside_quotes(cursor, "FROM");
    if (from == NULL) {
        snprintf(error, MAX_LINE, "FROM 키워드가 필요합니다.");
        return false;
    }

    *from = '\0';
    trim(cursor);
    if (strcmp(cursor, "*") == 0) {
        statement->select_all = true;
    } else if (!parse_list(cursor,
                           (char *)statement->columns,
                           sizeof(statement->columns[0]),
                           &statement->column_count,
                           MAX_COLUMNS,
                           error)) {
        return false;
    }

    cursor = from + 4;
    if (!read_table_name(&cursor, statement->schema, statement->table, error)) {
        return false;
    }

    cursor = skip_spaces(cursor);
    if (*cursor != '\0') {
        snprintf(error, MAX_LINE, "현재 WHERE, ORDER BY는 지원하지 않습니다.");
        return false;
    }

    statement->type = STMT_SELECT;
    return true;
}

static bool parse_statement(char *sql, Statement *statement, char *error) {
    memset(statement, 0, sizeof(*statement));

    if (ascii_ncasecmp(sql, "INSERT", 6) == 0) {
        return parse_insert(sql, statement, error);
    }

    if (ascii_ncasecmp(sql, "SELECT", 6) == 0) {
        return parse_select(sql, statement, error);
    }

    snprintf(error, MAX_LINE, "지원하지 않는 SQL입니다. INSERT, SELECT만 지원합니다.");
    return false;
}

static bool ensure_directory(const char *path, char *error) {
    char buffer[PATH_MAX];
    size_t length = strlen(path);
    size_t index;

    if (length >= sizeof(buffer)) {
        snprintf(error, MAX_LINE, "디렉터리 경로가 너무 깁니다.");
        return false;
    }

    memcpy(buffer, path, length + 1);
    for (index = 1; index < length; index++) {
        if (buffer[index] == '/') {
            buffer[index] = '\0';
            if (mkdir(buffer, 0755) != 0 && errno != EEXIST) {
                snprintf(error, MAX_LINE, "디렉터리 생성 실패: %s", buffer);
                return false;
            }
            buffer[index] = '/';
        }
    }

    if (mkdir(buffer, 0755) != 0 && errno != EEXIST) {
        snprintf(error, MAX_LINE, "디렉터리 생성 실패: %s", buffer);
        return false;
    }

    return true;
}

static bool build_table_path(const char *data_root, const Statement *statement, char *path, char *schema_dir, char *error) {
    if (snprintf(schema_dir, PATH_MAX, "%s/%s", data_root, statement->schema) >= PATH_MAX ||
        snprintf(path, PATH_MAX, "%s/%s/%s.csv", data_root, statement->schema, statement->table) >= PATH_MAX) {
        snprintf(error, MAX_LINE, "테이블 경로가 너무 깁니다.");
        return false;
    }

    return true;
}

static bool parse_csv_line(char *line, char items[][MAX_VALUE], int *count, char *error) {
    return parse_list(line, (char *)items, sizeof(items[0]), count, MAX_COLUMNS, error);
}

static bool read_header(FILE *file, char headers[][MAX_NAME], int *header_count, char *error) {
    char line[MAX_LINE];
    char values[MAX_COLUMNS][MAX_VALUE];
    int index;
    int value_count = 0;

    if (fgets(line, sizeof(line), file) == NULL) {
        snprintf(error, MAX_LINE, "테이블 파일에 헤더가 없습니다.");
        return false;
    }

    line[strcspn(line, "\r\n")] = '\0';
    if (!parse_csv_line(line, values, &value_count, error)) {
        return false;
    }

    *header_count = value_count;
    for (index = 0; index < value_count; index++) {
        if (snprintf(headers[index], sizeof(headers[index]), "%s", values[index]) >= (int)sizeof(headers[index])) {
            snprintf(error, MAX_LINE, "헤더 컬럼 이름이 너무 깁니다.");
            return false;
        }
    }

    return true;
}

static int find_column_index(char headers[][MAX_NAME], int header_count, const char *column_name) {
    int index;

    for (index = 0; index < header_count; index++) {
        if (equals_ignore_case(headers[index], column_name)) {
            return index;
        }
    }

    return -1;
}

static void print_selected(char items[][MAX_VALUE], int *indices, int count) {
    int index;

    for (index = 0; index < count; index++) {
        if (index > 0) {
            printf(" | ");
        }
        printf("%s", items[indices[index]]);
    }
    printf("\n");
}

static void write_csv_row(FILE *file, char items[][MAX_VALUE], int count) {
    int index;

    for (index = 0; index < count; index++) {
        if (index > 0) {
            fputc(',', file);
        }
        fputs(items[index], file);
    }
    fputc('\n', file);
}

static void write_header_row(FILE *file, const char columns[][MAX_NAME], int count) {
    int index;

    for (index = 0; index < count; index++) {
        if (index > 0) {
            fputc(',', file);
        }
        fputs(columns[index], file);
    }
    fputc('\n', file);
}

static bool execute_insert(const Statement *statement, const char *data_root, char *error) {
    char path[PATH_MAX];
    char schema_dir[PATH_MAX];
    char headers[MAX_COLUMNS][MAX_NAME];
    int header_count = 0;
    int index;
    FILE *file = NULL;

    if (!build_table_path(data_root, statement, path, schema_dir, error)) {
        return false;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        if (!statement->has_column_list) {
            snprintf(error, MAX_LINE, "새 테이블에 INSERT하려면 컬럼 목록이 필요합니다.");
            return false;
        }

        if (!ensure_directory(data_root, error) || !ensure_directory(schema_dir, error)) {
            return false;
        }

        file = fopen(path, "w");
        if (file == NULL) {
            snprintf(error, MAX_LINE, "테이블 파일을 생성할 수 없습니다: %s", path);
            return false;
        }

        write_header_row(file, statement->columns, statement->column_count);
        fclose(file);
        file = NULL;
    } else {
        if (!read_header(file, headers, &header_count, error)) {
            fclose(file);
            return false;
        }
        fclose(file);
        file = NULL;
    }

    if (header_count == 0) {
        header_count = statement->column_count;
        for (index = 0; index < statement->column_count; index++) {
            snprintf(headers[index], sizeof(headers[index]), "%s", statement->columns[index]);
        }
    }

    if (statement->has_column_list) {
        if (statement->column_count != header_count) {
            snprintf(error, MAX_LINE, "테이블 헤더와 INSERT 컬럼 수가 다릅니다.");
            return false;
        }

        for (index = 0; index < header_count; index++) {
            if (!equals_ignore_case(headers[index], statement->columns[index])) {
                snprintf(error, MAX_LINE, "컬럼 순서 또는 이름이 테이블 헤더와 다릅니다.");
                return false;
            }
        }
    }

    if (statement->value_count != header_count) {
        snprintf(error, MAX_LINE, "값 수가 테이블 컬럼 수와 다릅니다.");
        return false;
    }

    file = fopen(path, "a");
    if (file == NULL) {
        snprintf(error, MAX_LINE, "테이블 파일을 열 수 없습니다: %s", path);
        return false;
    }

    write_csv_row(file, (char (*)[MAX_VALUE])statement->values, statement->value_count);
    fclose(file);

    printf("INSERT 1 %s.%s\n", statement->schema, statement->table);
    return true;
}

static bool execute_select(const Statement *statement, const char *data_root, char *error) {
    char path[PATH_MAX];
    char schema_dir[PATH_MAX];
    char headers[MAX_COLUMNS][MAX_NAME];
    char row[MAX_COLUMNS][MAX_VALUE];
    int projection[MAX_COLUMNS];
    int header_count = 0;
    int projection_count = 0;
    int row_count = 0;
    int index;
    FILE *file;
    char line[MAX_LINE];

    if (!build_table_path(data_root, statement, path, schema_dir, error)) {
        return false;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        snprintf(error, MAX_LINE, "테이블 파일이 없습니다: %s", path);
        return false;
    }

    if (!read_header(file, headers, &header_count, error)) {
        fclose(file);
        return false;
    }

    if (statement->select_all) {
        projection_count = header_count;
        for (index = 0; index < header_count; index++) {
            projection[index] = index;
        }
    } else {
        projection_count = statement->column_count;
        for (index = 0; index < statement->column_count; index++) {
            projection[index] = find_column_index(headers, header_count, statement->columns[index]);
            if (projection[index] < 0) {
                fclose(file);
                snprintf(error, MAX_LINE, "존재하지 않는 컬럼입니다: %s", statement->columns[index]);
                return false;
            }
        }
    }

    printf("RESULT %s.%s\n", statement->schema, statement->table);
    for (index = 0; index < projection_count; index++) {
        if (index > 0) {
            printf(" | ");
        }
        printf("%s", headers[projection[index]]);
    }
    printf("\n");

    while (fgets(line, sizeof(line), file) != NULL) {
        int value_count = 0;

        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') {
            continue;
        }

        if (!parse_csv_line(line, row, &value_count, error)) {
            fclose(file);
            return false;
        }

        if (value_count != header_count) {
            fclose(file);
            snprintf(error, MAX_LINE, "행 데이터의 컬럼 수가 헤더와 다릅니다.");
            return false;
        }

        print_selected(row, projection, projection_count);
        row_count++;
    }

    fclose(file);
    printf("(%d rows)\n", row_count);
    return true;
}

static bool execute_statement(const Statement *statement, const char *data_root, char *error) {
    if (statement->type == STMT_INSERT) {
        return execute_insert(statement, data_root, error);
    }

    if (statement->type == STMT_SELECT) {
        return execute_select(statement, data_root, error);
    }

    snprintf(error, MAX_LINE, "실행할 수 없는 SQL입니다.");
    return false;
}

static bool run_sql_file(const char *sql_path, const char *data_root, char *error) {
    FILE *file = fopen(sql_path, "r");
    char line[MAX_LINE];
    int line_number = 0;

    if (file == NULL) {
        snprintf(error, MAX_LINE, "SQL 파일을 열 수 없습니다: %s", sql_path);
        return false;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        Statement statement;
        line_number++;

        line[strcspn(line, "\r\n")] = '\0';
        trim(line);

        if (line[0] == '\0' || (line[0] == '-' && line[1] == '-')) {
            continue;
        }

        if (line[strlen(line) - 1] == ';') {
            line[strlen(line) - 1] = '\0';
            trim(line);
        }

        if (!parse_statement(line, &statement, error)) {
            fclose(file);
            snprintf(error, MAX_LINE, "%d행 파싱 오류: %s", line_number, error);
            return false;
        }

        if (!execute_statement(&statement, data_root, error)) {
            fclose(file);
            snprintf(error, MAX_LINE, "%d행 실행 오류: %s", line_number, error);
            return false;
        }
    }

    fclose(file);
    return true;
}

static void print_usage(const char *program_name) {
    fprintf(stderr, "사용법: %s <sql-file> [data-root]\n", program_name);
    fprintf(stderr, "예시  : %s examples/demo.sql data\n", program_name);
}

int main(int argc, char **argv) {
    char error[MAX_LINE] = {0};
    const char *data_root = "data";

    if (argc < 2 || argc > 3) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (argc == 3) {
        data_root = argv[2];
    }

    if (!run_sql_file(argv[1], data_root, error)) {
        fprintf(stderr, "오류: %s\n", error);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
