#include "test_framework.h"

#include "sqlproc/binder.h"
#include "sqlproc/catalog.h"
#include "sqlproc/executor.h"
#include "sqlproc/parser.h"
#include "sqlproc/planner.h"

static void test_executor_returns_filtered_projection_rows(void) {
    const char *sql = "SELECT id, name FROM users WHERE active = true;";
    Catalog catalog;
    AstScript ast;
    BoundScript bound;
    PlanScript plan;
    ExecutionContext ctx;
    ExecutionOutput output;
    SqlError err;
    SqlStatus status;

    ast_script_init(&ast);
    bound_script_init(&bound);
    plan_script_init(&plan);
    execution_output_init(&output);

    catalog_init(&catalog, "tests/fixtures/db");
    status = parser_parse_script(sql, &ast, &err);
    ASSERT_STATUS_OK(status);
    status = binder_bind_script(&catalog, &ast, &bound, &err);
    ASSERT_STATUS_OK(status);
    status = planner_build_script(&bound, &plan, &err);
    ASSERT_STATUS_OK(status);

    ctx.db_root = "tests/fixtures/db";
    ctx.trace = 0;
    ctx.trace_stream = NULL;
    status = executor_run_script(&ctx, &plan, &output, &err);
    ASSERT_STATUS_OK(status);

    ASSERT_INT_EQ(1, output.result_count);
    ASSERT_INT_EQ(2, output.results[0].column_count);
    ASSERT_INT_EQ(2, output.results[0].row_count);
    ASSERT_INT_EQ(1, output.results[0].rows[0].values[0].as.int_value);
    ASSERT_STR_EQ("Alice", output.results[0].rows[0].values[1].as.text_value);
    ASSERT_INT_EQ(3, output.results[0].rows[1].values[0].as.int_value);
    ASSERT_STR_EQ("Charlie", output.results[0].rows[1].values[1].as.text_value);

    ast_script_free(&ast);
    bound_script_free(&bound);
    plan_script_free(&plan);
    execution_output_free(&output);
}

void register_executor_tests(TestSuite *suite) {
    test_suite_add(suite, "executor evaluates filtered projections", test_executor_returns_filtered_projection_rows);
}
