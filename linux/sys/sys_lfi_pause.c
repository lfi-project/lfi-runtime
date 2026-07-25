#include "sys/sys.h"

#include <stdlib.h>

uintptr_t
sys_lfi_pause(struct LFILinuxThread *t)
{
#ifdef SYS_MINIMAL
    if (t->owner != LFI_THREAD_MAIN) {
        ERROR("sandbox tried to pause a context that was never entered to run");
        abort();
    }
#endif

    // Pausing hands this context back to whoever entered it with lfi_ctx_run.
    // From then on it is entered through the trampoline.
    if (t->paused) {
        ERROR("sandbox paused a context that is already paused");
        abort();
    }
    t->paused = true;

    lfi_ctx_exit(t->ctx, 0);
    __builtin_unreachable();
}
