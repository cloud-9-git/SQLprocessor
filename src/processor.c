#include "processor.h"

#include "common.h"
#include "storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

static void free_row(char **row, size_t column_count) {
    size_t index;

    for (index = 0; index < column_count; index++) {
        free(row[index]);
    }
    free(row);
}

static ssize_t find_column_index(const TableData *table, const char *column_name) {
    size_t index;

    for (index = 0; index < table->column_count; index++) {
        if (sp_equals_ignore_case(table->columns[index], column_name)) {
            return (ssize_t)index;
        }
    }

    return -1;
}

static bool append_column(TableData *table,
                          const char *column_name,
                          char *error_buffer,
                          size_t error_buffer_size) {
    char **new_columns;
    size_t row_index;

    new_columns = (char **)realloc(table->columns, sizeof(char *) * (table->column_count + 1));
    if (new_columns == NULL) {
        sp_set_error(error_buffer, error_buffer_size, "메모리 할당에 실패했습니다.");
        return false;
    }

    table->columns = new_columns;
    table->columns[table->column_count] = sp_strdup(column_name);
    if (table->columns[table->column_count] == NULL) {
        sp_set_error(error_buffer, error_buffer_size, "메모리 할당에 실패했습니다.");
        return false;
    }

    for (row_index = 0; row_index < table->row_count; row_index++) {
        char **new_row = (char **)realloc(table->rows[row_index], sizeof(char *) * (table->column_count + 1));
        if (new_row == NULL) {
            sp_set_error(error_buffer, error_buffer_size, "메모리 할당에 실패했습니다.");
            return false;
        }

        table->rows[row_index] = new_row;
        table->rows[row_index][table->column_count] = sp_strdup("");
        if (table->rows[row_index][table->column_count] == NULL) {
            sp_set_error(error_buffer, error_buffer_size, "메모리 할당에 실패했습니다.");
            return false;
        }
    }

    table->column_count++;
    return true;
}

static bool append_row(TableData *table,
                       char **row,
                       char *error_buffer,
                       size_t error_buffer_size) {
    char ***rows = (char ***)realloc(table->rows, sizeof(char **) * (table->row_count + 1));

    if (rows == NULL) {
        sp_set_error(error_buffer, error_buffer_size, "메모리 할당에 실패했습니다.");
        return false;
    }

    rows[table->row_count] = row;
    table->rows = rows;
    table->row_count++;
    return true;
}

static bool contains_duplicate_columns(char **columns, size_t column_count) {
    size_t left;
    size_t right;

    for (left = 0; left < column_count; left++) {
        for (right = left + 1; right < column_count; right++) {
            if (sp_equals_ignore_case(columns[left], columns[right])) {
                return true;
            }
        }
    }

    return false;
}

