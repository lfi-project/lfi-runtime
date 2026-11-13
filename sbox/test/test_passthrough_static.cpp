// Passthrough static mode, with the test library linked into the program.

#define SBOX_STATIC
#include "sbox/passthrough.h"
#include "test_helpers.h"

extern "C" {
int add(int a, int b);
}

int main() {
    sbox::Sandbox<sbox::Passthrough> sandbox;

    TEST("static call and function handle");
    assert(sandbox.call(SBOX_FN(add), 20, 22) == 42);
    assert(sandbox.call(SBOX_FN(negate), 5) == -5);
    assert(sandbox.fn(SBOX_FN(add))(1, 2) == 3);
    PASS();

    TEST("static context calls with out and inout");
    {
        auto ctx = sandbox.context();
        int a = 1, b = 2, v = 0;
        sandbox.call(ctx, SBOX_FN(swap_ints), ctx.inout(a), ctx.inout(b));
        sandbox.call(ctx, SBOX_FN(write_int), ctx.out(v), 7);
        assert(a == 2 && b == 1 && v == 7);
    }
    PASS();

    TEST("static pointer out parameter stays tainted");
    {
        auto ctx = sandbox.context();
        sbox::sbox<const char*> name;
        sandbox.call(ctx, SBOX_FN(get_version_ptr), ctx.out(name));
        assert(sandbox.read_string(name, 64) == "testlib 1.0");
    }
    PASS();

    TEST_SUMMARY();
}
