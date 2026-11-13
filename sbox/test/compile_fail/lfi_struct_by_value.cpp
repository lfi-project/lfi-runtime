// Should fail: the LFI backend does not pass structs by value.
#include "sbox/lfi.h"

struct Pair {
    double a;
    double b;
};

int main() {
    auto sandbox = sbox::Sandbox<sbox::LFI>::create("./libfoo.lfi");
    sandbox->call<double(Pair)>("pair_sum", Pair{1.0, 2.0});
}
