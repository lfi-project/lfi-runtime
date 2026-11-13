// Should fail: pointers in sandbox memory must be held as sbox<T*>.
#include "sbox/lfi.h"

int main() {
    auto sandbox = sbox::Sandbox<sbox::LFI>::create("./libfoo.lfi");
    auto p = sandbox->alloc<const char*>(1);
    (void) p;
}
