#include "test_framework.h"

#include "sqlproc/diag.h"
#include "sqlproc/parser.h"

static void test_parser_parses_multi_statement_script(void) {
    const char *sql =
        "INSERT INTO users VALUES (1, 'Alice', true);"
        "SELECT id, name FROM users WHERE active = true;";
    AstScript script;
    SqlError err;
    SqlStatus status;

    ast_script_init(&script);
    status = parser_parse_script(sql, &script, &err);
    ASSERT_STATUS_OK(status);
    ASSERT_INT_EQ(2, script.statement_count);
    ASSERT_INT_EQ(STATEMENT_INSERT, script.statements[0].kind);
    ASSERT_INT_EQ(STATEMENT_SELECT, script.statements[1].kind);
    ASSERT_STR_EQ("users", script.statements[1].as.select_stmt.table_name);
    ASSERT_TRUE(script.statements[1].as.select_stmt.where_clause != NULL);
    ast_script_free(&script);
}

static void test_parser_reports_missing_semicolon(void) {
    AstScript script;
    SqlError err;
    SqlStatus status;

    ast_script_init(&script);
    status = parser_parse_script("SELECT * FROM users", &script, &err);
    ASSERT_TRUE(status != SQL_STATUS_OK);
    ASSERT_TRUE(strstr(err.message, "expected ';'") != NULL);
    ast_script_free(&script);
}

void register_parser_tests(TestSuite *suite) {
    test_suite_add(suite, "parser handles multi-statement scripts", test_parser_parses_multi_statement_script);
    test_suite_add(suite, "parser rejects missing semicolons", test_parser_reports_missing_semicolon);
}
