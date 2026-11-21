/*
 * test.h - tiny zero-dependency test harness for mini3
 *
 * Each test file is a TU with exactly one main(). Tests call CHECK(); the
 * harness reports pass/fail counts and exits non-zero on any failure.
 */
#ifndef M3_TEST_H
#define M3_TEST_H

#include <stdio.h>
#include <math.h>

static int g_test_pass = 0;
static int g_test_fail = 0;

#define CHECK(cond) do { \
    if (cond) { g_test_pass++; } \
    else { \
        g_test_fail++; \
        fprintf(stderr, "    FAIL: %s:%d: check(%s)\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

#define CHECK_NEAR(a, b, eps) do { \
    float _a = (a), _b = (b); \
    if (fabsf(_a - _b) <= (eps)) { g_test_pass++; } \
    else { \
        g_test_fail++; \
        fprintf(stderr, "    FAIL: %s:%d: check(%s != %s): %f vs %f\n", __FILE__, __LINE__, \
                #a, #b, (double)_a, (double)_b); \
    } \
} while (0)

#define TEST(name) static void test_##name(void)

#define RUN(name) test_##name()

#define TEST_MAIN(...) int main(void) { \
    fprintf(stderr, "## %s ==\n", __FILE__); \
    do { __VA_ARGS__; } while (0); \
    fprintf(stderr, "## %s done: %d pass, %d fail\n", __FILE__, g_test_pass, g_test_fail); \
    if (g_test_fail > 0) return 1; \
    return 0; \
}

#endif /* M3_TEST_H */
