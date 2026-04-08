#ifndef STORAGE_H
#define STORAGE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct TableData {
    size_t column_count;
    char **columns;
    size_t row_count;
    char ***rows;
} TableData;

bool storage_load_table(const char *data_root,
                        const char *schema,
                        const char *table_name,
                        bool allow_missing,
                        TableData *out_table,
                        char *error_buffer,
                        size_t error_buffer_size);

bool storage_save_table(const char *data_root,
                        const char *schema,
                        const char *table_name,
                        const TableData *table,
                        char *error_buffer,
                        size_t error_buffer_size);

void storage_free_table(TableData *table);

#endif
