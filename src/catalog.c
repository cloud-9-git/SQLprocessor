#include "sqlproc/catalog.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* schema/row 유틸 전반에서 재사용하는 문자열 복사 함수입니다. */
static char *dup_string(const char *text, SqlError *err) {
    size_t length;
    char *copy;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *)malloc(length + 1U);
    if (copy == NULL) {
        sql_error_set(err, 0, 0, 0, "out of memory");
        return NULL;
    }

    memcpy(copy, text, length + 1U);
    return copy;
}

/* 스키마 키워드와 컬럼 이름 비교 시 대소문자를 무시합니다. */
static int equals_ignore_case(const char *left, const char *right) {
    size_t index = 0U;

    while (left[index] != '\0' && right[index] != '\0') {
        if (tolower((unsigned char)left[index]) != tolower((unsigned char)right[index])) {
            return 0;
        }
        index++;
    }

    return left[index] == '\0' && right[index] == '\0';
}

/* TableSchema 뒤에 새 컬럼 정의를 하나 추가합니다. */
static SqlStatus append_column(TableSchema *schema, const char *name, DataType type, SqlError *err) {
    ColumnDef *new_columns;
    size_t new_count = schema->column_count + 1U;
    char *name_copy;

    name_copy = dup_string(name, err);
    if (name_copy == NULL) {
        return SQL_STATUS_OOM;
    }

    new_columns = (ColumnDef *)realloc(schema->columns, sizeof(ColumnDef) * new_count);
    if (new_columns == NULL) {
        free(name_copy);
        sql_error_set(err, 0, 0, 0, "out of memory");
        return SQL_STATUS_OOM;
    }

    schema->columns = new_columns;
    schema->columns[schema->column_count].name = name_copy;
    schema->columns[schema->column_count].type = type;
    schema->column_count = new_count;
    return SQL_STATUS_OK;
}

/* TableSchema를 비어 있는 상태로 초기화합니다. */
void table_schema_init(TableSchema *schema) {
    if (schema == NULL) {
        return;
    }

    schema->table_name = NULL;
    schema->column_count = 0U;
    schema->columns = NULL;
}

/* TableSchema 안의 테이블 이름과 컬럼 배열을 모두 해제합니다. */
void table_schema_free(TableSchema *schema) {
    size_t index;

    if (schema == NULL) {
        return;
    }

    free(schema->table_name);
    schema->table_name = NULL;

    for (index = 0U; index < schema->column_count; index++) {
        free(schema->columns[index].name);
        schema->columns[index].name = NULL;
    }

    free(schema->columns);
    schema->columns = NULL;
    schema->column_count = 0U;
}

/* 스키마를 깊은 복사해 binder/planner/executor가 독립적으로 사용할 수 있게 합니다. */
SqlStatus table_schema_clone(const TableSchema *src, TableSchema *dest, SqlError *err) {
    size_t index;

    if (src == NULL || dest == NULL) {
        sql_error_set(err, 0, 0, 0, "table_schema_clone received null pointer");
        return SQL_STATUS_ERROR;
    }

    table_schema_init(dest);
    dest->table_name = dup_string(src->table_name, err);
    if (src->table_name != NULL && dest->table_name == NULL) {
        return SQL_STATUS_OOM;
    }

    for (index = 0U; index < src->column_count; index++) {
        SqlStatus status = append_column(dest, src->columns[index].name, src->columns[index].type, err);
        if (status != SQL_STATUS_OK) {
            table_schema_free(dest);
            return status;
        }
    }

    return SQL_STATUS_OK;
}

/* 컬럼 이름을 schema index로 해석합니다. */
int table_schema_find_column(const TableSchema *schema, const char *column_name) {
    size_t index;

    if (schema == NULL || column_name == NULL) {
        return -1;
    }

    for (index = 0U; index < schema->column_count; index++) {
        if (equals_ignore_case(schema->columns[index].name, column_name)) {
            return (int)index;
        }
    }

    return -1;
}

/* Row를 빈 상태로 초기화합니다. */
void row_init(Row *row) {
    if (row == NULL) {
        return;
    }

    row->value_count = 0U;
    row->values = NULL;
}

/* Row 안의 값 배열과 TEXT 메모리를 모두 해제합니다. */
void row_free(Row *row) {
    size_t index;

    if (row == NULL) {
        return;
    }

    for (index = 0U; index < row->value_count; index++) {
        value_free(&row->values[index]);
    }

    free(row->values);
    row->values = NULL;
    row->value_count = 0U;
}

