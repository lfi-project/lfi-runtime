#include <cstdio>

#if defined(SBOX_LFI)
#include "sbox/lfi.h"
#else
#include "sbox/passthrough.h"
#endif

// Host callback function - will be called by sandbox code
static int my_adder(int a, int b) {
    printf("[HOST callback] adding %d + %d = %d\n", a, b, a + b);
    return a + b;
}

int main() {
#if defined(SBOX_LFI)
    auto sb = sbox::Sandbox<sbox::LFI>::create("./lib_callback.lfi");
    if (!sb) { fprintf(stderr, "failed to create sandbox\n"); return 1; }
    auto& sandbox = *sb;
#else
    sbox::Sandbox<sbox::Passthrough> sandbox("./libcallback.so");
#endif

    // Register callback
    auto add_fn = sandbox.register_callback(my_adder);
    printf("Registered callback\n");

    // Call sandbox function, passing callback
    using ProcessFn = int(int, int (*)(int, int));
    int result = sandbox.call<ProcessFn>("process_data", 42, add_fn);

    printf("process_data returned: %d\n", result);

    return 0;
}
