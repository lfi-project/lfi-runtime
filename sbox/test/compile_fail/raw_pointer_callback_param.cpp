// Should fail: callback pointer parameters must be sbox<T*>.
#include "sbox/passthrough.h"

static int sum(int* data, int count) {
    return data[0] + count;
}

int main() {
    sbox::Sandbox<sbox::Passthrough> sandbox("./libfoo.so");
    auto cb = sandbox.register_callback(sum);
    (void) cb;
}
