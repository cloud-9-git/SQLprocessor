#include "test_framework.h"

#include <stdarg.h>

void register_lexer_tests(TestSuite *suite);
void register_parser_tests(TestSuite *suite);
void register_binder_tests(TestSuite *suite);
void register_planner_tests(TestSuite *suite);
void register_storage_tests(TestSuite *suite);
void register_executor_tests(TestSuite *suite);

void test_suite_add(TestSuite *suite, const char *name, TestFn fn) {
    ASSERT_TRUE(suite->count < sizeof(suite->cases) / sizeof(suite->cases[0]));
    suite->cases[suite->count].name = name;
    suite->cases[suite->count].fn = fn;
    suite->count++;
}

void test_fail_impl(const char *file, int line, const char *fmt, ...) {
    va_list args;

    fprintf(stderr, "TEST FAILURE at %s:%d: ", file, line);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
    exit(1);
}

int main(void) {
    TestSuite suite = {0};

    register_lexer_tests(&suite);
    register_parser_tests(&suite);
    register_binder_tests(&suite);
    register_planner_tests(&suite);
    register_storage_tests(&suite);
    register_executor_tests(&suite);

    for (size_t index = 0U; index < suite.count; index++) {
        suite.cases[index].fn();
        printf("[PASS] %s\n", suite.cases[index].name);
    }

    printf("%zu tests passed\n", suite.count);
    return 0;
}
