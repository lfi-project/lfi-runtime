// Should fail: SBOX_FN cannot be combined with an explicit signature.
#include "sbox/passthrough.h"

extern "C" {
int read_int(int* p);
}

int main() {
    sbox::Sandbox<sbox::Passthrough> sandbox("./libfoo.so");
    int x = 0;
    sandbox.call<int(int*)>(SBOX_FN(read_int), &x);
}
