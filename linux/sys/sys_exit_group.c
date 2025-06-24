#include "list.h"
#include "sys/sys.h"

#include <signal.h>
#include <unistd.h>

uintptr_t
sys_exit_group(struct LFILinuxThread *t, int code)
{
    {
        LOCK_WITH_DEFER(&t->proc->lk_threads, lk_threads);
        // Kill all child threads by sending them SIGLFI.
        struct List *e;
        for (e = list_first(t->proc->threads); e;) {
            struct LFILinuxThread *child = LIST_CONTAINER(struct LFILinuxThread,
                threads_elem, e);
            LOG(t->proc->engine, "killing thread %d", child->tid);
            // TODO: check if thread actually exited?
            pthread_kill(*child->pthread, SIGLFI);
            e = list_next(t->proc->threads, e);
        }
    }
    lfi_ctx_exit(t->ctx, code);
    assert(!"unreachable");
}
