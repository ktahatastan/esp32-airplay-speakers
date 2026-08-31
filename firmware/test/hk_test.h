/**
 * @file hk_test.h
 * @brief Minimal host-side test harness.
 *
 * The point of these tests is that they run without ESP-IDF, on any machine,
 * in under a second. Every module they cover is deliberately written as pure C
 * so the logic can be checked long before hardware exists.
 *
 * Nothing here can prove a physical gate. A green run means the logic behaves;
 * it says nothing about a driver, a rail or a radio.
 */
#ifndef HK_TEST_H
#define HK_TEST_H

#include <stdio.h>
#include <string.h>

extern int hk_test_failures;
extern int hk_test_checks;

#define HK_CHECK(condition)                                                       \
    do {                                                                          \
        hk_test_checks++;                                                         \
        if (!(condition)) {                                                       \
            hk_test_failures++;                                                   \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #condition);         \
        }                                                                         \
    } while (0)

#define HK_CHECK_EQ_INT(actual, expected)                                         \
    do {                                                                          \
        hk_test_checks++;                                                         \
        long long a_ = (long long)(actual);                                       \
        long long e_ = (long long)(expected);                                     \
        if (a_ != e_) {                                                           \
            hk_test_failures++;                                                   \
            printf("  FAIL %s:%d  %s == %s (got %lld, want %lld)\n",              \
                   __FILE__, __LINE__, #actual, #expected, a_, e_);               \
        }                                                                         \
    } while (0)

#define HK_CHECK_EQ_STR(actual, expected)                                         \
    do {                                                                          \
        hk_test_checks++;                                                         \
        const char *a_ = (actual);                                                \
        const char *e_ = (expected);                                              \
        if (a_ == NULL || strcmp(a_, e_) != 0) {                                  \
            hk_test_failures++;                                                   \
            printf("  FAIL %s:%d  %s (got \"%s\", want \"%s\")\n",                \
                   __FILE__, __LINE__, #actual, a_ ? a_ : "(null)", e_);          \
        }                                                                         \
    } while (0)

#define HK_RUN(test_function)                                                     \
    do {                                                                          \
        printf("- %s\n", #test_function);                                         \
        test_function();                                                          \
    } while (0)

void test_pins(void);
void test_identity(void);
void test_version(void);
void test_button(void);
void test_led(void);
void test_provision(void);
void test_schema(void);
void test_manifest(void);
void test_gate(void);
void test_ota(void);
void test_health(void);
void test_power(void);

#endif /* HK_TEST_H */
