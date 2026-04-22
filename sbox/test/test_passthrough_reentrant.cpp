#include "sbox/passthrough.h"
#include "test_helpers.h"

static sbox::Sandbox<sbox::Passthrough>* g_sandbox;

// Re-entrant callback: calls back into sandbox during callback execution
static int reentrant_callback(int value) {
    return g_sandbox->call<int(int, int)>("add", value, 100);
}

// Simple callback for stress test
static int stress_add_callback(int a, int b) {
    return a + b;
}

int main() {
    sbox::Sandbox<sbox::Passthrough> sandbox("./libtestlib.so");
    g_sandbox = &sandbox;


    TEST("callback calls back into sandbox");
    // Register a callback that itself calls sandbox.call("add", value, 100)
    auto cb = sandbox.register_callback(reentrant_callback);
    sandbox.call<void(int (*)(int))>("set_reentrant_callback", cb);
    // call_reentrant(5) -> reentrant_callback(5) -> add(5, 100) = 105 -> +10 =
    // 115
    int result = sandbox.call<int(int)>("call_reentrant", 5);
    assert(result == 115);
    PASS();

    TEST("re-entrant callback with different values");
    // call_reentrant(0) -> reentrant_callback(0) -> add(0, 100) = 100 -> +10 =
    // 110
    result = sandbox.call<int(int)>("call_reentrant", 0);
    assert(result == 110);
    PASS();

    TEST("re-entrant callback with negative value");
    // call_reentrant(-50) -> reentrant_callback(-50) -> add(-50, 100) = 50 ->
    // +10 = 60
    result = sandbox.call<int(int)>("call_reentrant", -50);
    assert(result == 60);
    PASS();


    TEST("register callback 64 times");
    for (int i = 0; i < 64; i++) {
        auto tmp_cb = sandbox.register_callback(stress_add_callback);
        assert(tmp_cb != nullptr);
        int r = sandbox.call<int(int (*)(int, int), int, int)>(
            "apply_binary_callback", tmp_cb, i, 1);
        assert(r == i + 1);
    }
    PASS();

    TEST_SUMMARY();
}
