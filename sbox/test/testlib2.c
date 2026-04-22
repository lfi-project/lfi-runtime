// Second test library for multi-sandbox tests

int multiply(int a, int b) {
    return a * b;
}

int square(int x) {
    return x * x;
}

void fill_ints(int* arr, int count, int value) {
    for (int i = 0; i < count; i++) {
        arr[i] = value + i;
    }
}
