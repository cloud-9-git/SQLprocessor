#ifndef SQLPROC_TEST_FRAMEWORK_H
#define SQLPROC_TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void (*TestFn)(void);

typedef struct {
    const char *name;
    TestFn fn;
} TestCase;

typedef struct {
    size_t count;
    TestCase cases[128];
} TestSuite;

void test_suite_add(TestSuite *suite, const char *name, TestFn fn);
void test_fail_impl(const char *file, int line, const char *fmt, ...);

#define ASSERT_TRUE(condition)                                                                 \
    do {                                                                                       \
        if (!(condition)) {                                                                    \
            test_fail_impl(__FILE__, __LINE__, "assertion failed: %s", #condition);          \
        }                                                                                      \
    } while (0)

#define ASSERT_INT_EQ(expected, actual)                                                        \
    do {                                                                                       \
        long long expected_value__ = (long long)(expected);                                    \
        long long actual_value__ = (long long)(actual);                                        \
        if (expected_value__ != actual_value__) {                                              \
            test_fail_impl(__FILE__, __LINE__, "expected %lld but got %lld",                 \
                           expected_value__, actual_value__);                                  \
        }                                                                                      \
    } while (0)

#define ASSERT_STR_EQ(expected, actual)                                                        \
    do {                                                                                       \
        const char *expected_value__ = (expected);                                             \
        const char *actual_value__ = (actual);                                                 \
        if (((expected_value__) == NULL) != ((actual_value__) == NULL) ||                      \
            ((expected_value__) != NULL && strcmp(expected_value__, actual_value__) != 0)) {   \
            test_fail_impl(__FILE__, __LINE__, "expected \"%s\" but got \"%s\"",            \
                           expected_value__ == NULL ? "(null)" : expected_value__,             \
                           actual_value__ == NULL ? "(null)" : actual_value__);                \
        }                                                                                      \
    } while (0)

#define ASSERT_STATUS_OK(status)                                                               \
    do {                                                                                       \
        if ((status) != 0) {                                                                   \
            test_fail_impl(__FILE__, __LINE__, "expected SQL_STATUS_OK but got %d",          \
                           (int)(status));                                                     \
        }                                                                                      \
    } while (0)

#endif
