// Two sandboxes A and B sharing a thread, where a callback registered on
// A performs a cross-sandbox call into B. Verifies that callbacks
// declared with Sandbox<Backend>& as their first parameter receive the
// correct sandbox reference even when control crosses sandbox boundaries
// mid-call.

#include "sbox/passthrough.h"

using SboxType = sbox::Passthrough;

#include "test_helpers.h"

static sbox::Sandbox<SboxType>* g_a;
static sbox::Sandbox<SboxType>* g_b;
static sbox::Sandbox<SboxType>* cb1_observed = nullptr;
static sbox::Sandbox<SboxType>* cb2_observed = nullptr;
static int cb1_calls = 0;
static int cb2_calls = 0;

static void cb_first(sbox::Sandbox<SboxType>& sandbox) {
    cb1_calls++;
    cb1_observed = &sandbox;
    int r = g_b->call<int(int, int)>("multiply", 6, 7);
    assert(r == 42);
}

static void cb_second(sbox::Sandbox<SboxType>& sandbox) {
    cb2_calls++;
    cb2_observed = &sandbox;
}

int main() {
    sbox::Sandbox<SboxType> A("./libtestlib.so");
    sbox::Sandbox<SboxType> B("./libtestlib2.so");
    g_a = &A;
    g_b = &B;

    auto cb1 = A.register_callback<cb_first>();
    auto cb2 = A.register_callback<cb_second>();

    TEST("both callbacks fire");
    A.call<void(void (*)(), void (*)())>("invoke_two_void_callbacks", cb1, cb2);
    assert(cb1_calls == 1);
    assert(cb2_calls == 1);
    PASS();

    TEST("first callback receives the calling sandbox");
    assert(cb1_observed == g_a);
    PASS();

    TEST("second callback receives the calling sandbox after a cross-sandbox call");
    assert(cb2_observed == g_a);
    PASS();

    TEST_SUMMARY();
}
