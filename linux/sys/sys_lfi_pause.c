#include "sys/sys.h"

uintptr_t
sys_lfi_pause(struct LFILinuxThread *t)
{
    lfi_ctx_exit(t->ctx, 0);
    assert(!"unreachable");
}
