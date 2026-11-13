// Should fail: LFI callbacks cannot take integer parameters narrower than 32
// bits.
#include "sbox/lfi.h"

static int lookup(unsigned char i) {
    return i;
}

int main() {
    auto sandbox = sbox::Sandbox<sbox::LFI>::create("./libfoo.lfi");
    auto cb = sandbox->register_callback(lookup);
    (void) cb;
}
