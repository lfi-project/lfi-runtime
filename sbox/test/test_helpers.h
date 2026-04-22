#pragma once

#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

// -- Struct definitions (must match testlib.c) --

struct Point {
    int x;
    int y;
};

struct Complex {
    double real;
    double imag;
};

struct NamedArray {
    char* name;
    int* values;
    int count;
};

// -- Test runner macros (TAP output) --

static int tests_run = 0;
static const char* current_test_name = nullptr;

#define TEST(name)                \
    do {                          \
        tests_run++;              \
        current_test_name = name; \
    } while (0)

#define PASS()                                              \
    do {                                                    \
        printf("ok %d - %s\n", tests_run, current_test_name); \
    } while (0)

#define TEST_SUMMARY()                    \
    do {                                  \
        printf("1..%d\n", tests_run);     \
    } while (0)
