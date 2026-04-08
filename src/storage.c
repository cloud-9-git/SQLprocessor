#define _POSIX_C_SOURCE 200809L

#include "storage.h"

#include "common.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static void free_string_array(char **items, size_t count) {
    size_t index;

    for (index = 0; index < count; index++) {
        free(items[index]);
    }
    free(items);
}

static void free_row_array(char ***rows, size_t row_count, size_t column_count) {
    size_t row_index;
    size_t column_index;

    for (row_index = 0; row_index < row_count; row_index++) {
        for (column_index = 0; column_index < column_count; column_index++) {
            free(rows[row_index][column_index]);
        }
        free(rows[row_index]);
    }
    free(rows);
}

void storage_free_table(TableData *table) {
    if (table == NULL) {
        return;
    }

    free_string_array(table->columns, table->column_count);
    free_row_array(table->rows, table->row_count, table->column_count);
    table->columns = NULL;
    table->rows = NULL;
    table->column_count = 0;
    table->row_count = 0;
}

static bool build_schema_dir(const char *data_root,
                             const char *schema,
                             char *buffer,
                             size_t buffer_size,
                             char *error_buffer,
                             size_t error_buffer_size) {
    int written = snprintf(buffer, buffer_size, "%s/%s", data_root, schema);

    if (written < 0 || (size_t)written >= buffer_size) {
        sp_set_error(error_buffer, error_buffer_size, "스키마 경로가 너무 깁니다.");
        return false;
    }

    return true;
}

static bool build_table_path(const char *data_root,
                             const char *schema,
                             const char *table_name,
                             char *buffer,
                             size_t buffer_size,
                             char *error_buffer,
                             size_t error_buffer_size) {
    int written = snprintf(buffer, buffer_size, "%s/%s/%s.table", data_root, schema, table_name);

    if (written < 0 || (size_t)written >= buffer_size) {
        sp_set_error(error_buffer, error_buffer_size, "테이블 경로가 너무 깁니다.");
        return false;
    }

    return true;
}

static bool ensure_directory(const char *path, char *error_buffer, size_t error_buffer_size) {
    char temp[PATH_MAX];
    size_t index;
    size_t length = strlen(path);

    if (length == 0) {
        return true;
    }

    if (length >= sizeof(temp)) {
        sp_set_error(error_buffer, error_buffer_size, "디렉터리 경로가 너무 깁니다.");
        return false;
    }

    memcpy(temp, path, length + 1);

    for (index = 1; index < length; index++) {
        if (temp[index] == '/') {
            temp[index] = '\0';
            if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
                sp_set_error(error_buffer, error_buffer_size, "디렉터리 생성 실패: %s", temp);
                return false;
            }
            temp[index] = '/';
        }
    }

    if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
        sp_set_error(error_buffer, error_buffer_size, "디렉터리 생성 실패: %s", temp);
        return false;
    }

    return true;
}

static bool append_char(char **buffer, size_t *length, size_t *capacity, char value, char *error_buffer, size_t error_buffer_size) {
    if (*length + 1 >= *capacity) {
        size_t new_capacity = (*capacity == 0) ? 16 : (*capacity * 2);
        char *resized = (char *)realloc(*buffer, new_capacity);

        if (resized == NULL) {
            sp_set_error(error_buffer, error_buffer_size, "메모리 할당에 실패했습니다.");
            return false;
        }

        *buffer = resized;
        *capacity = new_capacity;
    }

    (*buffer)[(*length)++] = value;
    (*buffer)[*length] = '\0';
    return true;
}

static bool append_field(char ***fields,
                         size_t *field_count,
                         char *value,
                         char *error_buffer,
                         size_t error_buffer_size) {
    char **resized = (char **)realloc(*fields, sizeof(char *) * (*field_count + 1));

    if (resized == NULL) {
        free(value);
        sp_set_error(error_buffer, error_buffer_size, "메모리 할당에 실패했습니다.");
        return false;
    }

    resized[*field_count] = value;
    *fields = resized;
    (*field_count)++;
    return true;
}

static void trim_newline(char *line) {
    size_t length = strlen(line);

    while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
        line[length - 1] = '\0';
        length--;
    }
}

