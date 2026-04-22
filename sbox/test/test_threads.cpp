#include "sbox/passthrough.h"

#include <pthread.h>
#include <cassert>
#include <cstdio>

static sbox::Sandbox<sbox::Passthrough>* sandbox;

#define NUM_THREADS 4
#define ITERATIONS 200

static void* thread_fn(void* arg) {
    int id = static_cast<int>(reinterpret_cast<intptr_t>(arg));

    // Set TLS to thread-specific value
    int tls_base = (id + 1) * 1000;
    sandbox->call<void(int)>("set_tls", tls_base);

    int tls_val = sandbox->call<int()>("get_tls");
    assert(tls_val == tls_base);

    for (int i = 0; i < ITERATIONS; i++) {
        int a = id * 1000 + i;
        int b = i;

        // Test integer calls
        int result = sandbox->call<int(int, int)>("add", a, b);
        assert(result == a + b);

        // Test double calls
        double dr = sandbox->call<double(double, double)>(
            "add_double", (double) a, (double) b);
        assert(dr == (double) (a + b));

        // Test TLS isolation between threads
        int new_tls = sandbox->call<int()>("increment_tls");
        assert(new_tls == tls_base + i + 1);
    }

    // Final TLS check - verify no corruption from other threads
    tls_val = sandbox->call<int()>("get_tls");
    assert(tls_val == tls_base + ITERATIONS);

    printf("Thread %d completed %d iterations (TLS: %d -> %d)\n", id,
           ITERATIONS, tls_base, tls_val);
    return nullptr;
}

int main() {
    sbox::Sandbox<sbox::Passthrough> sb("./libtestlib.so");
    sandbox = &sb;

    // Sanity check on main thread
    int result = sandbox->call<int(int, int)>("add", 10, 20);
    assert(result == 30);

    // Set TLS on main thread
    sandbox->call<void(int)>("set_tls", 42);
    assert(sandbox->call<int()>("get_tls") == 42);

    // Spawn threads
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], nullptr, thread_fn,
                       reinterpret_cast<void*>(static_cast<intptr_t>(i)));
    }

    // Wait for all threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], nullptr);
    }

    // Verify main thread TLS wasn't affected by other threads
    int tls = sandbox->call<int()>("get_tls");
    assert(tls == 42);

    printf("All %d threads completed successfully!\n", NUM_THREADS);
    return 0;
}