/* Row를 깊은 복사합니다. */
SqlStatus row_clone(const Row *src, Row *dest, SqlError *err) {
    size_t index;

    if (src == NULL || dest == NULL) {
        sql_error_set(err, 0, 0, 0, "row_clone received null pointer");
        return SQL_STATUS_ERROR;
    }

    row_init(dest);
    if (src->value_count == 0U) {
        return SQL_STATUS_OK;
    }

    dest->values = (Value *)calloc(src->value_count, sizeof(Value));
    if (dest->values == NULL) {
        sql_error_set(err, 0, 0, 0, "out of memory");
        return SQL_STATUS_OOM;
    }

    dest->value_count = src->value_count;
    for (index = 0U; index < src->value_count; index++) {
        SqlStatus status = value_clone(&src->values[index], &dest->values[index], err);
        if (status != SQL_STATUS_OK) {
            row_free(dest);
            return status;
        }
    }

    return SQL_STATUS_OK;
}

/* catalog가 참조할 DB 루트 경로를 저장합니다. */
void catalog_init(Catalog *catalog, const char *db_root) {
    if (catalog == NULL) {
        return;
    }

    catalog->db_root = db_root;
}

/* 🧭 schema 파일을 읽어 TableSchema 객체로 바꾸는 핵심 함수입니다. */
SqlStatus catalog_load_table(const char *db_root, const char *table_name, TableSchema *out_schema, SqlError *err) {
    char path[1024];
    FILE *file;
    char line[512];
    int saw_version = 0;
    int saw_table = 0;

    if (db_root == NULL || table_name == NULL || out_schema == NULL) {
        sql_error_set(err, 0, 0, 0, "catalog_load_table received null pointer");
        return SQL_STATUS_ERROR;
    }

    table_schema_init(out_schema);

    if (snprintf(path, sizeof(path), "%s/schema/%s.schema", db_root, table_name) >= (int)sizeof(path)) {
        sql_error_set(err, 0, 0, 0, "schema path is too long");
        return SQL_STATUS_ERROR;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        sql_error_set(err, 0, 0, 0, "failed to open schema file for table '%s'", table_name);
        return SQL_STATUS_IO;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char keyword[64];
        char first[128];
        char second[128];
        int parsed;

        parsed = sscanf(line, "%63s %127s %127s", keyword, first, second);
        if (parsed <= 0) {
            continue;
        }

        if (equals_ignore_case(keyword, "version")) {
            if (parsed != 2 || !equals_ignore_case(first, "1")) {
                fclose(file);
                table_schema_free(out_schema);
                sql_error_set(err, 0, 0, 0, "unsupported schema version in '%s'", path);
                return SQL_STATUS_ERROR;
            }
            saw_version = 1;
            continue;
        }

        if (equals_ignore_case(keyword, "table")) {
            if (parsed != 2) {
                fclose(file);
                table_schema_free(out_schema);
                sql_error_set(err, 0, 0, 0, "invalid table declaration in '%s'", path);
                return SQL_STATUS_ERROR;
            }
            free(out_schema->table_name);
            out_schema->table_name = dup_string(first, err);
            if (out_schema->table_name == NULL) {
                fclose(file);
                table_schema_free(out_schema);
                return SQL_STATUS_OOM;
            }
            saw_table = 1;
            continue;
        }

        if (equals_ignore_case(keyword, "column")) {
            DataType type;
            SqlStatus status;

            if (parsed != 3 || value_parse_type_name(second, &type) != SQL_STATUS_OK) {
                fclose(file);
                table_schema_free(out_schema);
                sql_error_set(err, 0, 0, 0, "invalid column declaration in '%s'", path);
                return SQL_STATUS_ERROR;
            }

            status = append_column(out_schema, first, type, err);
            if (status != SQL_STATUS_OK) {
                fclose(file);
                table_schema_free(out_schema);
                return status;
            }
            continue;
        }

        fclose(file);
        table_schema_free(out_schema);
        sql_error_set(err, 0, 0, 0, "unknown schema directive '%s' in '%s'", keyword, path);
        return SQL_STATUS_ERROR;
    }

    fclose(file);

    if (!saw_version || !saw_table || out_schema->column_count == 0U) {
        table_schema_free(out_schema);
        sql_error_set(err, 0, 0, 0, "schema file '%s' is incomplete", path);
        return SQL_STATUS_ERROR;
    }

    if (!equals_ignore_case(out_schema->table_name, table_name)) {
        table_schema_free(out_schema);
        sql_error_set(err, 0, 0, 0, "schema table name '%s' does not match requested table '%s'",
                      out_schema->table_name, table_name);
        return SQL_STATUS_ERROR;
    }

    return SQL_STATUS_OK;
}