static bool split_encoded_line(const char *line,
                               char ***out_fields,
                               size_t *out_field_count,
                               char *error_buffer,
                               size_t error_buffer_size) {
    char **fields = NULL;
    size_t field_count = 0;
    char *current = NULL;
    size_t current_length = 0;
    size_t current_capacity = 0;
    size_t index;
    bool escaping = false;

    for (index = 0; line[index] != '\0'; index++) {
        char value = line[index];

        if (escaping) {
            char decoded = value;

            if (value == 'n') {
                decoded = '\n';
            } else if (value == 'r') {
                decoded = '\r';
            } else if (value == 't') {
                decoded = '\t';
            } else if (value != '\\' && value != '|') {
                free(current);
                free_string_array(fields, field_count);
                sp_set_error(error_buffer, error_buffer_size, "잘못된 이스케이프 시퀀스를 발견했습니다.");
                return false;
            }

            if (!append_char(&current, &current_length, &current_capacity, decoded, error_buffer, error_buffer_size)) {
                free(current);
                free_string_array(fields, field_count);
                return false;
            }

            escaping = false;
            continue;
        }

        if (value == '\\') {
            escaping = true;
            continue;
        }

        if (value == '|') {
            char *field_value = current == NULL ? sp_strdup("") : current;

            if (field_value == NULL) {
                free(current);
                free_string_array(fields, field_count);
                sp_set_error(error_buffer, error_buffer_size, "메모리 할당에 실패했습니다.");
                return false;
            }

            if (!append_field(&fields, &field_count, field_value, error_buffer, error_buffer_size)) {
                free(current);
                free_string_array(fields, field_count);
                return false;
            }
            current = NULL;
            current_length = 0;
            current_capacity = 0;
            continue;
        }

        if (!append_char(&current, &current_length, &current_capacity, value, error_buffer, error_buffer_size)) {
            free(current);
            free_string_array(fields, field_count);
            return false;
        }
    }

    if (escaping) {
        free(current);
        free_string_array(fields, field_count);
        sp_set_error(error_buffer, error_buffer_size, "라인 마지막에 잘못된 이스케이프가 있습니다.");
        return false;
    }

    {
        char *field_value = current == NULL ? sp_strdup("") : current;

        if (field_value == NULL) {
            free(current);
            free_string_array(fields, field_count);
            sp_set_error(error_buffer, error_buffer_size, "메모리 할당에 실패했습니다.");
            return false;
        }

        if (!append_field(&fields, &field_count, field_value, error_buffer, error_buffer_size)) {
            free(current);
            free_string_array(fields, field_count);
            return false;
        }
    }

    *out_fields = fields;
    *out_field_count = field_count;
    return true;
}

static bool append_row(TableData *table,
                       char **row_values,
                       char *error_buffer,
                       size_t error_buffer_size) {
    char ***resized = (char ***)realloc(table->rows, sizeof(char **) * (table->row_count + 1));

    if (resized == NULL) {
        sp_set_error(error_buffer, error_buffer_size, "메모리 할당에 실패했습니다.");
        return false;
    }

    resized[table->row_count] = row_values;
    table->rows = resized;
    table->row_count++;
    return true;
}

