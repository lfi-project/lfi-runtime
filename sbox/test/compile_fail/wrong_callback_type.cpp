// Should fail: sbox<double(*)(double,double)> does not match int(*)(int,int)
#include "sbox/passthrough.h"

extern "C" {
int apply_binary_callback(int (*cb)(int, int), int a, int b);
}

static double wrong_cb(double a, double b) { return a + b; }

int main() {
    sbox::Sandbox<sbox::Passthrough> sandbox("./libfoo.so");
    auto cb = sandbox.register_callback(wrong_cb);
    sandbox.call(SBOX_FN(apply_binary_callback), cb, 1, 2);
}
