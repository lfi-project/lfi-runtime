#pragma once

// The tests make their sandbox calls inside assert, so it must stay enabled.
#undef NDEBUG
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

#if defined(__APPLE__)
#define TESTLIB  "./libtestlib.dylib"
#define TESTLIB2 "./libtestlib2.dylib"
#else
#define TESTLIB  "./libtestlib.so"
#define TESTLIB2 "./libtestlib2.so"
#endif

// Struct definitions; must match testlib.c.

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

// Declarations used with SBOX_FN; must match testlib.c.

extern "C" {
void write_int(int* p, int value);
void swap_ints(int* a, int* b);
int read_const_int(const int* p);
int negate(int x) noexcept;
void get_version_ptr(const char** out);
int apply_binary_callback(int (*cb)(int, int), int a, int b);
const int seven(void);
}

inline int context_add_cb(int a, int b) {
    return a + b;
}

// Test runner macros (TAP output).

int tests_run = 0;
const char* current_test_name = nullptr;

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