bool storage_load_table(const char *data_root,
                        const char *schema,
                        const char *table_name,
                        bool allow_missing,
                        TableData *out_table,
                        char *error_buffer,
                        size_t error_buffer_size) {
    FILE *file;
    char path[PATH_MAX];
    char *line = NULL;
    size_t line_capacity = 0;
    ssize_t line_length;
    TableData table;

    memset(&table, 0, sizeof(table));

    if (!build_table_path(data_root, schema, table_name, path, sizeof(path), error_buffer, error_buffer_size)) {
        return false;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        if (errno == ENOENT && allow_missing) {
            *out_table = table;
            return true;
        }

        sp_set_error(error_buffer, error_buffer_size, "테이블 파일을 열 수 없습니다: %s", path);
        return false;
    }

    while ((line_length = getline(&line, &line_capacity, file)) != -1) {
        char **fields = NULL;
        size_t field_count = 0;
        size_t index;

        (void)line_length;
        trim_newline(line);
        if (line[0] == '\0') {
            continue;
        }

        if (!split_encoded_line(line, &fields, &field_count, error_buffer, error_buffer_size)) {
            free(line);
            fclose(file);
            storage_free_table(&table);
            return false;
        }

        if (field_count == 0) {
            free_string_array(fields, field_count);
            continue;
        }

        if (strcmp(fields[0], "@columns") == 0) {
            if (table.column_count != 0) {
                free_string_array(fields, field_count);
                free(line);
                fclose(file);
                storage_free_table(&table);
                sp_set_error(error_buffer, error_buffer_size, "컬럼 헤더가 중복되었습니다.");
                return false;
            }

            table.column_count = field_count - 1;
            table.columns = (char **)calloc(table.column_count, sizeof(char *));
            if (table.column_count > 0 && table.columns == NULL) {
                free_string_array(fields, field_count);
                free(line);
                fclose(file);
                storage_free_table(&table);
                sp_set_error(error_buffer, error_buffer_size, "메모리 할당에 실패했습니다.");
                return false;
            }

            for (index = 1; index < field_count; index++) {
                table.columns[index - 1] = fields[index];
            }

            free(fields[0]);
            free(fields);
            continue;
        }

        if (strcmp(fields[0], "@row") == 0) {
            char **row_values;

            if (table.column_count == 0) {
                free_string_array(fields, field_count);
                free(line);
                fclose(file);
                storage_free_table(&table);
                sp_set_error(error_buffer, error_buffer_size, "행 정보보다 컬럼 헤더가 먼저 와야 합니다.");
                return false;
            }

            if (field_count - 1 != table.column_count) {
                free_string_array(fields, field_count);
                free(line);
                fclose(file);
                storage_free_table(&table);
                sp_set_error(error_buffer, error_buffer_size, "행의 값 수가 컬럼 수와 다릅니다.");
                return false;
            }

            row_values = (char **)calloc(table.column_count, sizeof(char *));
            if (row_values == NULL) {
                free_string_array(fields, field_count);
                free(line);
                fclose(file);
                storage_free_table(&table);
                sp_set_error(error_buffer, error_buffer_size, "메모리 할당에 실패했습니다.");
                return false;
            }

            for (index = 1; index < field_count; index++) {
                row_values[index - 1] = fields[index];
            }

            free(fields[0]);
            free(fields);

            if (!append_row(&table, row_values, error_buffer, error_buffer_size)) {
                free(line);
                fclose(file);
                storage_free_table(&table);
                return false;
            }
            continue;
        }

        free_string_array(fields, field_count);
        free(line);
        fclose(file);
        storage_free_table(&table);
        sp_set_error(error_buffer, error_buffer_size, "알 수 없는 레코드 타입을 발견했습니다.");
        return false;
    }

    free(line);
    fclose(file);
    *out_table = table;
    return true;
}

static char *encode_field(const char *value, char *error_buffer, size_t error_buffer_size) {
    char *encoded = NULL;
    size_t length = 0;
    size_t capacity = 0;
    size_t index;

    for (index = 0; value[index] != '\0'; index++) {
        char current = value[index];

        if (current == '\n') {
            if (!append_char(&encoded, &length, &capacity, '\\', error_buffer, error_buffer_size) ||
                !append_char(&encoded, &length, &capacity, 'n', error_buffer, error_buffer_size)) {
                free(encoded);
                return NULL;
            }
        } else if (current == '\r') {
            if (!append_char(&encoded, &length, &capacity, '\\', error_buffer, error_buffer_size) ||
                !append_char(&encoded, &length, &capacity, 'r', error_buffer, error_buffer_size)) {
                free(encoded);
                return NULL;
            }
        } else if (current == '\t') {
            if (!append_char(&encoded, &length, &capacity, '\\', error_buffer, error_buffer_size) ||
                !append_char(&encoded, &length, &capacity, 't', error_buffer, error_buffer_size)) {
                free(encoded);
                return NULL;
            }
        } else if (current == '\\' || current == '|') {
            if (!append_char(&encoded, &length, &capacity, '\\', error_buffer, error_buffer_size) ||
                !append_char(&encoded, &length, &capacity, current, error_buffer, error_buffer_size)) {
                free(encoded);
                return NULL;
            }
        } else {
            if (!append_char(&encoded, &length, &capacity, current, error_buffer, error_buffer_size)) {
                free(encoded);
                return NULL;
            }
        }
    }

    if (encoded == NULL) {
        encoded = sp_strdup("");
    }

    return encoded;
}

