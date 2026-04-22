// Should fail: sbox_safe<double*> does not match int*
#include "sbox/passthrough.h"

extern "C" {
int sum_ints(int* arr, int count);
}

int main() {
    sbox::Sandbox<sbox::Passthrough> sandbox("./libfoo.so");
    auto arr = sandbox.alloc<double>(4);
    sandbox.call(SBOX_FN(sum_ints), arr, 4);
}
