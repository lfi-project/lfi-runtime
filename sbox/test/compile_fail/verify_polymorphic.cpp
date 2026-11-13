// Should fail: a polymorphic object in sandbox memory has a vtable pointer
// that the sandbox controls.
#include "sbox/lfi.h"

struct Shape {
    virtual int area() const = 0;
};

static int area(sbox::Sandbox<sbox::LFI>& sandbox, sbox::sbox<Shape*> s) {
    return (*sandbox.verify(s, 1)).area();
}

int main() {
    auto sandbox = sbox::Sandbox<sbox::LFI>::create("./libfoo.lfi");
    auto cb = sandbox->register_callback<area>();
    (void) cb;
}
