int add(int a, int b) {
    return a + b;
}

// Function with callback
typedef int (*callback_fn)(int);

int call_with_callback(int value, callback_fn cb) {
    return cb(value);
}

// Function with multiple in/out arguments
void multi_inout(const int* a, const int* b, int* sum, int* product) {
    *sum = *a + *b;
    *product = *a * *b;
}
