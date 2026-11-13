// Should fail: sbox_safe values cannot be stored in sandbox memory.
#include "sbox/lfi.h"

int main() {
    auto sandbox = sbox::Sandbox<sbox::LFI>::create("./libfoo.lfi");
    auto p = sandbox->alloc<sbox::sbox_safe<int*>>(4);
    (void) p;
}
