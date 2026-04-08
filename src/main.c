#define _POSIX_C_SOURCE 200809L

#include "common.h"
#include "parser.h"
#include "processor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_text_file(const char *path, char *error_buffer, size_t error_buffer_size) {
    FILE *file;
    long file_size;
    char *buffer;
    size_t bytes_read;

    file = fopen(path, "rb");
    if (file == NULL) {
        sp_set_error(error_buffer, error_buffer_size, "SQL 파일을 열 수 없습니다: %s", path);
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        sp_set_error(error_buffer, error_buffer_size, "SQL 파일 크기를 확인할 수 없습니다.");
        return NULL;
    }

    file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        sp_set_error(error_buffer, error_buffer_size, "SQL 파일 크기를 읽을 수 없습니다.");
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        sp_set_error(error_buffer, error_buffer_size, "SQL 파일 위치를 되돌릴 수 없습니다.");
        return NULL;
    }

    buffer = (char *)malloc((size_t)file_size + 1);
    if (buffer == NULL) {
        fclose(file);
        sp_set_error(error_buffer, error_buffer_size, "메모리 할당에 실패했습니다.");
        return NULL;
    }

    bytes_read = fread(buffer, 1, (size_t)file_size, file);
    if (bytes_read != (size_t)file_size) {
        free(buffer);
        fclose(file);
        sp_set_error(error_buffer, error_buffer_size, "SQL 파일을 끝까지 읽지 못했습니다.");
        return NULL;
    }

    buffer[file_size] = '\0';
    fclose(file);
    return buffer;
}

static void print_usage(const char *program_name) {
    fprintf(stderr, "사용법: %s <sql-file> [data-root]\n", program_name);
    fprintf(stderr, "예시  : %s examples/demo.sql data\n", program_name);
}

int main(int argc, char **argv) {
    const char *sql_path;
    const char *data_root = "data";
    char error_buffer[512];
    char *sql_script = NULL;
    StatementList statements;
    int exit_code = EXIT_FAILURE;

    memset(&statements, 0, sizeof(statements));
    error_buffer[0] = '\0';

    if (argc < 2 || argc > 3) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    sql_path = argv[1];
    if (argc == 3) {
        data_root = argv[2];
    }

    sql_script = read_text_file(sql_path, error_buffer, sizeof(error_buffer));
    if (sql_script == NULL) {
        fprintf(stderr, "오류: %s\n", error_buffer);
        goto cleanup;
    }

    if (!parse_sql_script(sql_script, &statements, error_buffer, sizeof(error_buffer))) {
        fprintf(stderr, "파싱 오류: %s\n", error_buffer);
        goto cleanup;
    }

    if (!execute_statements(&statements, data_root, error_buffer, sizeof(error_buffer))) {
        fprintf(stderr, "실행 오류: %s\n", error_buffer);
        goto cleanup;
    }

    exit_code = EXIT_SUCCESS;

cleanup:
    free_statement_list(&statements);
    free(sql_script);
    return exit_code;
}
