// Should fail: reference parameters are not supported.
#include "sbox/lfi.h"

extern "C" {
int read_ref(int& p);
}

int main() {
    auto sandbox = sbox::Sandbox<sbox::LFI>::create("./libfoo.lfi");
    int x = 1234;
    sandbox->call(SBOX_FN(read_ref), x);
}
