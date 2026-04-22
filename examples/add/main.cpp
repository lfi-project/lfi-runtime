#include <cstdio>

#if defined(SBOX_LFI)
#include "sbox/lfi.h"
#else
#include "sbox/passthrough.h"
#endif

#include "lib_add.h"

int main() {
#if defined(SBOX_LFI)
    auto sb = sbox::Sandbox<sbox::LFI>::create("./lib_add.lfi");
    if (!sb) { fprintf(stderr, "failed to create sandbox\n"); return 1; }
    auto& sandbox = *sb;
#else
    sbox::Sandbox<sbox::Passthrough> sandbox("./libadd.so");
#endif

    // Call add(2, 3)
    int result = sandbox.call(SBOX_FN(add), 2, 3);
    printf("add(2, 3) = %d\n", result);

    // Call add(100, 200)
    result = sandbox.call(SBOX_FN(add), 100, 200);
    printf("add(100, 200) = %d\n", result);

    // Use function handle for repeated calls
    auto add_fn = sandbox.fn(SBOX_FN(add));
    result = add_fn(1000, 2000);
    printf("add(1000, 2000) = %d\n", result);

    return 0;
}
