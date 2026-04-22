int add(int a, int b) {
    return a + b;
}

// Simulate some work
int slow_add(int a, int b) {
    volatile int sum = 0;
    for (int i = 0; i < 1000000; i++) {
        sum += 1;
    }
    return a + b;
}

// Thread-local storage
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
