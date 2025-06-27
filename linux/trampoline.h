#pragma once

#include "linux.h"

// Symbols defined by every library, and their index in the library's
// registered function table.
#define LFI_SYM_thread_create  0
#define LFI_SYM_thread_destroy 1
#define LFI_SYM_malloc         2
#define LFI_SYM_realloc        3
#define LFI_SYM_calloc         4
#define LFI_SYM_free           5
// Remaining symbols are library-dependent.

extern struct LFIContext *clone_ctx;

extern _Thread_local struct LFIContext *new_ctx;

struct LFIContext *
lfi_linux_clone_cb(struct LFIBox *box);
