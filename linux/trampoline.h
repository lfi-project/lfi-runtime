#pragma once

#include "linux.h"

extern struct LFIContext *clone_ctx;

extern _Thread_local struct LFIContext *new_ctx;

void
lfi_linux_clone_cb(struct LFIBox *box, struct LFIContext **ctxp);
