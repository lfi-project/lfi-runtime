// Should fail: sandbox bytes cannot be copied into a host pointer.
#include "sbox/lfi.h"

int main() {
    auto sandbox = sbox::Sandbox<sbox::LFI>::create("./libfoo.lfi");
    char* p = nullptr;
    sandbox->copy_from(&p, sbox::sbox<char*>(), sizeof(p));
}
