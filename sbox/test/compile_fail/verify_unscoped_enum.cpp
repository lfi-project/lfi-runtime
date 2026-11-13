// Should fail: enums in sandbox memory must be scoped.
#include "sbox/lfi.h"

enum Color { R, G, B };

int main() {
    auto sandbox = sbox::Sandbox<sbox::LFI>::create("./libfoo.lfi");
    auto c = sandbox->verify(sbox::sbox<Color*>(), 0);
    (void) c;
}
