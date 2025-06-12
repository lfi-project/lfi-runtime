#pragma once

#include <stdint.h>

#define MAXCALLBACKS 4096

// A CallbackEntry is what gets inserted into the sandbox when a new callback
// is registered. It consists of 16 bytes of code, which performs the callback
// transition to the host, along with the target function in the host to
// transfer to and the address of the trampoline. It must be 32 bytes to fit
// into a single bundle.
struct CallbackEntry {
    uint8_t code[16];
    _Atomic(uint64_t) target;
    _Atomic(uint64_t) trampoline;
};

_Static_assert(sizeof(struct CallbackEntry) == 32, "invalid callback entry size");