bool storage_save_table(const char *data_root,
                        const char *schema,
                        const char *table_name,
                        const TableData *table,
                        char *error_buffer,
                        size_t error_buffer_size) {
    FILE *file;
    char schema_dir[PATH_MAX];
    char path[PATH_MAX];
    char temp_path[PATH_MAX];
    size_t column_index;
    size_t row_index;

    if (!ensure_directory(data_root, error_buffer, error_buffer_size)) {
        return false;
    }

    if (!build_schema_dir(data_root, schema, schema_dir, sizeof(schema_dir), error_buffer, error_buffer_size)) {
        return false;
    }

    if (!ensure_directory(schema_dir, error_buffer, error_buffer_size)) {
        return false;
    }

    if (!build_table_path(data_root, schema, table_name, path, sizeof(path), error_buffer, error_buffer_size)) {
        return false;
    }

    {
        int written = snprintf(temp_path, sizeof(temp_path), "%s.tmp", path);
        if (written < 0 || (size_t)written >= sizeof(temp_path)) {
            sp_set_error(error_buffer, error_buffer_size, "임시 파일 경로가 너무 깁니다.");
            return false;
        }
    }

    file = fopen(temp_path, "w");
    if (file == NULL) {
        sp_set_error(error_buffer, error_buffer_size, "임시 파일을 열 수 없습니다: %s", temp_path);
        return false;
    }

    if (table->column_count > 0) {
        if (fprintf(file, "@columns") < 0) {
            fclose(file);
            unlink(temp_path);
            sp_set_error(error_buffer, error_buffer_size, "헤더를 기록하지 못했습니다.");
            return false;
        }

        for (column_index = 0; column_index < table->column_count; column_index++) {
            char *encoded = encode_field(table->columns[column_index], error_buffer, error_buffer_size);
            if (encoded == NULL) {
                fclose(file);
                unlink(temp_path);
                return false;
            }

            if (fprintf(file, "|%s", encoded) < 0) {
                free(encoded);
                fclose(file);
                unlink(temp_path);
                sp_set_error(error_buffer, error_buffer_size, "헤더를 기록하지 못했습니다.");
                return false;
            }

            free(encoded);
        }

        if (fprintf(file, "\n") < 0) {
            fclose(file);
            unlink(temp_path);
            sp_set_error(error_buffer, error_buffer_size, "헤더 줄바꿈을 기록하지 못했습니다.");
            return false;
        }
    }

    for (row_index = 0; row_index < table->row_count; row_index++) {
        if (fprintf(file, "@row") < 0) {
            fclose(file);
            unlink(temp_path);
            sp_set_error(error_buffer, error_buffer_size, "행을 기록하지 못했습니다.");
            return false;
        }

        for (column_index = 0; column_index < table->column_count; column_index++) {
            char *encoded = encode_field(table->rows[row_index][column_index], error_buffer, error_buffer_size);
            if (encoded == NULL) {
                fclose(file);
                unlink(temp_path);
                return false;
            }

            if (fprintf(file, "|%s", encoded) < 0) {
                free(encoded);
                fclose(file);
                unlink(temp_path);
                sp_set_error(error_buffer, error_buffer_size, "행을 기록하지 못했습니다.");
                return false;
            }

            free(encoded);
        }

        if (fprintf(file, "\n") < 0) {
            fclose(file);
            unlink(temp_path);
            sp_set_error(error_buffer, error_buffer_size, "행 줄바꿈을 기록하지 못했습니다.");
            return false;
        }
    }

    if (fclose(file) != 0) {
        unlink(temp_path);
        sp_set_error(error_buffer, error_buffer_size, "파일을 닫지 못했습니다.");
        return false;
    }

    if (rename(temp_path, path) != 0) {
        unlink(temp_path);
        sp_set_error(error_buffer, error_buffer_size, "테이블 파일 교체에 실패했습니다.");
        return false;
    }

    return true;
}
