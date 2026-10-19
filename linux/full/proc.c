#include "full.h"
#include "lock.h"

#include <stdlib.h>

bool
full_proc_init(struct LFILinuxProc *proc)
{
    struct LFIFullProc *full = calloc(1, sizeof(*full));
    if (!full)
        return false;
    pthread_mutex_init(&full->lk_threads, NULL);
    pthread_cond_init(&full->cond_threads, NULL);
    pthread_mutex_init(&full->lk_clone, NULL);
    proc->full = full;
    return true;
}

void
full_proc_join_threads(struct LFILinuxProc *proc)
{
    struct LFIFullProc *full = proc->full;
    LOCK_WITH_DEFER(&full->lk_threads, lk_threads);
    while (full->active_threads != 0)
        pthread_cond_wait(&full->cond_threads, &full->lk_threads);
}

void
full_proc_free(struct LFILinuxProc *proc)
{
    struct LFIFullProc *full = proc->full;

    // The last detach can get here without going through lfi_proc_free's
    // wait.
    full_proc_join_threads(proc);

    // The thread behind the clone context never attaches to a host thread, so
    // the proc owns it.
    if (full->clone_ctx) {
        struct LFILinuxThread *clone_thread = lfi_ctx_data(full->clone_ctx);
        if (clone_thread)
            lfi_thread_free(clone_thread);
    }

    pthread_mutex_destroy(&full->lk_clone);
    pthread_cond_destroy(&full->cond_threads);
    pthread_mutex_destroy(&full->lk_threads);
    free(full);
    proc->full = NULL;
}
