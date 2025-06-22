#include "trampoline.h"

// TODO: should these globals belong to the engine?

// The clone context is the context used for cloning new sandbox threads
// dynamically. To create a new thread, invoke thread_create via the clone_ctx.
// The sandbox will execute pthread_create inside thread_create, which will make
// a clone syscall, which will not create a new thread but will instead place
// the newly created context into new_ctx. The newly created context can then
// be retrieved from new_ctx by the caller.
struct LFIContext *clone_ctx;

// This is where sys_clone places new contexts that are created via clone.
_Thread_local struct LFIContext *new_ctx;

EXPORT void
lfi_linux_init_clone(struct LFILinuxThread *main)
{
    // Set clone_ctx to main's ctx to indicate that we are fake-cloning from
    // main.
    clone_ctx = main->ctx;
    // Invoke thread_create in main_thread.
    LFI_INVOKE(main->proc->box, &main->ctx, main->proc->fn_thread_create, void *, (void *), main->proc->fn_pause);
    // Store the resulting new_ctx in clone_ctx to use for future clones.
    clone_ctx = new_ctx;

    // TODO: register lfi_linux_clone_cb as the clone_cb
}

void
lfi_linux_clone_cb(struct LFIBox *box, struct LFIContext **ctxp)
{
    // Invoke thread_create in clone_ctx and store the resulting new_ctx in ctxp.
    LFI_INVOKE(box, &clone_ctx, lookup(fn_thread_create), void *, (void *), lookup(fn_pause));
    *ctxp = new_ctx;
}
