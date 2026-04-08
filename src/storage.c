#include "sqlproc/storage.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* data/<table>.rows 파일 경로를 조합합니다. */
static SqlStatus make_data_path(const char *db_root, const char *table_name, char *buffer, size_t buffer_size, SqlError *err) {
    if (snprintf(buffer, buffer_size, "%s/data/%s.rows", db_root, table_name) >= (int)buffer_size) {
        sql_error_set(err, 0, 0, 0, "data path is too long");
        return SQL_STATUS_ERROR;
    }
    return SQL_STATUS_OK;
}

/* 이스케이프 문자열을 만들 때 동적 버퍼 뒤에 문자 하나를 추가합니다. */
static SqlStatus append_text(char **buffer, size_t *length, size_t *capacity, char ch, SqlError *err) {
    char *grown;

    if (*length + 2U > *capacity) {
        *capacity *= 2U;
        grown = (char *)realloc(*buffer, *capacity);
        if (grown == NULL) {
            sql_error_set(err, 0, 0, 0, "out of memory");
            return SQL_STATUS_OOM;
        }
        *buffer = grown;
    }

    (*buffer)[(*length)++] = ch;
    (*buffer)[*length] = '\0';
    return SQL_STATUS_OK;
}

/* TEXT 값을 row 파일에 안전하게 저장할 수 있도록 escape합니다. */
static SqlStatus escape_field(const char *plain_text, char **out_text, SqlError *err) {
    size_t capacity = strlen(plain_text) * 2U + 8U;
    size_t length = 0U;
    char *buffer = (char *)malloc(capacity);
    size_t index;
    SqlStatus status;

    if (buffer == NULL) {
        sql_error_set(err, 0, 0, 0, "out of memory");
        return SQL_STATUS_OOM;
    }
    buffer[0] = '\0';

    /* ⭐ row 파일은 escaped TSV 형식이므로 탭, 줄바꿈, 역슬래시를 반드시 escape합니다. */
    for (index = 0U; plain_text[index] != '\0'; index++) {
        char ch = plain_text[index];
        if (ch == '\\') {
            status = append_text(&buffer, &length, &capacity, '\\', err);
            if (status != SQL_STATUS_OK) {
                free(buffer);
                return status;
            }
            status = append_text(&buffer, &length, &capacity, '\\', err);
        } else if (ch == '\t') {
            status = append_text(&buffer, &length, &capacity, '\\', err);
            if (status != SQL_STATUS_OK) {
                free(buffer);
                return status;
            }
            status = append_text(&buffer, &length, &capacity, 't', err);
        } else if (ch == '\n') {
            status = append_text(&buffer, &length, &capacity, '\\', err);
            if (status != SQL_STATUS_OK) {
                free(buffer);
                return status;
            }
            status = append_text(&buffer, &length, &capacity, 'n', err);
        } else {
            status = append_text(&buffer, &length, &capacity, ch, err);
        }

        if (status != SQL_STATUS_OK) {
            free(buffer);
            return status;
        }
    }

    *out_text = buffer;
    return SQL_STATUS_OK;
}

/* row 파일에서 읽은 escaped 문자열을 다시 평문으로 복원합니다. */
static SqlStatus unescape_field(const char *escaped_text, char **out_text, SqlError *err) {
    size_t capacity = strlen(escaped_text) + 1U;
    size_t length = 0U;
    char *buffer = (char *)malloc(capacity + 1U);
    size_t index;

    if (buffer == NULL) {
        sql_error_set(err, 0, 0, 0, "out of memory");
        return SQL_STATUS_OOM;
    }
    buffer[0] = '\0';

    for (index = 0U; escaped_text[index] != '\0'; index++) {
        char ch = escaped_text[index];
        if (ch == '\\') {
            index++;
            ch = escaped_text[index];
            if (ch == 'n') {
                ch = '\n';
            } else if (ch == 't') {
                ch = '\t';
            } else if (ch == '\\') {
                ch = '\\';
            } else {
                free(buffer);
                sql_error_set(err, 0, 0, 0, "invalid escape sequence in row data");
                return SQL_STATUS_ERROR;
            }
        }

        buffer[length++] = ch;
        buffer[length] = '\0';
    }

    *out_text = buffer;
    return SQL_STATUS_OK;
}

/* 저장 파일의 문자열 필드를 schema 타입에 맞는 Value로 해석합니다. */
static SqlStatus parse_value_from_text(DataType type, const char *text, Value *out_value, SqlError *err) {
    char *end_ptr = NULL;

    if (type == DATA_TYPE_TEXT) {
        return value_make_text(out_value, text, err);
    }
    if (type == DATA_TYPE_INT) {
        long long parsed = strtoll(text, &end_ptr, 10);
        if (end_ptr == NULL || *end_ptr != '\0') {
            sql_error_set(err, 0, 0, 0, "invalid integer '%s' in row data", text);
            return SQL_STATUS_ERROR;
        }
        *out_value = value_make_int(parsed);
        return SQL_STATUS_OK;
    }
    if (type == DATA_TYPE_BOOL) {
        if (strcmp(text, "true") == 0) {
            *out_value = value_make_bool(1);
            return SQL_STATUS_OK;
        }
        if (strcmp(text, "false") == 0) {
            *out_value = value_make_bool(0);
            return SQL_STATUS_OK;
        }
        sql_error_set(err, 0, 0, 0, "invalid boolean '%s' in row data", text);
        return SQL_STATUS_ERROR;
    }

    sql_error_set(err, 0, 0, 0, "unsupported data type in row parser");
    return SQL_STATUS_ERROR;
}

