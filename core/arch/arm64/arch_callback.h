#pragma once

#include <stdint.h>

#define MAXCALLBACKS 4096

// Callbacks on AArch64 are done differently than x86-64 because we don't have
// bundles. We use two callback regions, one for code and one for data. The
// code uses a pc-relative load instruction to load the trampoline and target
// from the callback data region and branch to it.

// Callback code region entry.
struct CallbackEntry {
    uint32_t code[4];
};

// Callback data region entry.
struct CallbackDataEntry {
    _Atomic(uint64_t) target;
    _Atomic(uint64_t) trampoline;
};

struct CallbackInfo {
    struct CallbackEntry *cbentries_alias;
    struct CallbackEntry *cbentries_box;
    struct CallbackDataEntry *dataentries_alias;
    struct CallbackDataEntry *dataentries_box;
};
