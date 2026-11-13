// Should fail: pointer out parameters must be held as sbox<T*>.
#include "sbox/passthrough.h"

extern "C" {
void get_version_ptr(const char** out);
}

int main() {
    sbox::Sandbox<sbox::Passthrough> sandbox("./libfoo.so");
    auto ctx = sandbox.context();
    const char* name = nullptr;
    sandbox.call(ctx, SBOX_FN(get_version_ptr), ctx.out(name));
}
