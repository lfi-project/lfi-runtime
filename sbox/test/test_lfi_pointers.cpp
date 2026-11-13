#include "sbox/lfi.h"
#include "test_helpers.h"

int main() {
    auto sb = sbox::Sandbox<sbox::LFI>::create("./testlib.lfi");
    assert(sb);
    auto& sandbox = *sb;
#include "test_pointers.inc.cpp"
#include "test_context.inc.cpp"

    TEST("context: out normalizes a bool written by the sandbox");
    {
        auto ctx = sandbox.context();
        bool b = false;
        sandbox.call<void(bool*, int)>(ctx, "write_bool_byte", ctx.out(b),
                                       200);
        unsigned char byte;
        std::memcpy(&byte, &b, 1);
        assert(byte == 1);
    }
    PASS();

    TEST_SUMMARY();
}