static bool execute_insert(const InsertStatement *statement,
                           const char *data_root,
                           char *error_buffer,
                           size_t error_buffer_size) {
    TableData table;
    char **row = NULL;
    size_t index;

    memset(&table, 0, sizeof(table));

    if (!storage_load_table(data_root,
                            statement->target.schema,
                            statement->target.table,
                            true,
                            &table,
                            error_buffer,
                            error_buffer_size)) {
        return false;
    }

    if (statement->has_column_list && contains_duplicate_columns(statement->columns, statement->column_count)) {
        storage_free_table(&table);
        sp_set_error(error_buffer, error_buffer_size, "INSERT 문에 중복 컬럼이 있습니다.");
        return false;
    }

    if (!statement->has_column_list) {
        if (table.column_count == 0) {
            storage_free_table(&table);
            sp_set_error(error_buffer, error_buffer_size, "컬럼 목록 없는 INSERT는 기존 테이블 헤더가 필요합니다.");
            return false;
        }

        if (statement->value_count != table.column_count) {
            storage_free_table(&table);
            sp_set_error(error_buffer, error_buffer_size, "값 수가 테이블 컬럼 수와 다릅니다.");
            return false;
        }

        row = (char **)calloc(table.column_count, sizeof(char *));
        if (row == NULL) {
            storage_free_table(&table);
            sp_set_error(error_buffer, error_buffer_size, "메모리 할당에 실패했습니다.");
            return false;
        }

        for (index = 0; index < table.column_count; index++) {
            row[index] = sp_strdup(statement->values[index]);
            if (row[index] == NULL) {
                free_row(row, table.column_count);
                storage_free_table(&table);
                sp_set_error(error_buffer, error_buffer_size, "메모리 할당에 실패했습니다.");
                return false;
            }
        }
    } else {
        if (table.column_count == 0) {
            table.columns = (char **)calloc(statement->column_count, sizeof(char *));
            if (statement->column_count > 0 && table.columns == NULL) {
                storage_free_table(&table);
                sp_set_error(error_buffer, error_buffer_size, "메모리 할당에 실패했습니다.");
                return false;
            }

            for (index = 0; index < statement->column_count; index++) {
                table.columns[index] = sp_strdup(statement->columns[index]);
                if (table.columns[index] == NULL) {
                    storage_free_table(&table);
                    sp_set_error(error_buffer, error_buffer_size, "메모리 할당에 실패했습니다.");
                    return false;
                }
            }
            table.column_count = statement->column_count;
        } else {
            for (index = 0; index < statement->column_count; index++) {
                if (find_column_index(&table, statement->columns[index]) == -1) {
                    if (!append_column(&table, statement->columns[index], error_buffer, error_buffer_size)) {
                        storage_free_table(&table);
                        return false;
                    }
                }
            }
        }

        row = (char **)calloc(table.column_count, sizeof(char *));
        if (row == NULL) {
            storage_free_table(&table);
            sp_set_error(error_buffer, error_buffer_size, "메모리 할당에 실패했습니다.");
            return false;
        }

        for (index = 0; index < table.column_count; index++) {
            row[index] = sp_strdup("");
            if (row[index] == NULL) {
                free_row(row, table.column_count);
                storage_free_table(&table);
                sp_set_error(error_buffer, error_buffer_size, "메모리 할당에 실패했습니다.");
                return false;
            }
        }

        for (index = 0; index < statement->column_count; index++) {
            ssize_t column_index = find_column_index(&table, statement->columns[index]);

            if (column_index < 0) {
                free_row(row, table.column_count);
                storage_free_table(&table);
                sp_set_error(error_buffer, error_buffer_size, "컬럼 인덱스를 찾지 못했습니다.");
                return false;
            }

            free(row[column_index]);
            row[column_index] = sp_strdup(statement->values[index]);
            if (row[column_index] == NULL) {
                free_row(row, table.column_count);
                storage_free_table(&table);
                sp_set_error(error_buffer, error_buffer_size, "메모리 할당에 실패했습니다.");
                return false;
            }
        }
    }

    if (!append_row(&table, row, error_buffer, error_buffer_size)) {
        free_row(row, table.column_count);
        storage_free_table(&table);
        return false;
    }

    if (!storage_save_table(data_root,
                            statement->target.schema,
                            statement->target.table,
                            &table,
                            error_buffer,
                            error_buffer_size)) {
        storage_free_table(&table);
        return false;
    }

    printf("INSERT 1 %s.%s\n", statement->target.schema, statement->target.table);
    storage_free_table(&table);
    return true;
}

static void print_projection_row(const TableData *table, const size_t *projection, size_t projection_count, size_t row_index) {
    size_t column_index;

    for (column_index = 0; column_index < projection_count; column_index++) {
        if (column_index > 0) {
            printf(" | ");
        }
        printf("%s", table->rows[row_index][projection[column_index]]);
    }
    printf("\n");
}

