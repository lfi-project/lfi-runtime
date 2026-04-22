// Should fail: raw int* not allowed, must use sbox<int*> or sbox_safe<int*>
#include "sbox/passthrough.h"

extern "C" {
int sum_ints(int* arr, int count);
}

int main() {
    sbox::Sandbox<sbox::Passthrough> sandbox("./libfoo.so");
    int arr[4] = {1, 2, 3, 4};
    sandbox.call(SBOX_FN(sum_ints), arr, 4);
}
