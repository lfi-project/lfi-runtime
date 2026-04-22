// Simple test library

#include <ctype.h>
#include <string.h>

// -- Basic arithmetic (int) --

int add(int a, int b) {
    return a + b;
}

int multiply(int a, int b) {
    return a * b;
}

// -- Type variety --

double add_double(double a, double b) {
    return a + b;
}

float multiply_float(float a, float b) {
    return a * b;
}

long long add_long_long(long long a, long long b) {
    return a + b;
}

unsigned int add_unsigned(unsigned int a, unsigned int b) {
    return a + b;
}

int negate(int x) {
    return -x;
}

// -- Many parameters --

int sum6(int a, int b, int c, int d, int e, int f) {
    return a + b + c + d + e + f;
}

double weighted_sum(double a, double b, double c, double wa, double wb,
                    double wc) {
    return a * wa + b * wb + c * wc;
}

// -- Structs by pointer --

typedef struct {
    int x;
    int y;
} Point;

void point_init(Point* p, int x, int y) {
    p->x = x;
    p->y = y;
}

int point_sum(Point* p) {
    return p->x + p->y;
}

void point_scale(Point* p, int factor) {
    p->x *= factor;
    p->y *= factor;
}

typedef struct {
    double real;
    double imag;
} Complex;

double complex_magnitude_sq(Complex* c) {
    return c->real * c->real + c->imag * c->imag;
}

// -- Strings --

char* process_string(char* s) {
    // Just return the string as-is for now
    return s;
}

int string_length(const char* s) {
    return (int) strlen(s);
}

void string_to_upper(char* s) {
    for (; *s; s++) {
        *s = (char) toupper((unsigned char) *s);
    }
}

// -- Void function --

static int void_fn_called = 0;

void noop(void) {
    void_fn_called = 1;
}

int was_noop_called(void) {
    int r = void_fn_called;
    void_fn_called = 0;
    return r;
}

// -- Callbacks --

typedef void (*callback_t)(int);
static callback_t stored_callback = 0;

void set_callback(callback_t cb) {
    stored_callback = cb;
}

void trigger_callback(int value) {
    if (stored_callback) {
        stored_callback(value);
    }
}

typedef int (*binary_callback_t)(int, int);

int apply_binary_callback(binary_callback_t cb, int a, int b) {
    return cb(a, b);
}

typedef double (*double_callback_t)(double);

double apply_double_callback(double_callback_t cb, double val) {
    return cb(val);
}

// -- Many parameters (up to 10 = PBOX_MAX_ARGS) --

int sum8(int a, int b, int c, int d, int e, int f, int g, int h) {
    return a + b + c + d + e + f + g + h;
}

double sum8_double(double a, double b, double c, double d, double e, double f,
                   double g, double h) {
    return a + b + c + d + e + f + g + h;
}

int sum9(int a, int b, int c, int d, int e, int f, int g, int h, int i) {
    return a + b + c + d + e + f + g + h + i;
}

int sum10(int a, int b, int c, int d, int e, int f, int g, int h, int i,
          int j) {
    return a + b + c + d + e + f + g + h + i + j;
}

double sum10_double(double a, double b, double c, double d, double e, double f,
                    double g, double h, double i, double j) {
    return a + b + c + d + e + f + g + h + i + j;
}

// Mixed int/double to exercise both register files
double mixed10(int a, double b, int c, double d, int e, double f, int g,
               double h, int i, double j) {
    return a + b + c + d + e + f + g + h + i + j;
}

// -- Pointer write + read-back --

void write_int(int* p, int value) {
    *p = value;
}

int read_int(int* p) {
    return *p;
}

void swap_ints(int* a, int* b) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

// -- Struct with pointer fields --

typedef struct {
    char* name;
    int* values;
    int count;
} NamedArray;

void named_array_init(NamedArray* na, char* name, int* values, int count) {
    na->name = name;
    na->values = values;
    na->count = count;
}

int named_array_sum(NamedArray* na) {
    int total = 0;
    for (int i = 0; i < na->count; i++) {
        total += na->values[i];
    }
    return total;
}

int named_array_name_len(NamedArray* na) {
    return (int) strlen(na->name);
}

// -- Re-entrant callback --

typedef int (*reentrant_callback_t)(int);
static reentrant_callback_t stored_reentrant_cb = 0;

void set_reentrant_callback(reentrant_callback_t cb) {
    stored_reentrant_cb = cb;
}

// Calls the callback, then adds 10 to its result.
// If the callback itself calls back into the sandbox (e.g. add()),
// this tests re-entrancy.
int call_reentrant(int value) {
    if (stored_reentrant_cb) {
        return stored_reentrant_cb(value) + 10;
    }
    return value;
}

// -- Callback with 4 params --

typedef int (*quad_callback_t)(int, int, int, int);

int apply_quad_callback(quad_callback_t cb, int a, int b, int c, int d) {
    return cb(a, b, c, d);
}

// -- Callbacks with many params (test stack arg passing) --

typedef int (*callback9_t)(int, int, int, int, int, int, int, int, int);

int apply_callback9(callback9_t cb, int a, int b, int c, int d, int e, int f,
                    int g, int h, int i) {
    return cb(a, b, c, d, e, f, g, h, i);
}

typedef int (*callback8_t)(int, int, int, int, int, int, int, int);

int apply_callback8(callback8_t cb, int a, int b, int c, int d, int e, int f,
                    int g, int h) {
    return cb(a, b, c, d, e, f, g, h);
}

// -- Callback with pointer argument --

typedef int (*ptr_callback_t)(int*, int);

int apply_ptr_callback(ptr_callback_t cb, int* data, int count) {
    return cb(data, count);
}

// -- Callback with double* argument --

typedef double (*double_ptr_callback_t)(double*, int);

double apply_double_ptr_callback(double_ptr_callback_t cb, double* data,
                                 int count) {
    return cb(data, count);
}

// -- Callback that mutates data through pointer --

typedef void (*mutate_callback_t)(int*, int);

void apply_mutate_callback(mutate_callback_t cb, int* data, int count) {
    cb(data, count);
}

// -- Memory pattern --

void fill_ints(int* arr, int count, int value) {
    for (int i = 0; i < count; i++) {
        arr[i] = value + i;
    }
}

int sum_ints(int* arr, int count) {
    int total = 0;
    for (int i = 0; i < count; i++) {
        total += arr[i];
    }
    return total;
}

// -- Thread-local storage (for threading test) --

static __thread int tls_value = 0;

void set_tls(int value) {
    tls_value = value;
}

int get_tls(void) {
    return tls_value;
}

int increment_tls(void) {
    return ++tls_value;
}
