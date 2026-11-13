// Should fail: the sandbox may store any byte in a bool.
#include "sbox/passthrough.h"

int main() {
    sbox::Sandbox<sbox::Passthrough> sandbox("./libfoo.so");
    auto b = sandbox.verify(sbox::sbox<bool*>(), 1);
    (void) b;
}
