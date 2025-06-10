#include "sys/sys.h"

uintptr_t
sys_exit_group(struct LFILinuxThread *t, int code)
{
    // TODO: this only exits the current thread, whereas exit_group should
    // cause all threads to exit.
    lfi_ctx_exit(t->ctx, code);
    assert(!"unreachable");
}
