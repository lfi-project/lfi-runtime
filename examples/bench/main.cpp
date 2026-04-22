#include <cstdio>
#include <ctime>
#include <thread>

#if defined(SBOX_LFI)
#include "sbox/lfi.h"
using SboxType = sbox::Sandbox<sbox::LFI>;
#else
#include "sbox/passthrough.h"
using SboxType = sbox::Sandbox<sbox::Passthrough>;
#endif

#define CALL_ITERATIONS 100000
#define THREAD_ITERATIONS 1000

static double now() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

// Callback function for sandbox to call
static int double_value(int x) {
    return x * 2;
}

int main() {
#if defined(SBOX_LFI)
    auto sbp = SboxType::create("./lib_bench.lfi");
    if (!sbp) { fprintf(stderr, "failed to create sandbox\n"); return 1; }
    auto& sandbox = *sbp;
    const char* backend_name = "lfi";
#else
    SboxType sandbox("./libbench.so");
    const char* backend_name = "passthrough";
#endif

    printf("Benchmark (%s backend)\n", backend_name);
    printf("==============================\n\n");

    // Benchmark 1: Basic function call
    {
        auto add = sandbox.fn<int(int, int)>("add");

        // Warmup
        for (int i = 0; i < 100; i++) {
            add(1, 2);
        }

        double start = now();
        for (int i = 0; i < CALL_ITERATIONS; i++) {
            int result = add(i, i + 1);
            (void) result;
        }
        double elapsed = now() - start;
        double per_call_us = (elapsed / CALL_ITERATIONS) * 1e6;

        printf("1. Basic function call:     %.3f us/call (%d iterations)\n",
               per_call_us, CALL_ITERATIONS);
    }

    // Benchmark 2: Function call with callback
    {
        auto call_with_cb = sandbox.fn<int(int, void*)>("call_with_callback");
        auto cb = sandbox.register_callback(double_value);

        // Warmup
        for (int i = 0; i < 100; i++) {
            call_with_cb(i, cb);
        }

        double start = now();
        for (int i = 0; i < CALL_ITERATIONS; i++) {
            int result = call_with_cb(i, cb);
            (void) result;
        }
        double elapsed = now() - start;
        double per_call_us = (elapsed / CALL_ITERATIONS) * 1e6;

        printf("2. Call with callback:      %.3f us/call (%d iterations)\n",
               per_call_us, CALL_ITERATIONS);
    }

    // Benchmark 3: New thread + function call + thread end
    {
        auto add = sandbox.fn<int(int, int)>("add");

        // Warmup
        for (int i = 0; i < 10; i++) {
            std::thread t([&]() {
                int result = add(1, 2);
                (void) result;
            });
            t.join();
        }

        double start = now();
        for (int i = 0; i < THREAD_ITERATIONS; i++) {
            std::thread t([&]() {
                int result = add(i, i + 1);
                (void) result;
            });
            t.join();
        }
        double elapsed = now() - start;
        double per_call_us = (elapsed / THREAD_ITERATIONS) * 1e6;

        printf("3. Thread + call + join:    %.3f us/call (%d iterations)\n",
               per_call_us, THREAD_ITERATIONS);
    }

    // Benchmark 4: Function call with multiple in/out arguments
    {
        // Warmup
        for (int i = 0; i < 100; i++) {
            auto ctx = sandbox.context();
            int a = i, b = i + 1;
            int sum, product;
            sandbox.call<void(const int*, const int*, int*, int*)>(
                ctx, "multi_inout", ctx.in(a), ctx.in(b), ctx.out(sum),
                ctx.out(product));
        }

        double start = now();
        for (int i = 0; i < CALL_ITERATIONS; i++) {
            auto ctx = sandbox.context();
            int a = i, b = i + 1;
            int sum, product;
            sandbox.call<void(const int*, const int*, int*, int*)>(
                ctx, "multi_inout", ctx.in(a), ctx.in(b), ctx.out(sum),
                ctx.out(product));
        }
        double elapsed = now() - start;
        double per_call_us = (elapsed / CALL_ITERATIONS) * 1e6;

        printf("4. Call with in/out args:   %.3f us/call (%d iterations)\n",
               per_call_us, CALL_ITERATIONS);
    }

    printf("\n");
    return 0;
}
