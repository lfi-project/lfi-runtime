// Functions that are statically linked into the executable

int add(int a, int b) {
    return a + b;
}

void get_value(int* out) {
    *out = 42;
}

int double_value(const int* in) {
    return *in * 2;
}

void increment(int* inout) {
    (*inout)++;
}
