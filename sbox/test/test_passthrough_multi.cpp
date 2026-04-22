#include "sbox/passthrough.h"
#include "test_helpers.h"

int main() {
    sbox::Sandbox<sbox::Passthrough> sandbox("./libtestlib.so");


    TEST("two sandboxes independently");
    {
        sbox::Sandbox<sbox::Passthrough> sandbox2("./libtestlib.so");
        int r1 = sandbox.call<int(int, int)>("add", 1, 2);
        int r2 = sandbox2.call<int(int, int)>("add", 3, 4);
        assert(r1 == 3);
        assert(r2 == 7);
    }
    PASS();

    TEST_SUMMARY();
}
