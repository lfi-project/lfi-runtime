// Should fail: the callback's Sandbox parameter is for another backend.
#include "sbox/lfi.h"
#include "sbox/passthrough.h"

static int cb(sbox::Sandbox<sbox::Passthrough>& sandbox, int value) {
    (void) sandbox;
    return value;
}

int main() {
    auto sandbox = sbox::Sandbox<sbox::LFI>::create("./libfoo.lfi");
    auto h = sandbox->register_callback<cb>();
    (void) h;
}
