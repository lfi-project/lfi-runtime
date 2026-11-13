// Should fail: raw int* not allowed through a function handle.
#include "sbox/passthrough.h"

int main() {
    sbox::Sandbox<sbox::Passthrough> sb("./libfoo.so");
    auto* sandbox = &sb;
    int x = 0;
    auto read = sandbox->fn<int(int*)>("read_int");
    read(&x);
}
