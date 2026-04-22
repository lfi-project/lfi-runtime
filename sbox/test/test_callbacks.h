#pragma once

// Callback functions and state used by test_callbacks.inc.cc

static int callback_value = 0;

static void my_callback(int x) {
    callback_value = x;
}

static int my_add_callback(int a, int b) {
    return a + b;
}

static int my_multiply_callback(int a, int b) {
    return a * b;
}

static double my_double_callback(double val) {
    return val * 2.0;
}

static double my_square_callback(double val) {
    return val * val;
}

static int my_quad_callback(int a, int b, int c, int d) {
    return a * b + c * d;
}

#ifndef SBOX_SKIP_STACK_ARG_CALLBACKS
static int my_sum8_callback(int a, int b, int c, int d, int e, int f, int g,
                             int h) {
    return a + b + c + d + e + f + g + h;
}

static int my_sum9_callback(int a, int b, int c, int d, int e, int f, int g,
                             int h, int i) {
    return a + b + c + d + e + f + g + h + i;
}
#endif

// Callback that receives a pointer from the sandbox.
// Takes sandbox as first parameter to verify the pointer.
// Uses sbox<int*> to enforce that the pointer is treated as untrusted.
static int my_ptr_sum_callback(sbox::Sandbox<SboxType>& sandbox,
                                sbox::sbox<int*> data, int count) {
    auto safe = sandbox.verify(data, count);
    int total = 0;
    for (int i = 0; i < count; i++) {
        total += safe[i];
    }
    return total;
}

// Callback that receives a double* from the sandbox and sums the array.
static double my_double_ptr_sum_callback(sbox::Sandbox<SboxType>& sandbox,
                                          sbox::sbox<double*> data, int count) {
    auto safe = sandbox.verify(data, count);
    double total = 0.0;
    for (int i = 0; i < count; i++) {
        total += safe[i];
    }
    return total;
}

// Callback that modifies data through a pointer (doubles each element).
static void my_mutate_callback(sbox::Sandbox<SboxType>& sandbox,
                                sbox::sbox<int*> data, int count) {
    auto safe = sandbox.verify(data, count);
    for (int i = 0; i < count; i++) {
        safe[i] *= 2;
    }
}
