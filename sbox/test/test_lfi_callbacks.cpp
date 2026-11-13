#if defined(__x86_64__)
#define SBOX_SKIP_8_ARG_CALLBACK
#endif
#define SBOX_SKIP_9_ARG_CALLBACK
#include "sbox/lfi.h"

using SboxType = sbox::LFI;

#include "test_callbacks.h"
#include "test_helpers.h"

// Callbacks that re-enter the sandbox, including calls with stack arguments
// and nested CallContexts.

static int reentrant_add(sbox::Sandbox<sbox::LFI>& sandbox, int value) {
    return sandbox.call<int(int, int)>("add", value, 100);
}

static int reentrant_stack_args(sbox::Sandbox<sbox::LFI>& sandbox,
                                int value) {
    return sandbox.call<int(int, int, int, int, int, int, int, int, int)>(
        "sum9", value, 1, 1, 1, 1, 1, 1, 1, 1);
}

static int reentrant_context(sbox::Sandbox<sbox::LFI>& sandbox, int value) {
    auto ctx = sandbox.context();
    int out = 0;
    sandbox.call<void(int*, int)>(ctx, "write_int", ctx.out(out), value * 2);
    return out;
}

int main() {
    auto sb = sbox::Sandbox<SboxType>::create("./testlib.lfi");
    assert(sb);
    auto& sandbox = *sb;
#include "test_callbacks.inc.cpp"

    TEST("callback calls back into sandbox");
    auto add_cb = sandbox.register_callback<reentrant_add>();
    sandbox.call<void(int (*)(int))>("set_reentrant_callback", add_cb);
    assert(sandbox.call<int(int)>("call_reentrant", 5) == 115);
    assert(sandbox.call<int(int)>("call_reentrant", -50) == 60);
    PASS();

    TEST("callback re-enters with stack arguments");
    auto stack_cb = sandbox.register_callback<reentrant_stack_args>();
    sandbox.call<void(int (*)(int))>("set_reentrant_callback", stack_cb);
    assert(sandbox.call<int(int)>("call_reentrant", 5) == 23);
    PASS();

    TEST("callback re-enters with a nested CallContext");
    auto ctx_cb = sandbox.register_callback<reentrant_context>();
    sandbox.call<void(int (*)(int))>("set_reentrant_callback", ctx_cb);
    {
        // The outer value stays on the sandbox stack while the callback's
        // context pushes and pops above it.
        auto ctx = sandbox.context();
        int outer = 1234;
        auto outer_ptr = ctx.inout(outer);
        int v = 0;
        int r = sandbox.call<int(int)>(ctx, "call_reentrant", 21);
        assert(r == 52);
        assert(sandbox.call<int(const int*)>(ctx, "read_const_int",
                                             outer_ptr) == 1234);
        sandbox.call<void(int*, int)>(ctx, "write_int", ctx.out(v), 7);
        assert(v == 7);
        assert(outer == 1234);
    }
    PASS();

    TEST_SUMMARY();
}