/* 🧭 executor가 넘긴 row를 실제 row 파일 끝에 추가합니다. */
SqlStatus storage_append_row(const char *db_root, const TableSchema *schema, const Row *row, SqlError *err) {
    char path[1024];
    FILE *file;
    size_t index;
    SqlStatus status;

    if (db_root == NULL || schema == NULL || row == NULL) {
        sql_error_set(err, 0, 0, 0, "storage_append_row received null pointer");
        return SQL_STATUS_ERROR;
    }

    if (row->value_count != schema->column_count) {
        sql_error_set(err, 0, 0, 0, "row column count does not match schema column count");
        return SQL_STATUS_ERROR;
    }

    status = make_data_path(db_root, schema->table_name, path, sizeof(path), err);
    if (status != SQL_STATUS_OK) {
        return status;
    }

    file = fopen(path, "a");
    if (file == NULL) {
        sql_error_set(err, 0, 0, 0, "failed to open data file '%s' for append", path);
        return SQL_STATUS_IO;
    }

    /* ⚠️ storage는 schema 순서의 row만 기록합니다.
     * 입력 순서를 schema 순서로 맞추는 책임은 binder에 있습니다.
     */
    for (index = 0U; index < row->value_count; index++) {
        char *plain = NULL;
        char *escaped = NULL;

        status = value_to_plain_text(&row->values[index], &plain, err);
        if (status != SQL_STATUS_OK) {
            fclose(file);
            return status;
        }

        status = escape_field(plain, &escaped, err);
        free(plain);
        if (status != SQL_STATUS_OK) {
            fclose(file);
            return status;
        }

        if (index > 0U) {
            fputc('\t', file);
        }
        fputs(escaped, file);
        free(escaped);
    }
    fputc('\n', file);

    if (fclose(file) != 0) {
        sql_error_set(err, 0, 0, 0, "failed to close data file '%s'", path);
        return SQL_STATUS_IO;
    }

    return SQL_STATUS_OK;
}

/* 🧭 row 파일을 처음부터 끝까지 읽어 typed Row로 복원한 뒤 콜백에 넘깁니다. */
SqlStatus storage_scan_rows(const char *db_root, const TableSchema *schema, RowVisitor visit, void *ctx, SqlError *err) {
    char path[1024];
    FILE *file;
    char *line = NULL;
    size_t line_capacity = 0U;
    ssize_t line_length;
    SqlStatus status;

    if (db_root == NULL || schema == NULL || visit == NULL) {
        sql_error_set(err, 0, 0, 0, "storage_scan_rows received null pointer");
        return SQL_STATUS_ERROR;
    }

    status = make_data_path(db_root, schema->table_name, path, sizeof(path), err);
    if (status != SQL_STATUS_OK) {
        return status;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        if (errno == ENOENT) {
            return SQL_STATUS_OK;
        }
        sql_error_set(err, 0, 0, 0, "failed to open data file '%s' for reading", path);
        return SQL_STATUS_IO;
    }

    /* ⭐ executor는 파일 포맷을 몰라도 되도록,
     * storage 단계에서 raw text를 typed Row로 완전히 복원합니다.
     */
    while ((line_length = getline(&line, &line_capacity, file)) >= 0) {
        size_t field_index = 0U;
        size_t start = 0U;
        Row row;

        if (line_length > 0 && line[(size_t)line_length - 1U] == '\n') {
            line[(size_t)line_length - 1U] = '\0';
            line_length--;
        }
        if (line_length == 0) {
            continue;
        }

        row_init(&row);
        row.value_count = schema->column_count;
        row.values = (Value *)calloc(schema->column_count, sizeof(Value));
        if (row.values == NULL) {
            free(line);
            fclose(file);
            sql_error_set(err, 0, 0, 0, "out of memory");
            return SQL_STATUS_OOM;
        }

        for (size_t index = 0U; index <= (size_t)line_length; index++) {
            if (line[index] == '\t' || line[index] == '\0') {
                char saved = line[index];
                char *plain = NULL;

                if (field_index >= schema->column_count) {
                    row_free(&row);
                    free(line);
                    fclose(file);
                    sql_error_set(err, 0, 0, 0, "row has more fields than schema allows");
                    return SQL_STATUS_ERROR;
                }

                line[index] = '\0';
                status = unescape_field(&line[start], &plain, err);
                line[index] = saved;
                if (status != SQL_STATUS_OK) {
                    row_free(&row);
                    free(line);
                    fclose(file);
                    return status;
                }

                status = parse_value_from_text(schema->columns[field_index].type, plain, &row.values[field_index], err);
                free(plain);
                if (status != SQL_STATUS_OK) {
                    row_free(&row);
                    free(line);
                    fclose(file);
                    return status;
                }

                field_index++;
                start = index + 1U;
            }
        }

        if (field_index != schema->column_count) {
            row_free(&row);
            free(line);
            fclose(file);
            sql_error_set(err, 0, 0, 0, "row field count does not match schema");
            return SQL_STATUS_ERROR;
        }

        status = visit(&row, ctx, err);
        row_free(&row);
        if (status != SQL_STATUS_OK) {
            free(line);
            fclose(file);
            return status;
        }
    }

    free(line);
    if (fclose(file) != 0) {
        sql_error_set(err, 0, 0, 0, "failed to close data file '%s'", path);
        return SQL_STATUS_IO;
    }

    return SQL_STATUS_OK;
}
