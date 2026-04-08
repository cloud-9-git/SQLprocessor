#include "test_framework.h"

#include "sqlproc/binder.h"
#include "sqlproc/catalog.h"
#include "sqlproc/parser.h"

static void test_binder_maps_insert_values_to_schema_order(void) {
    const char *sql = "INSERT INTO users (name, active, id) VALUES ('Alice', true, 7);";
    Catalog catalog;
    AstScript ast;
    BoundScript bound;
    SqlError err;
    SqlStatus status;

    ast_script_init(&ast);
    bound_script_init(&bound);
    catalog_init(&catalog, "tests/fixtures/db");

    status = parser_parse_script(sql, &ast, &err);
    ASSERT_STATUS_OK(status);
    status = binder_bind_script(&catalog, &ast, &bound, &err);
    ASSERT_STATUS_OK(status);

    ASSERT_INT_EQ(1, bound.statement_count);
    ASSERT_INT_EQ(BOUND_STATEMENT_INSERT, bound.statements[0].kind);
    ASSERT_INT_EQ(3, bound.statements[0].as.insert_stmt.row.value_count);
    ASSERT_INT_EQ(7, bound.statements[0].as.insert_stmt.row.values[0].as.int_value);
    ASSERT_STR_EQ("Alice", bound.statements[0].as.insert_stmt.row.values[1].as.text_value);
    ASSERT_INT_EQ(1, bound.statements[0].as.insert_stmt.row.values[2].as.bool_value);

    ast_script_free(&ast);
    bound_script_free(&bound);
}

static void test_binder_rejects_wrong_literal_type(void) {
    const char *sql = "SELECT * FROM users WHERE id = 'wrong';";
    Catalog catalog;
    AstScript ast;
    BoundScript bound;
    SqlError err;
    SqlStatus status;

    ast_script_init(&ast);
    bound_script_init(&bound);
    catalog_init(&catalog, "tests/fixtures/db");

    status = parser_parse_script(sql, &ast, &err);
    ASSERT_STATUS_OK(status);
    status = binder_bind_script(&catalog, &ast, &bound, &err);
    ASSERT_TRUE(status != SQL_STATUS_OK);
    ASSERT_TRUE(strstr(err.message, "type mismatch") != NULL);

    ast_script_free(&ast);
    bound_script_free(&bound);
}

void register_binder_tests(TestSuite *suite) {
    test_suite_add(suite, "binder aligns INSERT values to schema order", test_binder_maps_insert_values_to_schema_order);
    test_suite_add(suite, "binder rejects mismatched WHERE literal types", test_binder_rejects_wrong_literal_type);
}