static bool row_matches_where(const TableData *table,
                              const SelectStatement *statement,
                              size_t row_index,
                              size_t *where_column_index,
                              bool *out_matches,
                              char *error_buffer,
                              size_t error_buffer_size) {
    if (!statement->has_where_clause) {
        *out_matches = true;
        return true;
    }

    if (*where_column_index == (size_t)-1) {
        ssize_t resolved_index = find_column_index(table, statement->where_clause.column);
        if (resolved_index < 0) {
            sp_set_error(error_buffer, error_buffer_size, "WHERE 절의 컬럼이 존재하지 않습니다: %s", statement->where_clause.column);
            return false;
        }
        *where_column_index = (size_t)resolved_index;
    }

    *out_matches = strcmp(table->rows[row_index][*where_column_index], statement->where_clause.value) == 0;
    return true;
}

static bool execute_select(const SelectStatement *statement,
                           const char *data_root,
                           char *error_buffer,
                           size_t error_buffer_size) {
    TableData table;
    size_t *projection = NULL;
    size_t projection_count = 0;
    size_t index;
    size_t matched_row_count = 0;
    size_t where_column_index = (size_t)-1;

    memset(&table, 0, sizeof(table));

    if (!storage_load_table(data_root,
                            statement->source.schema,
                            statement->source.table,
                            false,
                            &table,
                            error_buffer,
                            error_buffer_size)) {
        return false;
    }

    if (table.column_count == 0) {
        storage_free_table(&table);
        sp_set_error(error_buffer, error_buffer_size, "SELECT 할 컬럼 헤더가 없습니다.");
        return false;
    }

    if (statement->select_all) {
        projection_count = table.column_count;
        projection = (size_t *)calloc(projection_count, sizeof(size_t));
        if (projection == NULL) {
            storage_free_table(&table);
            sp_set_error(error_buffer, error_buffer_size, "메모리 할당에 실패했습니다.");
            return false;
        }

        for (index = 0; index < projection_count; index++) {
            projection[index] = index;
        }
    } else {
        projection_count = statement->column_count;
        projection = (size_t *)calloc(projection_count, sizeof(size_t));
        if (projection == NULL) {
            storage_free_table(&table);
            sp_set_error(error_buffer, error_buffer_size, "메모리 할당에 실패했습니다.");
            return false;
        }

        for (index = 0; index < projection_count; index++) {
            ssize_t column_index = find_column_index(&table, statement->columns[index]);
            if (column_index < 0) {
                free(projection);
                storage_free_table(&table);
                sp_set_error(error_buffer, error_buffer_size, "존재하지 않는 컬럼입니다: %s", statement->columns[index]);
                return false;
            }
            projection[index] = (size_t)column_index;
        }
    }

    printf("RESULT %s.%s\n", statement->source.schema, statement->source.table);
    for (index = 0; index < projection_count; index++) {
        if (index > 0) {
            printf(" | ");
        }
        printf("%s", table.columns[projection[index]]);
    }
    printf("\n");

    for (index = 0; index < table.row_count; index++) {
        bool is_match = false;

        if (!row_matches_where(&table,
                               statement,
                               index,
                               &where_column_index,
                               &is_match,
                               error_buffer,
                               error_buffer_size)) {
            free(projection);
            storage_free_table(&table);
            return false;
        }

        if (!is_match) {
            continue;
        }

        print_projection_row(&table, projection, projection_count, index);
        matched_row_count++;
    }
    printf("(%zu rows)\n", matched_row_count);

    free(projection);
    storage_free_table(&table);
    return true;
}

bool execute_statements(const StatementList *statements,
                        const char *data_root,
                        char *error_buffer,
                        size_t error_buffer_size) {
    size_t index;

    for (index = 0; index < statements->count; index++) {
        const Statement *statement = &statements->items[index];

        if (statement->type == STATEMENT_INSERT) {
            if (!execute_insert(&statement->as.insert_statement, data_root, error_buffer, error_buffer_size)) {
                return false;
            }
        } else if (statement->type == STATEMENT_SELECT) {
            if (!execute_select(&statement->as.select_statement, data_root, error_buffer, error_buffer_size)) {
                return false;
            }
        }
    }

    return true;
}
