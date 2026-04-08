#include "test_framework.h"

#include "sqlproc/catalog.h"
#include "sqlproc/storage.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

char *mkdtemp(char *template);

typedef struct {
    size_t count;
    Row last_row;
} RowCollector;

static void make_dir(const char *path) {
    ASSERT_TRUE(mkdir(path, 0777) == 0);
}

static void write_text_file(const char *path, const char *contents) {
    FILE *file = fopen(path, "w");
    ASSERT_TRUE(file != NULL);
    ASSERT_INT_EQ((long long)strlen(contents), (long long)fwrite(contents, 1U, strlen(contents), file));
    ASSERT_TRUE(fclose(file) == 0);
}

static SqlStatus collect_last_row(const Row *row, void *ctx, SqlError *err) {
    RowCollector *collector = (RowCollector *)ctx;

    (void)err;
    row_free(&collector->last_row);
    ASSERT_STATUS_OK(row_clone(row, &collector->last_row, err));
    collector->count++;
    return SQL_STATUS_OK;
}

static void test_storage_round_trips_escaped_text_rows(void) {
    char temp_template[] = "build/storage-test-XXXXXX";
    char schema_dir[128];
    char data_dir[128];
    char db_root[128];
    Catalog catalog;
    TableSchema schema;
    Row row;
    RowCollector collector;
    SqlError err;
    SqlStatus status;

    ASSERT_TRUE(mkdtemp(temp_template) != NULL);
    snprintf(db_root, sizeof(db_root), "%s", temp_template);
    snprintf(schema_dir, sizeof(schema_dir), "%s/schema", db_root);
    snprintf(data_dir, sizeof(data_dir), "%s/data", db_root);
    make_dir(schema_dir);
    make_dir(data_dir);

    {
        char schema_path[160];
        snprintf(schema_path, sizeof(schema_path), "%s/users.schema", schema_dir);
        write_text_file(schema_path,
                        "version 1\n"
                        "table users\n"
                        "column id INT\n"
                        "column name TEXT\n"
                        "column active BOOL\n");
    }

    catalog_init(&catalog, db_root);
    table_schema_init(&schema);
    row_init(&row);
    row_init(&collector.last_row);
    collector.count = 0U;

    status = catalog_load_table(catalog.db_root, "users", &schema, &err);
    ASSERT_STATUS_OK(status);

    row.value_count = 3U;
    row.values = calloc(3U, sizeof(Value));
    ASSERT_TRUE(row.values != NULL);
    row.values[0] = value_make_int(10);
    ASSERT_STATUS_OK(value_make_text(&row.values[1], "A\tB\nC\\D", &err));
    row.values[2] = value_make_bool(1);

    status = storage_append_row(db_root, &schema, &row, &err);
    ASSERT_STATUS_OK(status);
    status = storage_scan_rows(db_root, &schema, collect_last_row, &collector, &err);
    ASSERT_STATUS_OK(status);

    ASSERT_INT_EQ(1, collector.count);
    ASSERT_INT_EQ(10, collector.last_row.values[0].as.int_value);
    ASSERT_STR_EQ("A\tB\nC\\D", collector.last_row.values[1].as.text_value);
    ASSERT_INT_EQ(1, collector.last_row.values[2].as.bool_value);

    row_free(&collector.last_row);
    row_free(&row);
    table_schema_free(&schema);
}

void register_storage_tests(TestSuite *suite) {
    test_suite_add(suite, "storage round-trips escaped TSV rows", test_storage_round_trips_escaped_text_rows);
}
