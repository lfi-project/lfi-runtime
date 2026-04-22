// Should fail: raw function pointer not wrapped in sbox
#include "sbox/passthrough.h"

extern "C" {
int apply_binary_callback(int (*cb)(int, int), int a, int b);
}

static int my_add(int a, int b) { return a + b; }

int main() {
    sbox::Sandbox<sbox::Passthrough> sandbox("./libfoo.so");
    sandbox.call(SBOX_FN(apply_binary_callback), my_add, 1, 2);
}
