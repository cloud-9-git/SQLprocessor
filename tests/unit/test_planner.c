#include "test_framework.h"

#include "sqlproc/binder.h"
#include "sqlproc/catalog.h"
#include "sqlproc/parser.h"
#include "sqlproc/planner.h"

static void test_planner_builds_project_filter_scan_chain(void) {
    const char *sql = "SELECT name FROM users WHERE active = true;";
    Catalog catalog;
    AstScript ast;
    BoundScript bound;
    PlanScript plan;
    SqlError err;
    SqlStatus status;

    ast_script_init(&ast);
    bound_script_init(&bound);
    plan_script_init(&plan);
    catalog_init(&catalog, "tests/fixtures/db");

    status = parser_parse_script(sql, &ast, &err);
    ASSERT_STATUS_OK(status);
    status = binder_bind_script(&catalog, &ast, &bound, &err);
    ASSERT_STATUS_OK(status);
    status = planner_build_script(&bound, &plan, &err);
    ASSERT_STATUS_OK(status);

    ASSERT_INT_EQ(1, plan.statement_count);
    ASSERT_INT_EQ(PLAN_NODE_PROJECT, plan.statements[0].root->kind);
    ASSERT_INT_EQ(PLAN_NODE_FILTER, plan.statements[0].root->child->kind);
    ASSERT_INT_EQ(PLAN_NODE_SEQ_SCAN, plan.statements[0].root->child->child->kind);

    ast_script_free(&ast);
    bound_script_free(&bound);
    plan_script_free(&plan);
}

void register_planner_tests(TestSuite *suite) {
    test_suite_add(suite, "planner builds project-filter-scan plan", test_planner_builds_project_filter_scan_chain);
}
