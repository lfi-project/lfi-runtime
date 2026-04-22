#include <cassert>
#include <cstdio>

#define SBOX_STATIC
#include "sbox/passthrough.h"

#include "lib_static.h"

int main() {
    // Static mode - no library path needed
    sbox::Sandbox<sbox::Passthrough> sandbox;

    printf("Using static backend\n\n");

    // Basic function call
    int result = sandbox.call<int(int, int)>(SBOX_FN(add), 10, 32);
    printf("add(10, 32) = %d (expected 42)\n", result);
    assert(result == 42);

    // Out parameter
    {
        auto ctx = sandbox.context();
        int value;
        sandbox.call<void(int*)>(ctx, SBOX_FN(get_value), ctx.out(value));
        printf("get_value() = %d (expected 42)\n", value);
        assert(value == 42);
    }

    // In parameter
    {
        auto ctx = sandbox.context();
        int value = 21;
        int result = sandbox.call<int(const int*)>(ctx, SBOX_FN(double_value),
                                                   ctx.in(value));
        printf("double_value(21) = %d (expected 42)\n", result);
        assert(result == 42);
    }

    // InOut parameter
    {
        auto ctx = sandbox.context();
        int counter = 5;
        sandbox.call<void(int*)>(ctx, SBOX_FN(increment), ctx.inout(counter));
        printf("increment(5) = %d (expected 6)\n", counter);
        assert(counter == 6);
    }

    // Function handle
    auto add_fn = sandbox.fn<int(int, int)>(SBOX_FN(add));
    result = add_fn(100, 200);
    printf("add_fn(100, 200) = %d (expected 300)\n", result);
    assert(result == 300);

    printf("\nAll tests passed!\n");
    return 0;
}
